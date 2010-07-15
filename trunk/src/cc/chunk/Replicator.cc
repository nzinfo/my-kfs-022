//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: Replicator.cc 84 2008-07-15 04:39:02Z sriramsrao $
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
// \brief Code for dealing with a replicating a chunk.  The metaserver
//asks a destination chunkserver to obtain a copy of a chunk from a source
//chunkserver; in response, the destination chunkserver pulls the data
//down and writes it out to disk.  At the end replication, the
//destination chunkserver notifies the metaserver.
//
//----------------------------------------------------------------------------

#include "Replicator.h"
#include "ChunkServer.h"
#include "Utils.h"
#include "libkfsIO/Globals.h"
#include "libkfsIO/Checksum.h"

#include <string>
#include <sstream>

#include "common/log.h"
#include <boost/scoped_array.hpp>
using boost::scoped_array;

using std::string;
using std::ostringstream;
using std::istringstream;
using namespace KFS;
using namespace KFS::libkfsio;

Replicator::Replicator(ReplicateChunkOp *op) :
	mFileId(op->fid), mChunkId(op->chunkId), mChunkVersion(op->chunkVersion),
			mOwner(op), mDone(false), mOffset(0), mChunkMetadataOp(0), mReadOp(
					0), mWriteOp(op->chunkId, op->chunkVersion)
{
	mReadOp.chunkId = op->chunkId;
	mReadOp.chunkVersion = op->chunkVersion;
	mReadOp.clnt = this;
	mWriteOp.clnt = this;
	mChunkMetadataOp.clnt = this;
	mWriteOp.Reset();
	mWriteOp.isFromReReplication = true;
	SET_HANDLER(&mReadOp, &ReadOp::HandleReplicatorDone);
}

Replicator::~Replicator()
{
}

/// 启动Replicator: 设置远程peer，创建一个mChunkMetadataOp操作
void Replicator::Start(RemoteSyncSMPtr &peer)
{
#ifdef DEBUG
	verifyExecutingOnEventProcessor();
#endif

	mPeer = peer;

	mChunkMetadataOp.seq = mPeer->NextSeqnum();
	mChunkMetadataOp.chunkId = mChunkId;

	/// 设置超时处理程序
	SET_HANDLER(this, &Replicator::HandleStartDone);

	/// 执行读全局数据的操作
	mPeer->Enqueue(&mChunkMetadataOp);
}

/// 启动读chunk数据的操作，完成mChunkSize, mChunkVersion, mReadOp, mWriteOp的构造之后
/// 通过Read()读入数据
int Replicator::HandleStartDone(int code, void *data)
{
#ifdef DEBUG
	verifyExecutingOnEventProcessor();
#endif

	if (mChunkMetadataOp.status < 0)
	{
		Terminate();
		return 0;
	}

	// 需要从数据源读出
	mChunkSize = mChunkMetadataOp.chunkSize;
	// 由命令发出者(metaserver)指定
	mChunkVersion = mChunkMetadataOp.chunkVersion;

	mReadOp.chunkVersion = mWriteOp.chunkVersion = mChunkVersion;

	// set the version to a value that will never be used; if
	// replication is successful, we then bump up the counter.
	// 在本地分配指定的chunk：在不同的chunk服务器上可以有相同的chunk
	if (gChunkManager.AllocChunk(mFileId, mChunkId, 0, true) < 0)
	{
		Terminate();
		return -1;
	}

	Read();
	return 0;
}

/*
 * 1. offset的指针在HandleWriteDone()中修改;
 * 2. 每次只能读入1MB的数据，通过递归的方式完成整个chunk的复制
 * 3. 每次写磁盘操作中，如果要写入磁盘的数据量大于1个校验块大小，则将数据按校验块对齐
 */
/// 调用关系如下：Read() -> HandleReadDone() -> HandleWriteDone() -> Read() -> ...
/// 直到在Read中mOffset >= (off_t) mChunkSize
void Replicator::Read()
{
	ReplicatorPtr self = shared_from_this();

#ifdef DEBUG
	verifyExecutingOnEventProcessor();
#endif

	// 如果当前读入数据的位置已经在chunk大小的范围以外了，则说明读数据已经完成了，则可以终止
	// 拷贝操作了
	if (mOffset >= (off_t) mChunkSize)
	{
		mDone = true;
		Terminate();
		return;
	}

	/// 每一次操作读入1MB的数据
	mReadOp.seq = mPeer->NextSeqnum();
	mReadOp.status = 0;
	mReadOp.offset = mOffset;
	// read an MB
	mReadOp.numBytes = 1 << 20;
	mPeer->Enqueue(&mReadOp);

	/// 将数据写入本地磁盘
	SET_HANDLER(this, &Replicator::HandleReadDone);
}

/// 处理一次读完成（1MB的数据）事件
int Replicator::HandleReadDone(int code, void *data)
{

#ifdef DEBUG
	verifyExecutingOnEventProcessor();
#endif

	if (mReadOp.status < 0)
	{
		Terminate();
		return 0;
	}

	delete mWriteOp.dataBuf;

	mWriteOp.Reset();

	/// 写入到write缓存中
	mWriteOp.dataBuf = new IOBuffer();
	mWriteOp.numBytes = mReadOp.dataBuf->BytesConsumable();
	mWriteOp.dataBuf->Move(mReadOp.dataBuf, mWriteOp.numBytes);
	mWriteOp.offset = mOffset;
	mWriteOp.isFromReReplication = true;

	// align the writes to checksum boundaries
	// 每次写数据时，如果数据量大于一个校验块的大小，则将数据对其到校验块的边缘部分
	if ((mWriteOp.numBytes >= CHECKSUM_BLOCKSIZE) && (mWriteOp.numBytes
			% CHECKSUM_BLOCKSIZE) != 0)
		// round-down so to speak; whatever is left will be picked up by the next read
		mWriteOp.numBytes = (mWriteOp.numBytes / CHECKSUM_BLOCKSIZE)
				* CHECKSUM_BLOCKSIZE;

	SET_HANDLER(this, &Replicator::HandleWriteDone);

	if (gChunkManager.WriteChunk(&mWriteOp) < 0)
	{
		// abort everything
		Terminate();
		return -1;
	}
	return 0;
}

/// 处理一次写完成事件（offset的指针在这里修改）
int Replicator::HandleWriteDone(int code, void *data)
{
	ReplicatorPtr self = shared_from_this();

#ifdef DEBUG
	verifyExecutingOnEventProcessor();
#endif

	assert((code == EVENT_CMD_DONE) || (code == EVENT_DISK_WROTE));

	if (mWriteOp.status < 0)
	{
		Terminate();
		return 0;
	}

	mOffset += mWriteOp.numBytesIO;

	Read();
	return 0;
}

/// 终止拷贝操作：如果已经完成或者未完成都要终止，但处理方法不同
void Replicator::Terminate()
{
#ifdef DEBUG
	verifyExecutingOnEventProcessor();
#endif


	if (mDone)
	{// 如果已经完成了克隆操作的话
		KFS_LOG_VA_INFO("Replication for %ld finished", mChunkId);

		// now that replication is all done, set the version appropriately
		// 设置刚完成拷贝的chunk的版本号
		gChunkManager.ChangeChunkVers(mFileId, mChunkId, mChunkVersion);

		SET_HANDLER(this, &Replicator::HandleReplicationDone);

		// 完成对应的克隆操作：标记mIsChunkTableDirty为true，isBeingReplicated为false
		gChunkManager.ReplicationDone(mChunkId);

		// 将chunk的全局数据写入chunk对应的Linux文件中
		int res = gChunkManager.WriteChunkMetadata(mChunkId, &mWriteOp);
		if (res == 0)
			return;
	}

	// 程序运行到此处说明chunk拷贝错误，删除已经拷贝了一部分的数据文件
	KFS_LOG_VA_INFO("Replication for %ld failed...cleaning up", mChunkId);
	gChunkManager.DeleteChunk(mChunkId);
	mOwner->status = -1;

	// Notify the owner of completion
	/// 通知事件的发起者已经完成操作
	mOwner->HandleEvent(EVENT_CMD_DONE, NULL);
}

// logging of the chunk meta data finished; we are all done
// chunk复制完成时进行的处理：调用发起块复制操作的用户的HandleEvent函数
int Replicator::HandleReplicationDone(int code, void *data)
{
	mOwner->status = 0;
	// Notify the owner of completion
	mOwner->HandleEvent(EVENT_CMD_DONE, (void *) &mChunkVersion);
	return 0;
}

