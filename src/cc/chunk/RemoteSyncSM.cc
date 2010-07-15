//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: RemoteSyncSM.cc 122 2008-08-11 17:41:09Z sriramsrao $
//
// Created 2006/09/27
// Author: Sriram Rao
//
// Copyright 2008 Quantcast Corp.
// Copyright 2006-2008 Kosmix Corp.
//
// This file is part of Kosmos File System (KFS).
//
// Licensed under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.
//
//
//----------------------------------------------------------------------------

#include "RemoteSyncSM.h"
#include "Utils.h"
#include "ChunkServer.h"
#include "libkfsIO/NetManager.h"
#include "libkfsIO/Globals.h"

#include "common/log.h"
#include "common/properties.h"

#include <cerrno>
#include <sstream>
#include <algorithm>
using std::find_if;
using std::for_each;
using std::istringstream;
using std::ostringstream;
using std::list;

using namespace KFS;
using namespace KFS::libkfsio;

#include <boost/scoped_array.hpp>
using boost::scoped_array;

RemoteSyncSM::~RemoteSyncSM()
{
	if (mTimer)
		globals().netManager.UnRegisterTimeoutHandler(mTimer);
	delete mTimer;

	if (mNetConnection)
		mNetConnection->Close();
	assert(mDispatchedOps.size() == 0);
	assert(mPendingOps.empty());
}

// 创建同mLocation指定的服务器的连接；如果成功，则返回true
bool RemoteSyncSM::Connect()
{
	TcpSocket *sock;
	int res;

	KFS_LOG_VA_DEBUG("Trying to connect to: %s", mLocation.ToString().c_str());

	// 创建socket并建立到Location指定server的连接
	sock = new TcpSocket();
	// do a non-blocking connect
	res = sock->Connect(mLocation, true);
	if ((res < 0) && (res != -EINPROGRESS))
	{
		KFS_LOG_VA_INFO("Connect to remote server (%s) failed: code = %d",
				mLocation.ToString().c_str(), res);
		delete sock;
		return false;
	}

	// 创建用于该类的timeout程序
	if (mTimer == NULL)
	{
		mTimer = new RemoteSyncSMTimeoutImpl(this);
		globals().netManager.RegisterTimeoutHandler(mTimer);
	}

	KFS_LOG_VA_INFO("Connect to remote server (%s) succeeded...",
			mLocation.ToString().c_str());

	SET_HANDLER(this, &RemoteSyncSM::HandleEvent);

	mNetConnection.reset(new NetConnection(sock, this));
	mNetConnection->SetDoingNonblockingConnect();
	// Add this to the poll vector
	globals().netManager.AddConnection(mNetConnection);

	return true;
}

/// 将op加入到mDispatchedOps队列中等待处理，如果这个op不是转发操作；否则，直接回复给client
/// 并同步mNetConnection中的数据到socket
void RemoteSyncSM::Enqueue(KfsOp *op)
{
	ostringstream os;

	// 如果没有建立起netConnection，则尝试建立连接；若无法建立连接，则所有的操作均做出错处理
	if (!mNetConnection)
	{
		if (!Connect())
		{
			mDispatchedOps.push_back(op);
			FailAllOps();
			return;
		}
	}

	op->Request(os);
	// 将os中的数据输出到端口
	mNetConnection->Write(os.str().c_str(), os.str().length());

	// 如果op的操作是转发，则直接将数据发送到mNetConnection指定的位置，并提交下响应
	if (op->op == CMD_WRITE_PREPARE_FWD)
	{
		// send the data as well
		WritePrepareFwdOp *wpfo = static_cast<WritePrepareFwdOp *> (op);
		mNetConnection->Write(wpfo->dataBuf, wpfo->dataBuf->BytesConsumable());
		// fire'n'forget
		op->status = 0;
		KFS::SubmitOpResponse(op);
	}
	else
		mDispatchedOps.push_back(op);

	// 将数据实际发送到网络中
	mNetConnection->StartFlush();
}

#if 0
void
RemoteSyncSM::Enqueue(KfsOp *op)
{
	mPendingOps.enqueue(op);
	// gChunkServer.ToggleNetThreadKicking(true);
	globals().netKicker.Kick();
}

void
RemoteSyncSM::Dispatch()
{
	KfsOp *op;

	while ((op = mPendingOps.dequeue_nowait()) != NULL)
	{
		ostringstream os;

		if (!mNetConnection)
		{
			if (!Connect())
			{
				mDispatchedOps.push_back(op);
				FailAllOps();
				return;
			}
		}

		op->Request(os);
		mNetConnection->Write(os.str().c_str(), os.str().length());
		if (op->op == CMD_WRITE_PREPARE_FWD)
		{
			// send the data as well
			WritePrepareFwdOp *wpfo = static_cast<WritePrepareFwdOp *>(op);
			mNetConnection->Write(wpfo->dataBuf, wpfo->dataBuf->BytesConsumable());
			// fire'n'forget
			op->status = 0;
			KFS::SubmitOpResponse(op);
		}
		else
		mDispatchedOps.push_back(op);
	}
}
#endif
// 处理系统收到的回复消息
int RemoteSyncSM::HandleEvent(int code, void *data)
{
	IOBuffer *iobuf;
	int msgLen, res;
	// take a ref to prevent the object from being deleted
	// while we are still in this function.
	RemoteSyncSMPtr self = shared_from_this();

#ifdef DEBUG
	verifyExecutingOnNetProcessor();
#endif

	switch (code)
	{
	case EVENT_NET_READ:
		// We read something from the network.  Run the RPC that
		// came in if we got all the data for the RPC
		iobuf = (IOBuffer *) data;

		while (IsMsgAvail(iobuf, &msgLen))
		{
			// 处理系统收到的回复
			res = HandleResponse(iobuf, msgLen);
			if (res < 0)
				// maybe the response isn't fully available
				break;
		}
		break;

	case EVENT_NET_WROTE:
		// Something went out on the network.  For now, we don't
		// track it. Later, we may use it for tracking throttling
		// and such.
		break;

	// 网络错误
	case EVENT_NET_ERROR:
		KFS_LOG_DEBUG("Closing connection")
		;

		if (mNetConnection)
			mNetConnection->Close();

		globals().netManager.UnRegisterTimeoutHandler(mTimer);
		delete mTimer;
		mTimer = NULL;

		// fail all the ops
		FailAllOps();

		//  submit this as an op thru the event processor to remove
		//  this one....when we remove the server, we fail any other
		//  ops at that point
		KFS::SubmitOp(&mKillRemoteSyncOp);

		break;

	default:
		assert(!"Unknown event");
		break;
	}
	return 0;

}

// 处理收到的回复的信息
int RemoteSyncSM::HandleResponse(IOBuffer *iobuf, int msgLen)
{
	const char separator = ':';
	scoped_array<char> buf(new char[msgLen + 1]);
	Properties prop;
	kfsSeq_t seqNum;
	int status;
	list<KfsOp *>::iterator i;
	size_t numBytes;
	int64_t nAvail;

	// 将buf中的数据拷出来，作为命令，并添加结束符
	iobuf->CopyOut(buf.get(), msgLen);
	buf[msgLen] = '\0';

	istringstream ist(buf.get());

	// 从iobuf的信息中解析各个属性
	prop.loadProperties(ist, separator, false);
	seqNum = prop.getValue("Cseq", (kfsSeq_t) -1);
	status = prop.getValue("Status", -1);
	numBytes = prop.getValue("Content-length", (long long) 0);

	// if we don't have all the data for the write, hold on...
	/// 如果我们还没有读完所有的数据，就退出等待
	nAvail = iobuf->BytesConsumable() - msgLen;
	if (nAvail < (int64_t) numBytes)
	{
		// the data isn't here yet...wait...
		return -1;
	}

	// 这时已经读入了所有的数据
	iobuf->Consume(msgLen);

	// find the matching op
	// 从mDispatchedOps中查找seqNum相同的操作
	i = find_if(mDispatchedOps.begin(), mDispatchedOps.end(), OpMatcher(seqNum));
	if (i != mDispatchedOps.end())
	{	// 找到了该操作
		KfsOp *op = *i;
		op->status = status;
		mDispatchedOps.erase(i);
		if (op->op == CMD_WRITE_ID_ALLOC)
		{	// 如果是写操作，则只需要给出Write-id即可处理回复
			WriteIdAllocOp *wiao = static_cast<WriteIdAllocOp *> (op);

			wiao->writeIdStr = prop.getValue("Write-id", "");
		}
		else if (op->op == CMD_READ)
		{	// 如果是读操作，还需要给出已经读入的内容才能进行回复的处理
			ReadOp *rop = static_cast<ReadOp *> (op);
			if (rop->dataBuf == NULL)
				rop->dataBuf = new IOBuffer();
			rop->dataBuf->Move(iobuf, numBytes);
		}
		else if (op->op == CMD_SIZE)
		{	// 如果是查询大小的操作，指定size即可处理回复
			SizeOp *sop = static_cast<SizeOp *> (op);
			sop->size = prop.getValue("Size", 0);
		}
		else if (op->op == CMD_GET_CHUNK_METADATA)
		{	// 需要获取chunk的所有全局数据
			GetChunkMetadataOp *gcm = static_cast<GetChunkMetadataOp *> (op);
			gcm->chunkVersion = prop.getValue("Chunk-version", 0);
			gcm->chunkSize = prop.getValue("Size", 0);
			if (gcm->dataBuf == NULL)
				gcm->dataBuf = new IOBuffer();
			gcm->dataBuf->Move(iobuf, numBytes);
		}
		// op->HandleEvent(EVENT_DONE, op);
		// 提交回复处理的请求
		KFS::SubmitOpResponse(op);
	}
	else
	{
		KFS_LOG_VA_DEBUG("Discarding a reply for unknown seq #: %d", seqNum);
	}
	return 0;
}

// Helper functor that fails an op with an error code.
/// 判断指定的操作状态是否为错误
class OpFailer
{
	int errCode;
public:
	OpFailer(int c) :
		errCode(c)
	{
	}
	;
	void operator()(KfsOp *op)
	{
		op->status = errCode;
		// op->HandleEvent(EVENT_DONE, op);
		KFS::SubmitOpResponse(op);
	}
};

/// 将所有的Op（包括队列中的和pending的）状态设置为error，并通知给client
void RemoteSyncSM::FailAllOps()
{
	KfsOp *op;
	// get rid of the pending ones as well
	while ((op = mPendingOps.dequeue_nowait()) != NULL)
	{
		mDispatchedOps.push_back(op);
	}

	for_each(mDispatchedOps.begin(), mDispatchedOps.end(), OpFailer(
			-EHOSTUNREACH));
	mDispatchedOps.clear();
}

/// 关闭本chunk server，撤销其中的所有操作，并从gChunkServer移除该chunkserver
void RemoteSyncSM::Finish()
{
#ifdef DEBUG
	verifyExecutingOnEventProcessor();
#endif

	FailAllOps();
	gChunkServer.RemoveServer(this);
}
