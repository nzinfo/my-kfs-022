//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: ClientSM.cc 146 2008-09-06 19:48:49Z sriramsrao $
//
// Created 2006/03/23
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

#include "ClientSM.h"

#include "ChunkManager.h"
#include "ChunkServer.h"
#include "Utils.h"
#include "KfsOps.h"

#include <string>
#include <sstream>
using std::string;
using std::ostringstream;

#include "common/log.h"
#include "libkfsIO/Globals.h"

#include <boost/scoped_array.hpp>
using boost::scoped_array;

using namespace KFS;
using namespace KFS::libkfsio;

/// 使用指定的NetConnection来构造clientSM
ClientSM::ClientSM(NetConnectionPtr &conn)
{
	mNetConnection = conn;
	SET_HANDLER(this, &ClientSM::HandleRequest);
}

ClientSM::~ClientSM()
{
	KfsOp *op;

	assert(mOps.empty());
	while (!mOps.empty())
	{
		op = mOps.front();
		mOps.pop_front();
		delete op;
	}
	gClientManager.Remove(this);
}

///
/// Send out the response to the client request.  The response is
/// generated by MetaRequest as per the protocol.
/// @param[in] op The request for which we finished execution.
///
/// 处理回复
/// @note 如果是读请求的回复，则需要将读入的数据添加到操作的末尾
void ClientSM::SendResponse(KfsOp *op)
{
	ostringstream os;
	ReadOp *rop;
	string s = op->Show();
	int len;

#ifdef DEBUG
	verifyExecutingOnNetProcessor();
#endif
	// 调用执行回复，将回复信息写入到os中
	op->Response(os);

	KFS_LOG_VA_DEBUG("Command %s: Response status: %d\n",
			s.c_str(), op->status);

	len = os.str().length();
	// 通过mNetConnection向client发出回复
	if (len > 0)
		mNetConnection->Write(os.str().c_str(), len);

	// 如果是读操作，则需要返回读入的数据
	if (op->op == CMD_READ)
	{
		// 把读入的数据添加到操作Response的末尾
		// need to send out the data read
		rop = static_cast<ReadOp *> (op);
		if (op->status >= 0)
		{
			KFS_LOG_VA_DEBUG("Bytes avail from read: %d\n",
					rop->dataBuf->BytesConsumable());
			assert(rop->dataBuf->BytesConsumable() == rop->status);
			mNetConnection->Write(rop->dataBuf, rop->numBytesIO);
		}
	}
	// 如果操作是获取chunk server的全局数据，则需要把读入的数据添加到操作Response的末尾
	else if (op->op == CMD_GET_CHUNK_METADATA)
	{
		GetChunkMetadataOp *gcm = static_cast<GetChunkMetadataOp *> (op);
		if (op->status >= 0)
			mNetConnection->Write(gcm->dataBuf, gcm->numBytesIO);
	}
}

///
/// Generic event handler.  Decode the event that occurred and
/// appropriately extract out the data and deal with the event.
/// @param[in] code: The type of event that occurred
/// @param[in] data: Data being passed in relative to the event that
/// occurred.
/// @retval 0 to indicate successful event handling; -1 otherwise.
/// 处理收到的client的回复信息，包括：网络读，网络写，完成和错误
/// void *data可以强制转换为KfsOp *data
int ClientSM::HandleRequest(int code, void *data)
{
	IOBuffer *iobuf;
	KfsOp *op;
	int cmdLen;

#ifdef DEBUG
	verifyExecutingOnNetProcessor();
#endif

	switch (code)
	{
	/// 我们发送的命令是读命令，则需要分析并执行用户的RPC
	case EVENT_NET_READ:
		// We read something from the network.  Run the RPC that
		// came in.
		iobuf = (IOBuffer *) data;
		while (IsMsgAvail(iobuf, &cmdLen))
		{
			// if we don't have all the data for the command, wait
			if (!HandleClientCmd(iobuf, cmdLen))
				break;
		}
		break;

	/// 如果我们发送的命令是写命令，则不需要做任何处理
	case EVENT_NET_WROTE:
		// Something went out on the network.  For now, we don't
		// track it. Later, we may use it for tracking throttling
		// and such.
		break;

	/// 如果我们发送的是命令完成的通知操作，
	case EVENT_CMD_DONE:
		// An op finished execution.  Send response back in FIFO
		// 更改gChunkServer中op计数器的数目
		gChunkServer.OpFinished();

		op = (KfsOp *) data;
		op->done = true;
		assert(!mOps.empty());
		// 清除mOps中前几个已经完成的操作
		while (!mOps.empty())
		{
			KfsOp *qop = mOps.front();

			// 发现队列中第一个未完成的操作时，终止操作
			if (!qop->done)
				break;
			SendResponse(qop);
			mOps.pop_front();
			OpFinished(qop);
			delete qop;
		}

		// 将netConnection缓存中的数据写入到网络socket中
		mNetConnection->StartFlush();
		break;

	case EVENT_NET_ERROR:
		// KFS_LOG_VA_DEBUG("Closing connection");

		if (mNetConnection)
			mNetConnection->Close();

		// wait for the ops to finish
		SET_HANDLER(this, &ClientSM::HandleTerminate);
		// 如果出现网络读写错误，则等待所有的操作完成后删除clientSM程序
		// 相当于执行了HandleTerminate()
		if (mOps.empty())
		{
			delete this;
		}

		break;

	default:
		assert(!"Unknown event");
		break;
	}
	return 0;
}

///
/// Termination handler.  For the client state machine, we could have
/// ops queued at the logger.  So, for cleanup wait for all the
/// outstanding ops to finish and then delete this.  In this state,
/// the only event that gets raised is that an op finished; anything
/// else is bad.
/// 尝试删除mOps队列中所有的操作请求，完成所有的操作之后删除自身，即进行终止操作
int ClientSM::HandleTerminate(int code, void *data)
{
	KfsOp *op;

#ifdef DEBUG
	verifyExecutingOnNetProcessor();
#endif

	switch (code)
	{
	// 如果是命令完成操作，则更新gChunkServer中op的计数值，并删除mOps队列首已经完成的操作
	case EVENT_CMD_DONE:
		gChunkServer.OpFinished();
		// An op finished execution.  Send a response back
		op = (KfsOp *) data;
		op->done = true;
		if (op != mOps.front())
			break;
		while (!mOps.empty())
		{
			op = mOps.front();
			if (!op->done)
				break;
			OpFinished(op);
			// we are done with the op
			mOps.pop_front();
			delete op;
		}
		break;
	default:
		assert(!"Unknown event");
		break;
	}

	// 如果所有的操作都已经完成了，那么就删除自身
	if (mOps.empty())
	{
		// all ops are done...so, now, we can nuke ourself.
		assert(mPendingOps.empty());
		delete this;
	}
	return 0;
}

///
/// We have a command in a buffer.  It is possible that we don't have
/// everything we need to execute it (for example, for a write we may
/// not have received all the data the client promised).  So, parse
/// out the command and if we have everything execute it.
/// 执行iobuf中的cmd命令（如果这个命令已经接受完整）
/// @note 如果操作为CMD_WRITE_SYNC，则需要建立含依赖关系的op操作
bool ClientSM::HandleClientCmd(IOBuffer *iobuf, int cmdLen)
{
	scoped_array<char> buf;
	KfsOp *op;
	size_t nAvail;

	buf.reset(new char[cmdLen + 1]);
	iobuf->CopyOut(buf.get(), cmdLen);
	buf[cmdLen] = '\0';

	// 解析iobuf中的命令，存放在buf中
	if (ParseCommand(buf.get(), cmdLen, &op) != 0)
	{
		// 如果解析出现错误，则清除iobuf中对应的数据（此时我们认为这个cmd是错误的）
		iobuf->Consume(cmdLen);

		KFS_LOG_VA_DEBUG("Aye?: %s", buf.get());
		// got a bogus command
		return true;
	}

	/*
	 * 需要处理的命令可能是：预备写（write prepare），写同步（write sync）
	 */
	if (op->op == CMD_WRITE_PREPARE)
	{ // WRITE_PREPARE命令，需要读入之后的数据
		WritePrepareOp *wop = static_cast<WritePrepareOp *> (op);

		assert(wop != NULL);

		// if we don't have all the data for the write, hold on...
		nAvail = iobuf->BytesConsumable() - cmdLen;
		if (nAvail < wop->numBytes)
		{// 如果要写的数据尚未完全准备好，则删除该操作（我们认为命令传输不完整）
			delete op;
			// we couldn't process the command...so, wait
			return false;
		}
		iobuf->Consume(cmdLen);
		wop->dataBuf = new IOBuffer();

		// 将iobuf中的数据剪切到dataBuf中
		wop->dataBuf->Move(iobuf, wop->numBytes);
		nAvail = wop->dataBuf->BytesConsumable();
		// KFS_LOG_VA_DEBUG("Got command: %s", buf.get());

		// KFS_LOG_VA_DEBUG("# of bytes avail for write: %lu", nAvail);
	}
	else // WRITE_PREPARE以外的所有操作
	{
		string s = op->Show();

		KFS_LOG_VA_DEBUG("Got command: %s\n", s.c_str());

		iobuf->Consume(cmdLen);
	}

	// 如果该命令是写同步命令
	if (op->op == CMD_WRITE_SYNC)
	{
		// make the write sync depend on a previous write
		KfsOp *w = NULL;

		// 查找是否有预备写(prepared write)，预备写（转发）(prepared forwarding
		// write)或者写操作(write)
		for (deque<KfsOp *>::iterator i = mOps.begin(); i != mOps.end(); i++)
		{
			if (((*i)->op == CMD_WRITE_PREPARE) || ((*i)->op
					== CMD_WRITE_PREPARE_FWD) || ((*i)->op == CMD_WRITE))
			{
				w = *i;
			}
		}
		if (w != NULL)
		{
			// 添加依赖操作关系，等待指定的操作完成后执行
			OpPair p;

			op->clnt = this;
			p.op = w;
			p.dependentOp = op;
			mPendingOps.push_back(p);

			KFS_LOG_VA_DEBUG("Keeping write-sync (%d) pending and depends on %d",
					op->seq, p.op->seq);

			return true;
		}
		else
		{	// 如果没有需要同步的写操作
			KFS_LOG_VA_DEBUG("Write-sync is being pushed down; no writes left... (%d ops left in q)",
					mOps.size());
		}
	}

	// 将通过参数传输进来的操作加入到FIFO队列中
	mOps.push_back(op);

	op->clnt = this;

	// 更新操作计数器（+1）
	gChunkServer.OpInserted();

	// 执行操作
	// op->Execute();
	SubmitOp(op);

	return true;
}

/// 完成指定的含依赖性的操作：
/// @note 可能有多个操作都在等待一个操作的完成
/// @note 指定的操作必须是等待完成的依赖操作的第一个；如果不是，则不进处理
void ClientSM::OpFinished(KfsOp *doneOp)
{
	// multiple ops could be waiting for a single op to finish...
	while (1)
	{
		// 如果没有等待执行的含依赖关系的操作操作，则直接返回即可
		if (mPendingOps.empty())
			return;

		OpPair p;
		p = mPendingOps.front();

		if (p.op->seq != doneOp->seq)
		{
			break;
		}

		KFS_LOG_VA_DEBUG("Submitting write-sync (%d) since %d finished", p.dependentOp->seq,
				p.op->seq);
		mOps.push_back(p.dependentOp);
		gChunkServer.OpInserted();
		mPendingOps.pop_front();
		SubmitOp(p.dependentOp);
	}
}
