//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: RemoteSyncSM.h 71 2008-07-07 15:49:14Z sriramsrao $
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

#ifndef CHUNKSERVER_REMOTESYNCSM_H
#define CHUNKSERVER_REMOTESYNCSM_H

#include <list>

#include "libkfsIO/KfsCallbackObj.h"
#include "libkfsIO/DiskConnection.h"
#include "libkfsIO/NetConnection.h"
#include "KfsOps.h"
#include "Chunk.h"
#include "libkfsIO/ITimeout.h"
#include "meta/queue.h"

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

namespace KFS
{

class RemoteSyncSMTimeoutImpl;

// State machine that asks a remote server to commit a write
/// 远端同步：处理收到的服务器的回复信息
class RemoteSyncSM: public KfsCallbackObj,
		public boost::enable_shared_from_this<RemoteSyncSM>
{
public:

	RemoteSyncSM(const ServerLocation &location) :
		mLocation(location), mSeqnum(1), mTimer(NULL),
		mKillRemoteSyncOp(1, this)
	{
	}
	;

	~RemoteSyncSM();

	/// 创建同mLocation指定的服务器的连接；如果成功，则返回true
	bool Connect();

	kfsSeq_t NextSeqnum()
	{
		return mSeqnum++;
	}

	/// 将op加入到mDispatchedOps队列中等待处理，如果这个op不是转发操作；否则，直接回复给client
	/// 并同步mNetConnection中的数据到socket
	void Enqueue(KfsOp *op);

	/// 关闭本RemoteSyncSM，撤销其中的所有操作，并从gChunkServer移除该RemoteSyncSM
	void Finish();

	// void Dispatch();

	/// 处理发送的数据
	int HandleEvent(int code, void *data);

	ServerLocation GetLocation() const
	{
		return mLocation;
	}

private:
	/// 用于连接mLocation指定的服务器
	NetConnectionPtr mNetConnection;

	/// 指定远端服务器
	ServerLocation mLocation;

	/// Assign a sequence # for each op we send to the remote server
	kfsSeq_t mSeqnum;

	/// A timer to periodically dispatch pending
	/// messages to the remote server
	/// 定时向远端服务器发送消息的定时器
	RemoteSyncSMTimeoutImpl *mTimer;

	KillRemoteSyncOp mKillRemoteSyncOp;

	/// Queue of outstanding ops to be dispatched: this is the queue
	/// shared between event processor and the network threads.  When
	/// the network dispatcher runs, it pulls messages from this queue
	/// and stashes them away in the dispatched ops list.
	/// 尚未发送的服务器请求，等待发送
	MetaQueue<KfsOp> mPendingOps;

	/// Queue of outstanding ops sent to remote server.
	/// 已经发送到远程server中，等待server回复信息的操作队列
	std::list<KfsOp *> mDispatchedOps;

	/// We (may) have got a response from the peer.  If we are doing
	/// re-replication, then we need to wait until we got all the data
	/// for the op; in such cases, we need to know if we got the full
	/// response.
	/// @retval 0 if we got the response; -1 if we need to wait
	/// 处理收到的服务器相应
	int HandleResponse(IOBuffer *iobuf, int cmdLen);
	void FailAllOps();
};

/// A Timeout interface object for dispatching messages.
/// 用于发送消息的timeout类
/// @note 该类不能脱离RemoteSyncSM而存在
class RemoteSyncSMTimeoutImpl: public ITimeout
{
public:
	RemoteSyncSMTimeoutImpl(RemoteSyncSM *mgr)
	{
		mRemoteSyncSM = mgr;
	}
	;
	/// On each timeout, check that the connection with the server is
	/// good.  Also, dispatch any pending messages.
	void Timeout()
	{
		// mRemoteSyncSM->Dispatch();
	}
	;
private:
	/// Owning remote-sync SM.
	RemoteSyncSM *mRemoteSyncSM;
};

typedef boost::shared_ptr<RemoteSyncSM> RemoteSyncSMPtr;

}

#endif // CHUNKSERVER_REMOTESYNCSM_H
