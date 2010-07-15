//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: MetaServerSM.h 71 2008-07-07 15:49:14Z sriramsrao $
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
// \file MetaServerSM.h
// \brief State machine that interfaces with the meta server and
// handles the RPCs sent by the meta server.
//
//----------------------------------------------------------------------------

#ifndef CHUNKSERVER_METASERVERSM_H
#define CHUNKSERVER_METASERVERSM_H

#include "libkfsIO/KfsCallbackObj.h"
#include "libkfsIO/ITimeout.h"
#include "libkfsIO/NetConnection.h"
#include "KfsOps.h"

#include "meta/queue.h"

#include <list>
#include <string>

namespace KFS
{

class MetaServerSMTimeoutImpl;

/// 用于管理同meta server的操作交互
class MetaServerSM: public KfsCallbackObj
{
public:
	MetaServerSM();
	~MetaServerSM();

	void SetMetaInfo(const ServerLocation &metaLoc, const char *clusterKey,
			int rackId);
	/// Init function for configuring the metaserver SM.
	/// @param[in] chunkServerPort  Port at which chunk-server is
	/// listening for connections from KFS clients.
	/// @retval 0 if we could connect/send HELLO; -1 otherwise
	void Init(int chunkServerPort);

	/// Send HELLO message.  This sends an op down to the event
	/// processor to get all the info.
	int SendHello();

	/// Generic event handler to handle RPC requests sent by the meta server.
	int HandleRequest(int code, void *data);

	bool HandleMsg(IOBuffer *iobuf, int msgLen);

	void EnqueueOp(KfsOp *op);

	/// If the connection to the server breaks, periodically, retry to
	/// connect; also dispatch ops.
	void Timeout();

	/// Return the server name/port information
	ServerLocation GetLocation() const
	{
		return mLocation;
	}

	kfsSeq_t nextSeq()
	{
		return mCmdSeq++;
	}

private:
	kfsSeq_t mCmdSeq;

	/// 服务器的物理地址（IP:port）
	/// where is the server located?
	ServerLocation mLocation;

	/// 服务器所在的机架
	/// An id that specifies the rack on which the server is located;
	/// this is used to do rack-aware replication
	int mRackId;

	/// "shared secret" key for this cluster.  This is used to prevent
	/// config mishaps: When we connect to a metaserver, we have to
	/// agree on the cluster key.
	/// 密钥
	std::string mClusterKey;

	/// the port that the metaserver tells the clients to connect to us at.
	/// metaserver告诉clients，他们需要使用哪一个端口对该chunk server进行连接
	int mChunkServerPort;

	/// Track if we have sent a "HELLO" to metaserver
	/// 是否已经向metaserver发送了“HELLO”消息
	bool mSentHello;

	/// a handle to the hello op.  The model: the network processor
	/// queues the hello op to the event processor; the event
	/// processor pulls the result and enqueues the op back to us; the
	/// network processor dispatches the op and gets rid of it.
	/// 向服务器发送请求的op
	HelloMetaOp *mHelloOp;

	/// list of ops that need to be dispatched: this is the queue that
	/// is shared between the event processor and the network
	/// dispatcher.  When the network dispatcher runs, it pulls ops
	/// from this queue and stashes them away in the dispatched list.
	/// 等待发送的Meta请求
	MetaQueue<KfsOp> mPendingOps;

	/// same deal: queue that is shared between event processor and
	/// the network dispatcher.  when the ops are dispatched, they are
	/// deleted (i.e., fire'n'forget).
	/// 等待的响应
	MetaQueue<KfsOp> mPendingResponses;

	/// ops that we have sent to metaserver and are waiting for reply.
	/// 已经发送到服务器的请求。这里边的所有请求都在等待服务器的回复
	std::list<KfsOp *> mDispatchedOps;

	/// Our connection to the meta server.
	/// 连接meta server的netConnection
	NetConnectionPtr mNetConnection;

	/// A timer to periodically check that the connection to the
	/// server is good; if the connection broke, reconnect and do the
	/// handshake again.  Also, we use the timeout to dispatch pending
	/// messages to the server.
	/// 定时器，其功能包括：监视与metaserver的连接是否正常；定时向metaserver发送pending
	/// 队列里边的请求
	MetaServerSMTimeoutImpl *mTimer;

	/// Connect to the meta server
	/// @retval 0 if connect was successful; -1 otherwise
	/// 建立于metaserver的连接
	int Connect();

	/// Given a (possibly) complete op in a buffer, run it.
	bool HandleCmd(IOBuffer *iobuf, int cmdLen);
	/// Handle a reply to an RPC we previously sent.
	void HandleReply(IOBuffer *iobuf, int msgLen);

	/// Op has finished execution.  Send a response to the meta
	/// server.
	void EnqueueResponse(KfsOp *op);

	/// This is special: we dispatch mHelloOp and get rid of it.
	void DispatchHello();

	/// Submit all the enqueued ops
	void DispatchOps();
	/// Send out all the responses that are ready
	void DispatchResponse();

	/// We reconnected to the metaserver; so, resend all the pending ops.
	void ResubmitOps();
};

/// A Timeout interface object for checking connection status with the server
/// 用于检测对于server的连接是否正常的timeout类
class MetaServerSMTimeoutImpl: public ITimeout
{
public:
	MetaServerSMTimeoutImpl(MetaServerSM *mgr)
	{
		mMetaServerSM = mgr;
	}
	;
	/// On each timeout, check that the connection with the server is
	/// good.  Also, dispatch any pending messages.
	void Timeout()
	{
		mMetaServerSM->Timeout();
	}
	;
private:
	/// Owning metaserver SM.
	/// 拥有的metaserver的SM
	MetaServerSM *mMetaServerSM;
};

extern MetaServerSM gMetaServerSM;

}

#endif // CHUNKSERVER_METASERVERSM_H
