//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: MetaServerSM.cc 152 2008-09-17 19:03:51Z sriramsrao $
//
// Created 2006/06/07
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
// \file MetaServerSM.cc
// \brief Handle interactions with the meta server.
//
//----------------------------------------------------------------------------

#include <unistd.h>
#include "common/log.h"
#include "MetaServerSM.h"
#include "ChunkManager.h"
#include "ChunkServer.h"
#include "Utils.h"

#include "libkfsIO/NetManager.h"
#include "libkfsIO/Globals.h"

#include <arpa/inet.h>
#include <netdb.h>

#include <algorithm>
#include <sstream>
using std::ostringstream;
using std::istringstream;
using std::find_if;
using std::list;

using namespace KFS;
using namespace KFS::libkfsio;

#include <boost/scoped_array.hpp>
using boost::scoped_array;

/// 用于chunkserver的MetaServerSM
MetaServerSM KFS::gMetaServerSM;

/// 将HandleRequest加入到this中，作为回调函数的执行体
MetaServerSM::MetaServerSM() :
	mCmdSeq(1), mRackId(-1), mSentHello(false), mHelloOp(NULL), mTimer(NULL)
{
	SET_HANDLER(this, &MetaServerSM::HandleRequest);
}

MetaServerSM::~MetaServerSM()
{
	if (mTimer)
		globals().netManager.UnRegisterTimeoutHandler(mTimer);
	delete mTimer;
	delete mHelloOp;
}

/// 设置本地有关metaserver的一些信息
void MetaServerSM::SetMetaInfo(const ServerLocation &metaLoc,
		const char *clusterKey, int rackId)
{
	mLocation = metaLoc;
	mClusterKey = clusterKey;
	mRackId = rackId;
}

/// 初始化：注册一个Timout类，指定client要访问本chunkServer时应该使用的端口
void MetaServerSM::Init(int chunkServerPort)
{
	if (mTimer == NULL)
	{
		mTimer = new MetaServerSMTimeoutImpl(this);
		globals().netManager.RegisterTimeoutHandler(mTimer);
	}
	mChunkServerPort = chunkServerPort;
}

/// timeout处理程序，主要操作包括： 1. 查看同metaserver的连接是否正常; 2. 发送服务器请求;
/// 3. 发送对服务器的回复
void MetaServerSM::Timeout()
{
	// 如果向metaserver的连接未建立或者已经断开，则需要重新建立请求（发送Hello），并重新
	// 提交mDispatcherOps中所有的操作
	if (!mNetConnection)
	{
		KFS_LOG_WARN("Connection to meta broke. Reconnecting...");
		if (Connect() < 0)
		{
			return;
		}

		// 发送问好信息
		SendHello();
		// 重新提交所有的操作
		ResubmitOps();
	}

	// 向服务器发送mPendingOps队列中的操作请求
	DispatchOps();
	// 向服务器发送mPendingResponse队列中的回复操作
	DispatchResponse();
}

/// 创建同metaserver的socket连接
int MetaServerSM::Connect()
{
	TcpSocket *sock;

	// 如果timeout为空，则创建一个timeout，注册到本类中
	if (mTimer == NULL)
	{
		mTimer = new MetaServerSMTimeoutImpl(this);
		globals().netManager.RegisterTimeoutHandler(mTimer);
	}

	KFS_LOG_VA_DEBUG("Trying to connect to: %s:%d",
			mLocation.hostname.c_str(), mLocation.port);

	/// 创建连接，并尝试连接服务器
	sock = new TcpSocket();
	if (sock->Connect(mLocation) < 0)
	{
		// KFS_LOG_DEBUG("Reconnect failed...");
		delete sock;
		return -1;
	}
	KFS_LOG_VA_INFO("Connect to metaserver (%s) succeeded...",
			mLocation.ToString().c_str());

	mNetConnection.reset(new NetConnection(sock, this));

	// when the system is overloaded, we still want to add this
	// connection to the poll vector for reads; this ensures that we
	// get the heartbeats and other RPCs from the metaserver
	// 为保证心跳程序的正常运行和对于metaserver的命令的及时响应，即使超载也要进行数据传输
	mNetConnection->EnableReadIfOverloaded();

	// Add this to the poll vector
	globals().netManager.AddConnection(mNetConnection);

	// time to resend all the ops queued?

	return 0;
}

/// 发出问好信息：向系统提交一个HelloMetaOp
int MetaServerSM::SendHello()
{
	char hostname[256];

	// 如果已经发送了Hello信息，直接退出即可
	if (mHelloOp != NULL)
		return 0;

#ifdef DEBUG
	verifyExecutingOnNetProcessor();
#endif

	// 如果对metaserver的连接尚未建立，则尝试建立连接
	if (!mNetConnection)
	{
		// 尝试建立对于metaserver的连接
		if (Connect() < 0)
		{
			KFS_LOG_DEBUG("Unable to connect to meta server");
			return -1;
		}
	}

	// 获取本地系统的hostname
	gethostname(hostname, 256);

	// switch to IP address so we can avoid repeated DNS lookups
	struct hostent *hent = gethostbyname(hostname);
	in_addr ipaddr;

	memcpy(&ipaddr, hent->h_addr, hent->h_length);

	// 本地的主机名和端口
	ServerLocation loc(inet_ntoa(ipaddr), mChunkServerPort);
	mHelloOp = new HelloMetaOp(nextSeq(), loc, mClusterKey, mRackId);
	mHelloOp->clnt = this;
	// send the op and wait for it comeback
	KFS::SubmitOp(mHelloOp);
	return 0;
}

/// 向metaserver发送Hello
void MetaServerSM::DispatchHello()
{
	ostringstream os;

#ifdef DEBUG
	verifyExecutingOnNetProcessor();
#endif

	if (!mNetConnection)
	{
		if (Connect() < 0)
		{// 如果建立连接失败，则退出程序，置mHelloOp为空，等待下一次操作请求
			// don't have a connection...so, need to start the process again...
			delete mHelloOp;
			mHelloOp = NULL;
			return;
		}
	}

	// 向服务器发送Hello请求
	mHelloOp->Request(os);
	mNetConnection->Write(os.str().c_str(), os.str().length());

	mSentHello = true;

	KFS_LOG_VA_INFO("Sent hello to meta server: %s", mHelloOp->Show().c_str());

	// 发送完成之后，就可以清除mHelloOp了？？不接受回复确认？？
	delete mHelloOp;
	mHelloOp = NULL;
}

#if 0
int
MetaServerSM::SendHello()
{
	ostringstream os;
	char hostname[256];

#ifdef DEBUG
	verifyExecutingOnNetProcessor();
#endif

	mChunkServerPort = chunkServerPort;

	if (!mNetConnection)
	{
		if (Connect() < 0)
		{
			KFS_LOG_DEBUG("Unable to connect to meta server");
			return -1;
		}
	}
	gethostname(hostname, 256);

	ServerLocation loc(hostname, chunkServerPort);
	HelloMetaOp op(nextSeq(), loc, mClusterKey);

	op.totalSpace = gChunkManager.GetTotalSpace();
	op.usedSpace = gChunkManager.GetUsedSpace();
	// XXX: For thread safety, force the request thru the event
	// processor to get this info.
	gChunkManager.GetHostedChunks(op.chunks);

	op.Request(os);
	mNetConnection->Write(os.str().c_str(), os.str().length());

	mSentHello = true;

	KFS_LOG_VA_INFO("Sent hello to meta server: %s", op.Show().c_str());

	return 0;
}
#endif

///
/// Generic event handler.  Decode the event that occurred and
/// appropriately extract out the data and deal with the event.
/// @param[in] code: The type of event that occurred
/// @param[in] data: Data being passed in relative to the event that
/// occurred.
/// @retval 0 to indicate successful event handling; -1 otherwise.
///
/// 1 解析命令；2 解析出数据；3 处理事件
/// data中存放的是对操作的回复
/// @para[in] code 给定操作的类型
/// @para[in] data 给定操作的内容
int MetaServerSM::HandleRequest(int code, void *data)
{
	IOBuffer *iobuf;
	KfsOp *op;
	int cmdLen;

#ifdef DEBUG
	verifyExecutingOnNetProcessor();
#endif

	switch (code)
	{
	case EVENT_NET_READ:
		// We read something from the network.  Run the RPC that
		// came in.
		/// 数据格式：header + '\r\n\r\n' + data
		iobuf = (IOBuffer *) data;

		// 判断是否有一个完整的部分：以'\r\n\r\n'结尾的数据
		while (IsMsgAvail(iobuf, &cmdLen))
		{
			// if we don't have all the data for the command, bail
			if (!HandleMsg(iobuf, cmdLen))
				break;
		}
		break;

	case EVENT_NET_WROTE:
		// Something went out on the network.  For now, we don't
		// track it. Later, we may use it for tracking throttling
		// and such.
		break;

	case EVENT_CMD_DONE:
		// An op finished execution.  Send a response back
		op = (KfsOp *) data;
		if (op->op == CMD_META_HELLO)
		{
			DispatchHello();
			break;
		}

		// the op will be deleted after we send the response.
		EnqueueResponse(op);
		break;

	case EVENT_NET_ERROR:
		// KFS_LOG_VA_DEBUG("Closing connection");

		if (mNetConnection)
			mNetConnection->Close();

		mSentHello = false;
		// Give up the underlying pointer
		mNetConnection.reset();
		break;

	default:
		assert(!"Unknown event");
		break;
	}
	return 0;
}

/// 处理服务器发送的消息
/// @note 服务器发送的消息只有两种：1. 对于已经发送的op的回复 2. RPC
bool MetaServerSM::HandleMsg(IOBuffer *iobuf, int msgLen)
{
	char buf[5];

	// 从buffer中取出3个字符
	iobuf->CopyOut(buf, 3);
	buf[4] = '\0';

	if (strncmp(buf, "OK", 2) == 0)
	{
		// This is a response to some op we sent earlier
		// 如果这个消息是以OK开头，则说明它是对于某一个op的回复
		HandleReply(iobuf, msgLen);
		return true;
	}
	else
	{
		// is an RPC from the server
		// 处理RPC
		return HandleCmd(iobuf, msgLen);
	}
}

/// 处理服务器返回的对于操作的回复信息，删除mDispatchedOps中的对应项
void MetaServerSM::HandleReply(IOBuffer *iobuf, int msgLen)
{
	scoped_array<char> buf;
	const char separator = ':';
	kfsSeq_t seq;
	int status;
	list<KfsOp *>::iterator iter;

	// 将iobuf中的数据拷贝到buf中，并添加结束符
	buf.reset(new char[msgLen + 1]);
	iobuf->CopyOut(buf.get(), msgLen);
	buf[msgLen] = '\0';

	// 删除iobuf中的数据
	iobuf->Consume(msgLen);
	istringstream ist(buf.get());
	Properties prop;

	// 分析读入的数据中的参数
	prop.loadProperties(ist, separator, false);
	seq = prop.getValue("Cseq", (kfsSeq_t) -1);
	status = prop.getValue("Status", -1);

	if (status == -EBADCLUSTERKEY)
	{
		KFS_LOG_VA_FATAL("Aborting...due to cluster key mismatch; our key: %s",
				mClusterKey.c_str());
		exit(-1);
	}

	// 在mDispatchedOps中查找seq相同的操作
	iter = find_if(mDispatchedOps.begin(), mDispatchedOps.end(), OpMatcher(
			seq));
	if (iter == mDispatchedOps.end())
		return;

	KfsOp *op = *iter;
	op->status = status;
	// 删除该操作在mDispatchedOps中的副本
	mDispatchedOps.erase(iter);

	// The op will be gotten rid of by this call.
	// op->HandleEvent(EVENT_CMD_DONE, op);
	KFS::SubmitOpResponse(op);
}

///
/// We have a command in a buffer.  It is possible that we don't have
/// everything we need to execute it (for example, for a stale chunks
/// RPC, we may not have received all the chunkids).  So, parse
/// out the command and if we have everything execute it.
///
/// @note 只有在iobuf中已经接受到了完整的cmd时才进行实际的操作
/// 要处理的命令只有删除chunk一种，其他的解析后直接提交到SubmitOp()即可
bool MetaServerSM::HandleCmd(IOBuffer *iobuf, int cmdLen)
{
	scoped_array<char> buf;
	StaleChunksOp *sc;
	istringstream ist;
	kfsChunkId_t c;
	int i, nAvail;
	KfsOp *op;

	buf.reset(new char[cmdLen + 1]);
	iobuf->CopyOut(buf.get(), cmdLen);
	buf[cmdLen] = '\0';

	// 解析iobuf中的RPC命令，放到op中
	if (ParseCommand(buf.get(), cmdLen, &op) != 0)
	{
		// 如果
		iobuf->Consume(cmdLen);

		KFS_LOG_VA_DEBUG("Aye?: %s", buf.get());
		// got a bogus command
		return false;
	}

	// 只处理删除chunk的操作
	if (op->op == CMD_STALE_CHUNKS)
	{// 这个命令是删除chunk的命令
		sc = static_cast<StaleChunksOp *> (op);
		// if we don't have all the data wait...
		nAvail = iobuf->BytesConsumable() - cmdLen;

		// 如果nAvail < sc->contentLength，则说明这个命令不完整
		if (nAvail < sc->contentLength)
		{
			delete op;
			return false;
		}
		iobuf->Consume(cmdLen);
		buf.reset(new char[sc->contentLength + 1]);
		buf[sc->contentLength] = '\0';
		iobuf->CopyOut(buf.get(), sc->contentLength);
		iobuf->Consume(sc->contentLength);

		ist.str(buf.get());

		// 解析出命令中要删除的所有chunk ID
		for (i = 0; i < sc->numStaleChunks; ++i)
		{
			ist >> c;
			sc->staleChunkIds.push_back(c);
		}

	}
	else
	{
		iobuf->Consume(cmdLen);
	}

	op->clnt = this;
	// op->Execute();

	// RPC命令解析完毕，可以进行操作了
	KFS::SubmitOp(op);
	return true;
}

/// 添加操作到等待队列中
void MetaServerSM::EnqueueOp(KfsOp *op)
{
	op->seq = nextSeq();

	mPendingOps.enqueue(op);

	globals().netKicker.Kick();
}

///
/// Queue the response to the meta server request.  The response is
/// generated by MetaRequest as per the protocol.
/// @param[in] op The request for which we finished execution.
///
/// 将要回复的信息添加到mPendingResponses队列中
void MetaServerSM::EnqueueResponse(KfsOp *op)
{
	mPendingResponses.enqueue(op);
	globals().netKicker.Kick();
}

/// 向metaserver发送mPendingOps中的操作请求
void MetaServerSM::DispatchOps()
{
	KfsOp *op;

#ifdef DEBUG
	verifyExecutingOnNetProcessor();
#endif

	// 对mPendingOps中的每一个操作请求进行处理：从最早提交的一个开始
	while ((op = mPendingOps.dequeue_nowait()) != NULL)
	{// dequeue_nowait()取出最早压入队列的一个元素
		ostringstream os;

		assert(op->op != CMD_META_HELLO);

		// 发送到已经发出的请求队列里边，等待服务器的回复信息
		mDispatchedOps.push_back(op);

		// XXX: If the server connection is dead, hold on
		// 如果连向metaserver的服务器连接不可用，或者还没有发出问好消息，则不做任何操作
		if ((!mNetConnection) || (!mSentHello))
		{
			KFS_LOG_INFO("Metaserver connection is down...will dispatch later");
			return;
		}

		// 将操作请求发送到远端服务器
		op->Request(os);
		mNetConnection->Write(os.str().c_str(), os.str().length());
	}
}

/// 将mPendingResponse中的回复操作发送到metaserver中
/// @note 由于不需要接受服务器对于response的回复，故不添加到mDispatchedResponse中
void MetaServerSM::DispatchResponse()
{
	KfsOp *op;

#ifdef DEBUG
	verifyExecutingOnNetProcessor();
#endif

	// 对于mPendingResponse中的每一个回复操作：
	while ((op = mPendingResponses.dequeue_nowait()) != NULL)
	{
		ostringstream os;

		// fire'n'forget..
		op->Response(os);
		// 发送到远端服务器
		mNetConnection->Write(os.str().c_str(), os.str().length());
		delete op;
	}
}

// 用于向metaserver提交操作请求
class OpDispatcher
{
	NetConnectionPtr conn;
public:
	OpDispatcher(NetConnectionPtr &c) :
		conn(c)
	{
	}

	// 发送请求
	void operator()(KfsOp *op)
	{
		ostringstream os;

		op->Request(os);
		conn->Write(os.str().c_str(), os.str().length());
	}
};

// After re-establishing connection to the server, resubmit the ops.
// 重新向系统提交所有的操作
void MetaServerSM::ResubmitOps()
{
	for_each(mDispatchedOps.begin(), mDispatchedOps.end(), OpDispatcher(
			mNetConnection));
}
