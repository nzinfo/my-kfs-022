//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: Replicator.h 84 2008-07-15 04:39:02Z sriramsrao $
//
// Created 2007/01/17
// Author: Sriram Rao
//
// Copyright 2008 Quantcast Corp.
// Copyright 2007-2008 Kosmix Corp.
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
// \brief Code to deal with (re)replicating a chunk.
//----------------------------------------------------------------------------

#ifndef CHUNKSERVER_REPLICATOR_H
#define CHUNKSERVER_REPLICATOR_H

#include "libkfsIO/KfsCallbackObj.h"
#include "libkfsIO/NetConnection.h"
#include "KfsOps.h"
#include "RemoteSyncSM.h"

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

namespace KFS
{

class Replicator: public KfsCallbackObj, public boost::enable_shared_from_this<
		Replicator>
{
public:
	// Model for doing a chunk replication involves 3 steps:
	//  - First, figure out the size of the chunk.
	//  - Second in a loop:
	//        - read N bytes from the source
	//        - write N bytes to disk
	// - Third, notify the metaserver of the status (0 to mean
	// success, -1 on failure).
	//
	// Implementing the above model given the thread setup is done as
	// follows:
	// 1. The event thread triggers the creation of a Replicator
	// object
	// 2. All the network I/O is done via the network thread.  This
	// accomplished by setting up a timer handler which does the
	// network dispatching.
	// 3. When we read data from the peer, that comes in via the
	// network thread; we then submit a write op to the event thread
	// to get the data written out.
	//
	// During replication, the chunk isn't part of the chunkTable data
	// structure that is maintained locally.  This is done for
	// simplifying failure handling: if we die in the midst of
	// replication, upon restart, we will find a "zombie" chunk---a
	// chunk with nothing pointing to it; this chunk will get nuked.
	// So, at the end of a successful replication, we update the
	// chunkTable data structure and the subsequent checkpoint will
	// get the chunk info. logged.  Note that, when we do the writes
	// to disk, we are logging the writes; we are, however,
	// intentionally not logging the presence of the chunk until the
	// replication is complete.
	//
	Replicator(ReplicateChunkOp *op);
	~Replicator();
	// Start by sending out a size request
	/// 启动Replicator: 设置远程peer，创建一个mChunkMetadataOp操作，并将这个操作发送给
	/// 指定的mPeer
	void Start(RemoteSyncSMPtr &peer);
	// Handle the callback for a size request
	/// 启动读chunk数据的操作，完成mChunkSize, mChunkVersion, mReadOp, mWriteOp的构造之后
	/// 通过Read()读入数据
	int HandleStartDone(int code, void *data);
	// Handle the callback for a remote read request
	/// 处理一次读完成（1MB的数据）事件
	int HandleReadDone(int code, void *data);
	// Handle the callback for a write
	/// 处理一次写完成事件（offset的指针在这里修改）
	int HandleWriteDone(int code, void *data);
	// When replication done, we write out chunk meta-data; this is
	// the handler that gets called when this event is done.
	/// chunk复制完成时进行的处理：调用发起块复制操作的用户的HandleEvent函数
	int HandleReplicationDone(int code, void *data);
	// Cleanup...
	/// 终止拷贝操作：如果已经完成或者未完成都要终止，但处理方法不同
	void Terminate();

private:

	/// Inputs from the metaserver
	/// 复制操作是metaserver发出的命令，需要指定一些参数：KFS文件ID，chunk ID和
	/// chunk version
	kfsFileId_t mFileId;
	kfsChunkId_t mChunkId;
	kfsSeq_t mChunkVersion;

	// What we obtain from the src from where we download the chunk.
	/// 我们从数据源中读入的信息
	size_t mChunkSize;
	// The op that triggered this replication operation.
	/// 引发块复制操作的操作
	ReplicateChunkOp *mOwner;
	// Are we done yet?
	bool mDone;
	// What is the offset we are currently reading at
	off_t mOffset;

	// Handle to the peer from where we have to get data
	/// 我们要获取数据的主机句柄
	RemoteSyncSMPtr mPeer;

	GetChunkMetadataOp mChunkMetadataOp;
	ReadOp mReadOp;
	WriteOp mWriteOp;

	// Send out a read request to the peer
	void Read();

};

typedef boost::shared_ptr<Replicator> ReplicatorPtr;

}

#endif // CHUNKSERVER_REPLICATOR_H
