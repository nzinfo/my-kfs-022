//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: ChunkServer.cc 149 2008-09-10 05:31:35Z sriramsrao $
//
// Created 2006/06/06
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

#include "ChunkServer.h"
#include "LayoutManager.h"
#include "NetDispatch.h"
#include "util.h"
#include "libkfsIO/Globals.h"

using namespace KFS;
using namespace libkfsio;

#include <cassert>
#include <string>
#include <sstream>
using std::string;
using std::istringstream;

#include <boost/scoped_array.hpp>
using boost::scoped_array;

#include "common/log.h"

//
// if a chunkserver is not responsive for over 10 mins, mark it down
//
const int32_t INACTIVE_SERVER_TIMEOUT = 600;

ChunkServer::ChunkServer() :
	mSeqNo(1), mTimer(NULL),
	mHelloDone(false), mDown(false), mHeartbeatSent(false), mHeartbeatSkipped(
			false), mIsRetiring(false), mRackId(-1), mNumCorruptChunks(0),
			mTotalSpace(0), mUsedSpace(0), mAllocSpace(0), mNumChunks(0),
			mNumChunkWrites(0), mNumChunkWriteReplications(0),
			mNumChunkReadReplications(0)
{
	// this is used in emulation mode...

}

/// 用面向chunkserver的netConnection来构造ChunkServer
ChunkServer::ChunkServer(NetConnectionPtr &conn) :
	mSeqNo(1), mNetConnection(conn), mHelloDone(false), mDown(false),
			mHeartbeatSent(false), mHeartbeatSkipped(false),
			mIsRetiring(false), mRackId(-1), mNumCorruptChunks(0), mTotalSpace(
					0), mUsedSpace(0), mAllocSpace(0), mNumChunks(0),
			mNumChunkWrites(0), mNumChunkWriteReplications(0),
			mNumChunkReadReplications(0)
{
	mTimer = new ChunkServerTimeoutImpl(this);
	// Receive HELLO message
	SET_HANDLER(this, &ChunkServer::HandleHello);

	globals().netManager.RegisterTimeoutHandler(mTimer);
}

ChunkServer::~ChunkServer()
{
	// KFS_LOG_VA_DEBUG("Deleting %p", this);

	if (mNetConnection)
		mNetConnection->Close();
	if (mTimer)
	{
		globals().netManager.UnRegisterTimeoutHandler(mTimer);
		delete mTimer;
	}
}

/// 注销当前ChunkServer中的timeout
void ChunkServer::StopTimer()
{
	if (mTimer)
	{
		globals().netManager.UnRegisterTimeoutHandler(mTimer);
		delete mTimer;
		mTimer = NULL;
	}
}

///
/// Handle a HELLO message from the chunk server.
/// @param[in] code: The type of event that occurred
/// @param[in] data: Data being passed in relative to the event that
/// occurred.
/// @retval 0 to indicate successful event handling; -1 otherwise.
///
/// 处理从chunkserver发送过来的Hello信息：若从未收到过该chunkserver的Hello信息，并且
/// 此次Hello信息不正确，则直接中断同该chunkserver的连接
int ChunkServer::HandleHello(int code, void *data)
{
	IOBuffer *iobuf;
	int msgLen, retval;

	switch (code)
	{
	case EVENT_NET_READ:
		// We read something from the network.  It
		// better be a HELLO message.
		iobuf = (IOBuffer *) data;

		// 是否已经完成了一个完整部分信息的读取
		if (IsMsgAvail(iobuf, &msgLen))
		{
			// 处理iobuf中的Hello消息，发送到requestList中
			retval = HandleMsg(iobuf, msgLen);
			if (retval < 0)
			{
				// Couldn't process hello
				// message...bye-bye
				// 如果不能判断该chunkserver发送的信息有效，则删除该chunk
				mDown = true;
				gNetDispatch.GetChunkServerFactory()->RemoveServer(this);
				return -1;
			}
			if (retval > 0)
			{
				// not all data is available...so, hold on
				return 0;

			}

			// 到此时，可以说明该Hello操作已经确认完毕，设置HandleRequest执行操作
			mHelloDone = true;
			mLastHeard = time(NULL);
			// Hello message successfully
			// processed.  Setup to handle RPCs
			SET_HANDLER(this, &ChunkServer::HandleRequest);
		}
		break;

	case EVENT_NET_WROTE:
		// Something went out on the network.
		break;

		// 如果这个chunkserver出现了网络错误，则终止同该chunkserver的连接
	case EVENT_NET_ERROR:
		// KFS_LOG_VA_DEBUG("Closing connection");
		mDown = true;
		gNetDispatch.GetChunkServerFactory()->RemoveServer(this);
		break;

	default:
		assert(!"Unknown event");
		return -1;
	}
	return 0;
}

///
/// Generic event handler.  Decode the event that occurred and
/// appropriately extract out the data and deal with the event.
/// @param[in] code: The type of event that occurred
/// @param[in] data: Data being passed in relative to the event that
/// occurred.
/// @retval 0 to indicate successful event handling; -1 otherwise.
/// 处理chunkserver发送的对于Op的请求：NET_READ, CMD_DONE, NET_WROTE, NET_ERROR.
int ChunkServer::HandleRequest(int code, void *data)
{
	IOBuffer *iobuf;
	int msgLen;
	MetaRequest *op;

	switch (code)
	{

	// 如果是EVENT_NET_READ操作，并且已经接受到完整数据，则直接调用HandleMsg执行
	case EVENT_NET_READ:
		// We read something from the network.  It is
		// either an RPC (such as hello) or a reply to
		// an RPC we sent earlier.
		iobuf = (IOBuffer *) data;
		while (IsMsgAvail(iobuf, &msgLen))
		{
			HandleMsg(iobuf, msgLen);
		}
		break;

		// 如果是事件完成确认操作，并且同chunkserver的连接正常，则直接回复op，并删除该操作
	case EVENT_CMD_DONE:
		op = (MetaRequest *) data;
		if (!mDown)
		{
			SendResponse(op);
		}
		// nothing left to be done...get rid of it
		delete op;
		break;

		// 如果是写操作，则直接退出即可
	case EVENT_NET_WROTE:
		// Something went out on the network.
		break;

		// 如果是网络错误操作，则：关闭定时器，删除mDispatchedReqs中的op，关闭同
		// chunkserver的连接，强制该chunkserver退出
	case EVENT_NET_ERROR:
		KFS_LOG_VA_INFO("Chunk server %s is down...", GetServerName());

		// 关闭定时器，删除mDispatchedReqs队列中的所有op
		StopTimer();
		FailDispatchedOps();

		// Take out the server from write-allocation
		mTotalSpace = mAllocSpace = mUsedSpace = 0;

		// 关闭同该chunkserver的连接
		mNetConnection->Close();
		mNetConnection.reset();

		mDown = true;
		// force the server down thru the main loop to avoid races
		// 强制chunkserver退出
		op = new MetaBye(0, shared_from_this());
		op->clnt = this;

		gNetDispatch.GetChunkServerFactory()->RemoveServer(this);
		submit_request(op);

		break;

	default:
		assert(!"Unknown event");
		return -1;
	}
	return 0;
}

///
/// We have a message from the chunk server.  The message we got is one
/// of:
///  -  a HELLO message from the server
///  -  it is a response to some command we previously sent
///  -  is an RPC from the chunkserver
///
/// Of these, for the first and third case,create an op and
/// send that down the pike; in the second case, retrieve the op from
/// the pending list, attach the response, and push that down the pike.
///
/// @param[in] iobuf: Buffer containing the command
/// @param[in] msgLen: Length of the command in the buffer
/// @retval 0 if we handled the message properly; -1 on error;
///   1 if there is more data needed for this message and we haven't
///   yet received the data.
/// IOBuffer中存放的是消息内容: Hello, Reply, Command.
int ChunkServer::HandleMsg(IOBuffer *iobuf, int msgLen)
{
	char buf[5];

	// 如果mHelloDone未置位，则这一条消息必须是Hello消息，否则则是错误
	if (!mHelloDone)
	{
		/// 处理Hello信息，但此时mHelloDone并没有置位，而是放进requestList中待处理
		return HandleHelloMsg(iobuf, msgLen);
	}

	iobuf->CopyOut(buf, 3);
	buf[4] = '\0';
	if (strncmp(buf, "OK", 2) == 0)
	{
		// 如果是收到的Response消息，则为之进行处理
		return HandleReply(iobuf, msgLen);
	}

	// 如果以上两种操作都不是，则一定是第三种操作：RPC
	return HandleCmd(iobuf, msgLen);
}

/// Case #1: Handle Hello message from a chunkserver that
/// just connected to us.
/// 处理刚刚连接到本metaserver的chunkserver发送的Hello消息：
int ChunkServer::HandleHelloMsg(IOBuffer *iobuf, int msgLen)
{
	// 存放命令的具体数据
	scoped_array<char> buf, contentBuf;
	MetaRequest *op;
	MetaHello *helloOp;
	int i, nAvail;
	istringstream ist;

	buf.reset(new char[msgLen + 1]);
	// 将iobuf中的数据拷贝到buf中待处理
	iobuf->CopyOut(buf.get(), msgLen);
	buf[msgLen] = '\0';

	assert(!mHelloDone);

	// We should only get a HELLO message here; anything
	// else is bad.
	// 解析命令转换成对应的KfsOp
	if (ParseCommand(buf.get(), msgLen, &op) < 0)
	{
		KFS_LOG_VA_DEBUG("Aye?: %s", buf.get());
		iobuf->Consume(msgLen);
		// we couldn't parse out hello
		return -1;
	}

	// we really ought to get only hello here
	if (op->op != META_HELLO)
	{
		// 如果不是Hello信息，则错误退出
		KFS_LOG_VA_DEBUG("Only  need hello...but: %s", buf.get());
		iobuf->Consume(msgLen);
		delete op;
		// got a bogus command
		return -1;

	}

	helloOp = static_cast<MetaHello *> (op);

	KFS_LOG_VA_INFO("New server: \n%s", buf.get());
	op->clnt = this;
	helloOp->server = shared_from_this();

	// make sure we have the chunk ids...
	if (helloOp->contentLength > 0)
	{
		nAvail = iobuf->BytesConsumable() - msgLen;
		if (nAvail < helloOp->contentLength)
		{
			// 说明命令数据尚未传输完毕，需要返回等待
			// need to wait for data...
			delete op;
			return 1;
		}
		// we have everything
		iobuf->Consume(msgLen);

		contentBuf.reset(new char[helloOp->contentLength + 1]);
		contentBuf[helloOp->contentLength] = '\0';

		// get the chunkids
		// 将命令数据拷贝到contentBuf中
		iobuf->CopyOut(contentBuf.get(), helloOp->contentLength);
		iobuf->Consume(helloOp->contentLength);

		// 将命令数据存储成一个字符串
		ist.str(contentBuf.get());

		// 分别处理每一条chunk信息
		for (i = 0; i < helloOp->numChunks; ++i)
		{
			ChunkInfo c;

			ist >> c.fileId;
			ist >> c.chunkId;
			ist >> c.chunkVersion;
			helloOp->chunks.push_back(c);
			// KFS_LOG_VA_DEBUG("Server has chunk: %lld", chunkId);
		}
	}
	else
	{
		// Message is ready to be pushed down.  So remove it.
		iobuf->Consume(msgLen);
	}

	// send it on its merry way
	// 将该操作提交到requestList中（全局变量），待处理
	submit_request(op);
	return 0;
}

///
/// Case #2: Handle an RPC from a chunkserver.
///
/// 解析RPC：解析完成之后发送到RequestList后退出
int ChunkServer::HandleCmd(IOBuffer *iobuf, int msgLen)
{
	scoped_array<char> buf;
	MetaRequest *op;

	// 将iobuf中的cmd数据拷贝到buf中之后，删除其中的数据
	buf.reset(new char[msgLen + 1]);
	iobuf->CopyOut(buf.get(), msgLen);
	buf[msgLen] = '\0';

	assert(mHelloDone);

	// Message is ready to be pushed down.  So remove it.
	iobuf->Consume(msgLen);

	// 解析命令操作，如果解析失败，则错误退出
	if (ParseCommand(buf.get(), msgLen, &op) != 0)
	{
		return -1;
	}
	if (op->op == META_CHUNK_CORRUPT)
	{
		MetaChunkCorrupt *ccop = static_cast<MetaChunkCorrupt *> (op);

		ccop->server = shared_from_this();
	}
	op->clnt = this;
	submit_request(op);

	/*
	 if (iobuf->BytesConsumable() > 0) {
	 KFS_LOG_VA_DEBUG("More command data likely available: ");
	 }
	 */
	return 0;
}

///
/// Case #3: Handle a reply from a chunkserver to an RPC we
/// previously sent.
/// 处理收到的回复信息: HEARTBEAT, REPLICATE, CHUNKSIZE
int ChunkServer::HandleReply(IOBuffer *iobuf, int msgLen)
{
	scoped_array<char> buf, contentBuf;
	MetaRequest *op;
	int status;
	seq_t cseq;
	istringstream ist;
	Properties prop;
	MetaChunkRequest *submittedOp;

	buf.reset(new char[msgLen + 1]);
	iobuf->CopyOut(buf.get(), msgLen);
	buf[msgLen] = '\0';

	assert(mHelloDone);

	// Message is ready to be pushed down.  So remove it.
	iobuf->Consume(msgLen);

	// ----->此时已经将iobuf中的数据拷贝到了buf中

	// We got a response for a command we previously
	// sent.  So, match the response to its request and
	// resume request processing.
	// 将Response中的数据解析出来存放到prop中
	ParseResponse(buf.get(), msgLen, prop);
	cseq = prop.getValue("Cseq", (seq_t) -1);
	status = prop.getValue("Status", -1);

	// 在mDispatchedReqs中查找cseq对应的操作，并从mDispatchedReqs中删除
	op = FindMatchingRequest(cseq);
	if (op == NULL)
	{
		// 如果服务器在中途重启，可能会出现此种情况
		// Uh-oh...this can happen if the server restarts between sending
		// the message and getting reply back
		// assert(!"Unable to find command for a response");
		KFS_LOG_VA_WARN("Unable to find command for response (cseq = %d)", cseq);
		return -1;
	}
	// KFS_LOG_VA_DEBUG("Got response for cseq=%d", cseq);

	// 改变对应操作的状态等信息
	submittedOp = static_cast<MetaChunkRequest *> (op);
	submittedOp->status = status;
	if (submittedOp->op == META_CHUNK_HEARTBEAT)
	{
		// 如果这个回复是心跳请求的回复，则更新一下信息：磁盘空间总量，可用磁盘空间数，chunk数，
		// 已经分配出去的磁盘空间，上一次心跳时间
		mTotalSpace = prop.getValue("Total-space", (long long) 0);
		mUsedSpace = prop.getValue("Used-space", (long long) 0);
		mNumChunks = prop.getValue("Num-chunks", 0);
		mAllocSpace = mUsedSpace + mNumChunkWrites * CHUNKSIZE;
		mHeartbeatSent = false;
		mLastHeard = time(NULL);
	}
	else if (submittedOp->op == META_CHUNK_REPLICATE)
	{
		// 如果这个回复是Replicator的回复，则设置该op的fid和chunkVersion
		MetaChunkReplicate *mcr = static_cast<MetaChunkReplicate *> (op);
		mcr->fid = prop.getValue("File-handle", (long long) 0);
		mcr->chunkVersion = prop.getValue("Chunk-version", (long long) 0);
	}
	else if (submittedOp->op == META_CHUNK_SIZE)
	{
		// 如果这个操作是请求chunk大小的操作，则更新op的chunkSize
		MetaChunkSize *mcs = static_cast<MetaChunkSize *> (op);
		mcs->chunkSize = prop.getValue("Size", (off_t) -1);
	}

	// 完成对应op的数据更新后，重新起动该op继续执行
	ResumeOp(op);

	return 0;
}

/// 重新开始执行op：完成指定要求后，提交给requestList.
void ChunkServer::ResumeOp(MetaRequest *op)
{
	MetaChunkRequest *submittedOp;
	MetaAllocate *allocateOp;
	MetaRequest *req;

	// get the original request and get rid of the
	// intermediate one we generated for the RPC.
	submittedOp = static_cast<MetaChunkRequest *> (op);
	req = submittedOp->req;

	// op types:
	// - allocate ops have an "original" request; this
	//    needs to be reactivated, so that a response can
	//    be sent to the client.
	// -  delete ops are a fire-n-forget. there is no
	//    other processing left to be done on them.
	// -  heartbeat: update the space usage statistics and nuke
	// it, which is already done (in the caller of this method)
	// -  stale-chunk notify: we tell the chunk server and that is it.
	//
	if (submittedOp->op == META_CHUNK_ALLOCATE)
	{
		// 如果这个op是一个chunk分配操作
		/*
		 * 在为一个块进行空间分配时，需要同时向多个chunk server发出分配命令，并且等待所有的
		 * chunkserver完成操作才能结束
		 */
		assert(req && (req->op == META_ALLOCATE));

		// if there is a non-zero status, don't throw it away
		if (req->status == 0)
			req->status = submittedOp->status;

		delete submittedOp;


		allocateOp = static_cast<MetaAllocate *> (req);
		allocateOp->numServerReplies++;

		// wait until we get replies from all servers
		// 如果所有的chunkserver都已经完成了磁盘分配操作，则可已完成操作了
		if (allocateOp->numServerReplies == allocateOp->servers.size())
		{
			allocateOp->layoutDone = true;
			// The op is no longer suspended.
			req->suspended = false;
			// send it on its merry way
			submit_request(req);
		}
	}
	/*
	 * 如果这个操作是删除chunk，改变chunk大小，心跳，删除确认，更改版本号或关闭chunkserver
	 * 操作，则可以直接退出，完成操作
	 */
	else if ((submittedOp->op == META_CHUNK_DELETE) || (submittedOp->op
			== META_CHUNK_TRUNCATE)
			|| (submittedOp->op == META_CHUNK_HEARTBEAT) || (submittedOp->op
			== META_CHUNK_STALENOTIFY) || (submittedOp->op
			== META_CHUNK_VERSCHANGE) || (submittedOp->op == META_CHUNK_RETIRE))
	{
		assert(req == NULL);
		delete submittedOp;
	}

	// 如果这个操作是Replicate操作，则直接提交操作
	else if (submittedOp->op == META_CHUNK_REPLICATE)
	{
		// This op is internally generated.  We need to notify
		// the layout manager of this op's completion.  So, send
		// it there.
		MetaChunkReplicate *mcr =
				static_cast<MetaChunkReplicate *> (submittedOp);
		KFS_LOG_VA_DEBUG("Meta chunk replicate for chunk %lld finished with version %lld, status: %d",
				mcr->chunkId, mcr->chunkVersion, submittedOp->status);
		submit_request(submittedOp);
		// the op will get nuked after it is processed
	}

	// 如果这个操作是请求chunk大小，则直接提交给requestList
	else if (submittedOp->op == META_CHUNK_SIZE)
	{
		// chunkserver has responded with the chunk's size.  So, update
		// the meta-tree
		submit_request(submittedOp);
	}
	else
	{
		assert(!"Unknown op!");
	}

}

///
/// The response sent by a chunkserver is of the form:
/// OK \r\n
/// Cseq: <seq #>\r\n
/// Status: <status> \r\n
/// {<other header/value pair>\r\n}*\r\n
///
/// @param[in] buf Buffer containing the response
/// @param[in] bufLen length of buf
/// @param[out] prop  Properties object with the response header/values
/// buf中存放Response的数据，将buf中的数据解析到prop中
void ChunkServer::ParseResponse(char *buf, int bufLen, Properties &prop)
{
	istringstream ist(buf);
	const char separator = ':';
	string respOk;

	// KFS_LOG_VA_DEBUG("Got chunk-server-response: %s", buf);

	ist >> respOk;
	// 第一行必须是OK
	if (respOk.compare("OK") != 0)
	{

		KFS_LOG_VA_DEBUG("Didn't get an OK: instead, %s",
				respOk.c_str());

		return;
	}
	// 将Response的数据解析到prop中
	prop.loadProperties(ist, separator, false);
}

// Helper functor that matches ops by sequence #'s
class OpMatch
{
	seq_t myseq;
public:
	OpMatch(seq_t s) :
		myseq(s)
	{
	}
	bool operator()(const MetaRequest *r)
	{
		return (r->opSeqno == myseq);
	}
};

///
/// Request/responses are matched based on sequence #'s.
///
/// 从mDispatchedReqs中取出cseq对应的操作，并在mDispatchedReqs中删除
MetaRequest *
ChunkServer::FindMatchingRequest(seq_t cseq)
{
	list<MetaRequest *>::iterator iter;
	MetaRequest *op;

	iter = find_if(mDispatchedReqs.begin(), mDispatchedReqs.end(),
			OpMatch(cseq));
	if (iter == mDispatchedReqs.end())
		return NULL;

	op = *iter;
	mDispatchedReqs.erase(iter);
	return op;
}

///
/// Queue an RPC request
///
/// 将r加入到mPendingReqs中，等待处理
void ChunkServer::Enqueue(MetaRequest *r)
{
	mPendingReqs.enqueue(r);
	globals().netKicker.Kick();
}

/// 在chunkserver中分配一个chunk，需要更新的数据有：mAllocSpace, chunk写次数。然后向
/// chunkserver发送命令
int ChunkServer::AllocateChunk(MetaAllocate *r, int64_t leaseId)
{
	MetaChunkAllocate *ca;

	mAllocSpace += CHUNKSIZE;

	UpdateNumChunkWrites(1);

	ca = new MetaChunkAllocate(NextSeq(), r, this, leaseId);

	// save a pointer to the request so that we can match up the
	// response whenever we get it.
	Enqueue(ca);

	return 0;
}

/// 删除chunkserver中的一个chunk，需要更新的数据同上，操作相反
int ChunkServer::DeleteChunk(chunkId_t chunkId)
{
	MetaChunkDelete *r;

	mAllocSpace -= CHUNKSIZE;

	if (IsRetiring())
	{
		EvacuateChunkDone(chunkId);
	}

	r = new MetaChunkDelete(NextSeq(), this, chunkId);

	// save a pointer to the request so that we can match up the
	// response whenever we get it.
	Enqueue(r);

	return 0;
}

/// 发送命令要求chunkserver改变指定chunk的大小
int ChunkServer::TruncateChunk(chunkId_t chunkId, off_t s)
{
	MetaChunkTruncate *r;

	mAllocSpace -= (CHUNKSIZE - s);

	r = new MetaChunkTruncate(NextSeq(), this, chunkId, s);

	// save a pointer to the request so that we can match up the
	// response whenever we get it.
	Enqueue(r);

	return 0;
}

/// 向chunkserver请求指定的chunk的大小
int ChunkServer::GetChunkSize(fid_t fid, chunkId_t chunkId)
{
	MetaChunkSize *r;

	r = new MetaChunkSize(NextSeq(), this, fid, chunkId);

	// save a pointer to the request so that we can match up the
	// response whenever we get it.
	Enqueue(r);

	return 0;
}

/// 发送命令要求chunkserver向指定的Server请求数据复制
int ChunkServer::ReplicateChunk(fid_t fid, chunkId_t chunkId,
		seq_t chunkVersion, const ServerLocation &loc)
{
	MetaChunkReplicate *r;

	r = new MetaChunkReplicate(NextSeq(), this, fid, chunkId,
			chunkVersion, loc);
	r->server = shared_from_this();
	mNumChunkWriteReplications++;
	// save a pointer to the request so that we can match up the
	// response whenever we get it.
	Enqueue(r);

	return 0;
}

/// 检测chunkserver的运行状态
void ChunkServer::Heartbeat()
{
	// 如果尚未收到Hello消息，即连接尚未建立
	if (!mHelloDone)
	{
		return;
	}

	if (mHeartbeatSent)
	{
		// 如果心跳请求已经发送，则检测是否已经超时
		string loc = mLocation.ToString();
		time_t now = time(0);

		if (now - mLastHeard > INACTIVE_SERVER_TIMEOUT)
		{
			// 如果已经超时，则中断同该服务器的连接
			KFS_LOG_VA_INFO("Server %s has been non-responsive for too long; taking it down", loc.c_str());
			// We are executing in the context of the network thread
			// So, take the server down as though the net connection
			// broke.
			HandleRequest(EVENT_NET_ERROR, NULL);
			return;
		}

		// 如果已经发送了一个heartbeat请求并且尚未收到回复，则不必在发送另一个请求了
		mHeartbeatSkipped = true;
		KFS_LOG_VA_INFO("Skipping send of heartbeat to %s", loc.c_str());
		return;
	}

	// 没有正在等待的heartbeat回复，此时发送heart beat请求
	mHeartbeatSent = true;
	mHeartbeatSkipped = false;

	MetaChunkHeartbeat *r;

	r = new MetaChunkHeartbeat(NextSeq(), this);

	// save a pointer to the request so that we can match up the
	// response whenever we get it.
	// 放到队列中等待回复
	Enqueue(r);
}

/// 通知删除指定chunk序列
void ChunkServer::NotifyStaleChunks(const vector<chunkId_t> &staleChunkIds)
{
	MetaChunkStaleNotify *r;

	mAllocSpace -= (CHUNKSIZE * staleChunkIds.size());
	r = new MetaChunkStaleNotify(NextSeq(), this);

	r->staleChunkIds = staleChunkIds;

	// save a pointer to the request so that we can match up the
	// response whenever we get it.
	Enqueue(r);

}

/// 通知删除指定chunk
void ChunkServer::NotifyStaleChunk(chunkId_t staleChunkId)
{
	MetaChunkStaleNotify *r;

	mAllocSpace -= CHUNKSIZE;
	r = new MetaChunkStaleNotify(NextSeq(), this);

	r->staleChunkIds.push_back(staleChunkId);

	// save a pointer to the request so that we can match up the
	// response whenever we get it.
	Enqueue(r);
}

/// 通知改变指定chunk的Version
void ChunkServer::NotifyChunkVersChange(fid_t fid, chunkId_t chunkId,
		seq_t chunkVers)
{
	MetaChunkVersChange *r;

	r = new MetaChunkVersChange(NextSeq(), this, fid, chunkId, chunkVers);

	// save a pointer to the request so that we can match up the
	// response whenever we get it.
	Enqueue(r);
}

///
void ChunkServer::SetRetiring()
{
	mIsRetiring = true;
	mRetireStartTime = time(NULL);
	KFS_LOG_VA_INFO("Initiation of retire for chunks on %s : %d blocks to do",
			ServerID().c_str(), mNumChunks);
}

/// 清空指定的chunk，若mEvacuatingChunks中的所有chunk都已经evacuated，则Retire该server
void ChunkServer::EvacuateChunkDone(chunkId_t chunkId)
{
	if (!mIsRetiring)
		return;
	mEvacuatingChunks.erase(chunkId);
	if (mEvacuatingChunks.empty())
	{
		KFS_LOG_VA_INFO("Evacuation of chunks on %s is done; retiring",
				ServerID().c_str());
		Retire();
	}
}

/// 发送retire命令到pending队列中
void ChunkServer::Retire()
{
	MetaChunkRetire *r;

	r = new MetaChunkRetire(NextSeq(), this);
	Enqueue(r);
}

//
// Helper functor that dispatches an RPC request to the server.
//
class OpDispatcher
{
	ChunkServer *server;
	NetConnectionPtr conn;
public:
	OpDispatcher(ChunkServer *s, NetConnectionPtr &c) :
		server(s), conn(c)
	{
	}

	// 将命令通过网络分发到chunkserver
	void operator()(MetaRequest *r)
	{

		ostringstream os;
		MetaChunkRequest *cr = static_cast<MetaChunkRequest *> (r);

		if (!conn)
		{
			// Server is dead...so, drop the op
			r->status = -EIO;
			server->ResumeOp(r);
			return;
		}
		assert(cr != NULL);

		// Get the request into string format
		cr->request(os);

		// Send it on its merry way
		conn->Write(os.str().c_str(), os.str().length());
		// Notify the server the op is dispatched
		server->Dispatched(r);
	}
};

/// 发送mPendingReqs队列中的操作到chunkserver中
void ChunkServer::Dispatch()
{
	OpDispatcher dispatcher(this, mNetConnection);
	list<MetaRequest *> reqs;
	MetaRequest *r;

	while ((r = mPendingReqs.dequeue_nowait()))
	{
		reqs.push_back(r);
	}
	for_each(reqs.begin(), reqs.end(), dispatcher);

	reqs.clear();
}

// Helper functor that fails an op with an error code.
// 设置指定的op的status为错误，并且完成op操作
class OpFailer
{
	ChunkServer *server;
	int errCode;
public:
	OpFailer(ChunkServer *s, int c) :
		server(s), errCode(c)
	{
	}
	;
	void operator()(MetaRequest *op)
	{
		op->status = errCode;
		server->ResumeOp(op);
	}
};

// 设置mDispatchedReqs中的所有op为Error
void ChunkServer::FailDispatchedOps()
{
	for_each(mDispatchedReqs.begin(), mDispatchedReqs.end(), OpFailer(this,
			-EIO));

	mDispatchedReqs.clear();
}

// 将mPendingReqs中的所有op设置为Error
void ChunkServer::FailPendingOps()
{
	list<MetaRequest *> reqs;
	MetaRequest *r;

	while ((r = mPendingReqs.dequeue_nowait()))
	{
		reqs.push_back(r);
	}
	for_each(reqs.begin(), reqs.end(), OpFailer(this, -EIO));
	reqs.clear();
}

inline float convertToMB(off_t bytes)
{
	return bytes / (1024.0 * 1024.0);
}

inline float convertToGB(off_t bytes)
{
	return bytes / (1024.0 * 1024.0 * 1024.0);
}

/// 提取chunkserver的Retire信息到result中
/// 输出的信息包括: hostname, port, starttime, numleft, numdone.
void ChunkServer::GetRetiringStatus(string &result)
{
	if (!mIsRetiring)
		return;

	ostringstream ost;
	char timebuf[64];
	ctime_r(&mRetireStartTime, timebuf);
	if (timebuf[24] == '\n')
		timebuf[24] = '\0';

	ost << "s=" << mLocation.hostname << ", p=" << mLocation.port
			<< ", started=" << timebuf << ", numLeft="
			<< mEvacuatingChunks.size() << ", numDone=" << mNumChunks
			- mEvacuatingChunks.size() << '\t';

	result += ost.str();
}

/// 从本地获取chunkserver的信息，输出到result中
/// 输出的信息包括: hostname, port, totalSpace, usedSpace, util, numChunks,
/// lastHeard, numCorruptChunks, chunksToMove.
void ChunkServer::Ping(string &result)
{
	ostringstream ost;
	char marker = ' ';
	time_t now = time(NULL);

	// for retiring nodes, add a '*' so that we can differentiate in the UI
	// with web-ui, don't need this...
	// if (mIsRetiring)
	//	marker = '*';

	if (mTotalSpace < (1L << 30))
	{
		ost << "s=" << mLocation.hostname << marker << ", p=" << mLocation.port
				<< ", total=" << convertToMB(mTotalSpace) << "(MB), used="
				<< convertToMB(mUsedSpace) << "(MB), util="
				<< GetSpaceUtilization() * 100.0 << "%, nblocks=" << mNumChunks
				<< ", lastheard=" << now - mLastHeard << " (sec)"
				<< ", ncorrupt=" << mNumCorruptChunks << ", nchunksToMove="
				<< mChunksToMove.size() << "\t";
	}
	else
	{
		ost << "s=" << mLocation.hostname << marker << ", p=" << mLocation.port
				<< ", total=" << convertToGB(mTotalSpace) << "(GB), used="
				<< convertToGB(mUsedSpace) << "(GB), util="
				<< GetSpaceUtilization() * 100.0 << "%, nblocks=" << mNumChunks
				<< ", lastheard=" << now - mLastHeard << " (sec)"
				<< ", ncorrupt=" << mNumCorruptChunks << ", nchunksToMove="
				<< mChunksToMove.size() << "\t";
	}
	result += ost.str();
}

// 发送回复消息到chunkserver
void ChunkServer::SendResponse(MetaRequest *op)
{
	ostringstream os;

	op->response(os);

	if (os.str().length() > 0)
		mNetConnection->Write(os.str().c_str(), os.str().length());
}
