//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: DiskConnection.cc 71 2008-07-07 15:49:14Z sriramsrao $
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

#include "DiskConnection.h"
#include "DiskManager.h"
#include "Counter.h"
#include "Globals.h"

using std::deque;
using std::string;
using std::list;
using std::min;

using namespace KFS;
using namespace KFS::libkfsio;

/// String description of the enum's in DiskEvent_t
static const char *gDiskEventStr[] =
{ "OP_NONE", "OP_READ", "OP_WRITE", "OP_SYNC" };

const char* DiskEvent_t::ToString()
{
	return gDiskEventStr[op];
}

DiskConnection::DiskConnection(FileHandlePtr &handle,
        KfsCallbackObj *callbackObj)
{
	// KFS_LOG_VA_DEBUG("Allocating connection 0x%p", this);

	mHandle = handle;
	mCallbackObj = callbackObj;
}

DiskConnection::~DiskConnection()
{
	// KFS_LOG_VA_DEBUG("Freeing connection 0x%p", this);
	// Cancel out the events
	Close();
	mDiskIO.clear();

	// A magic number~~
	mCallbackObj = (KfsCallbackObj *) 0xfeedface;
}

void DiskConnection::Close()
{
	deque<DiskIORequest>::iterator ioIter;
	list<DiskEventPtr>::iterator eventIter;
	DiskIORequest r;
	DiskEventPtr event;

	// 清空每一个磁盘请求中的所有磁盘事件
	for (ioIter = mDiskIO.begin(); ioIter != mDiskIO.end(); ++ioIter)
	{
		r = *ioIter;
		for (eventIter = r.diskEvents.begin(); eventIter != r.diskEvents.end(); ++eventIter)
		{
			event = *eventIter;
			event->Cancel(mHandle->mFd);
		}
	}
}

///
/// Schedule a read on the connection by sending a request to the
/// DiskManager.  Returns the # of bytes for which read was
/// successfully scheduled; -1 if there was an error.
///
ssize_t DiskConnection::Read(off_t offset, size_t numBytes)
{
	// 一个char类型的数组
	IOBufferDataPtr data;
	DiskEventPtr event;
	const size_t defPerRead = 65536 * 8;
	int res;
	size_t bytesPerRead, nRead;
	DiskIORequest r(OP_READ, offset, numBytes);

	assert(mCallbackObj != NULL);

	if (numBytes == 0)
		return 0;

	for (nRead = 0; nRead < numBytes; nRead += bytesPerRead)
	{
		bytesPerRead = min(defPerRead, numBytes - nRead);

		if (bytesPerRead <= 0)
			break;

		data.reset(new IOBufferData(bytesPerRead));

		// event返回事件，存储在Request中
		res = globals().diskManager.Read(this, mHandle->mFd, data, offset,
		        bytesPerRead, event);
		if (res < 0)
		{
			return nRead > 0 ? nRead : (ssize_t) -1;
		}

		offset += bytesPerRead;
		// 添加到全局数据中DiskEvents的列表中
		r.diskEvents.push_back(event);
	}

	/*
	 KFS_LOG_VA_DEBUG("# of reads queued for (off = %lld, size = %zd): %d",
	 r.offset, r.numBytes, r.diskEvents.size());
	 */

	// 添加到磁盘请求队列
	mDiskIO.push_back(r);

	return nRead;
}

///
/// DiskManager calls back when read is done.  Now, since AIO is used
/// for reads, the reads can complete in any order.  However, to the
/// KfsCallbackObj, present the results to match the order in which they
/// were issued.
///
// 读文件任务完成时的回调函数，res表示IO完成状态
// 确认已经完成的
int DiskConnection::ReadDone(DiskEventPtr &doneEvent, int res)
{
	deque<DiskIORequest>::iterator ioIter;
	list<DiskEventPtr>::iterator eventIter;
	DiskIORequest r;
	DiskEventPtr event;
	IOBuffer *ioBuf;
	int retval;
	bool found;

	if (res != 0)
	{
		KFS_LOG_VA_DEBUG("Read failure: errno = %d",
				res);
	}
	assert(mCallbackObj != NULL);

	// find the request that issued the event
	found = false;

	// 如果有一个DiskIORequest的一个DiskEvent事件是“已完成”事件，则found为真
	for (ioIter = mDiskIO.begin(); ioIter != mDiskIO.end(); ++ioIter)
	{
		r = *ioIter;
		for (eventIter = r.diskEvents.begin(); eventIter != r.diskEvents.end(); ++eventIter)
		{
			event = *eventIter;
			if (event == doneEvent)
			{
				found = true;
				break;
			}
		}
		if (found)
			break;
	}

	if (!found)
		return 0;

	// 到这里说明在所有的任务中，至少有一个任务已经完成，则开始进行下面的统计工作

	// If the READ on the head of the queue is done, notify the
	// associated KfsCallbackObj.
	// NOTE: On a given disk connection, we can only do one: read or
	// write, not both.  So, when we call back the KfsCallbackObj, we
	// are guaranteed to be only doing reads.
	//
	// 从队首开始，对已经完成的DiskIO进行统计
	while (!mDiskIO.empty())
	{
		r = mDiskIO.front();

		// 如果有一个diskEvent未完成，则整个任务未完成，返回0
		for (eventIter = r.diskEvents.begin(); eventIter != r.diskEvents.end(); ++eventIter)
		{
			event = *eventIter;
			if (event->status != EVENT_DONE)
			{
				return 0;
			}
		}
		assert(r.op == OP_READ);

		// the request is all done
		// 此时，r所指向的所有的任务都已经完成了，弹出DiskIO的队首元素，并进行统计计数（读取字节数目）
		mDiskIO.pop_front();

		// unlike writes which are a "binary" operation, for reads we
		// are little lenient: we return as much data as possible;
		// that is, of the sub-requests that make up a read request,
		// we return the result of the read until the first failure;
		// obviously, if there are no failures, we return everything.
		// so, a client can ask for N bytes and will get at most N bytes.
		//
		ioBuf = new IOBuffer();
		retval = 0;

		for (eventIter = r.diskEvents.begin(); eventIter != r.diskEvents.end(); ++eventIter)
		{
			event = *eventIter;
			if (event->retval >= 0)
			{
				retval = event->retval;
				ioBuf->Append(event->data);
			}
			else
			{
				retval = event->retval;
				break;
			}
		}

		// 计算已经读写的字节数目
		if (retval >= 0)
		{
			globals().ctrDiskBytesRead.Update(ioBuf->BytesConsumable());
			mCallbackObj->HandleEvent(EVENT_DISK_READ, (void *) ioBuf);
		}
		else
		{	// 因为前一步已经确认所有任务都已经完成，若有任务没有完成，一定是出现了错误，
			// 则错误计数器加一
			globals().ctrDiskIOErrors.Update(1);
			mCallbackObj->HandleEvent(EVENT_DISK_ERROR, NULL);
		}

		delete ioBuf;
	}

	return 0;
}

///
/// Analogous to read.
///
ssize_t DiskConnection::Write(off_t offset, size_t numBytes, IOBuffer *buf)
{
	list<IOBufferDataPtr>::iterator iter;
	IOBufferDataPtr data;
	DiskEventPtr event;
	int res;
	size_t nWrote = 0, numIO;
	DiskIORequest r(OP_WRITE, offset, numBytes);

	assert(mCallbackObj != NULL);

	if (numBytes == 0)
		return 0;

	// schedule each blob as a separate write
	for (iter = buf->mBuf.begin(); iter != buf->mBuf.end(); iter++)
	{
		data = *iter;
		numIO = data->BytesConsumable();
		if (numIO == 0)
			continue;

		if (numIO + nWrote > numBytes)
			numIO = numBytes - nWrote;

		// 创建异步IO，并不进行IO操作
		res = globals().diskManager.Write(this, mHandle->mFd, data, offset,
		        numIO, event);
		if (res < 0)
		{
			return nWrote > 0 ? nWrote : (ssize_t) -1;
		}

		offset += numIO;
		nWrote += numIO;
		r.diskEvents.push_back(event);
		if (nWrote >= numBytes)
			break;
	}

	mDiskIO.push_back(r);

	/*
	 KFS_LOG_VA_DEBUG("# of writes queued for (off = %lld, size = %lld): %d",
	 r.offset, r.numBytes, r.diskEvents.size());

	 KFS_LOG_VA_DEBUG("# of elements in write list: %d",
	 mDiskIO.size());
	 */

	return nWrote;
}

///
/// Analogous to ReadDone
///
int DiskConnection::WriteDone(DiskEventPtr &doneEvent, int res)
{
	deque<DiskIORequest>::iterator ioIter;
	list<DiskEventPtr>::iterator eventIter;
	DiskIORequest r;
	DiskEventPtr event;
	bool found = true, success = true;
	int retval = 0;

	if (res != 0)
	{
		KFS_LOG_VA_DEBUG("Write failure: errno = %d", res);
	}

	// find the request that issued the event
	found = false;
	for (ioIter = mDiskIO.begin(); ioIter != mDiskIO.end(); ++ioIter)
	{
		r = *ioIter;
		for (eventIter = r.diskEvents.begin(); eventIter != r.diskEvents.end(); ++eventIter)
		{
			event = *eventIter;
			if (event == doneEvent)
			{
				found = true;
				break;
			}
		}
		if (found)
			break;
	}

	// assert(found);
	if (!found)
		return 0;

	// If the WRITE on the head of the queue is done, notify the
	// associated KfsCallbackObj.
	// NOTE: On a given disk connection, we can only do one: read or
	// write, not both.  So, when we call back the KfsCallbackObj, we
	// are guaranteed to be only doing writes
	//
	while (!mDiskIO.empty())
	{
		r = mDiskIO.front();

		assert((r.op == OP_WRITE) || (r.op == OP_SYNC));

		if (r.op != OP_WRITE)
		{
			// we could've have a sync op
			break;
		}
		for (eventIter = r.diskEvents.begin(); eventIter != r.diskEvents.end(); ++eventIter)
		{
			event = *eventIter;
			if (event->status != EVENT_DONE)
			{
				return 0;
			}
		}

		// the request is all done
		mDiskIO.pop_front();

		//
		// we treat a write as a binary operation: all the
		// sub-writes that make up the write must succeed (and hence the
		// write request is a sucess); if any one fails, we fail the
		// write operation.
		//
		success = true;
		retval = 0;
		for (eventIter = r.diskEvents.begin(); eventIter != r.diskEvents.end(); ++eventIter)
		{
			event = *eventIter;
			if (event->retval < 0)
			{
				success = false;
				break;
			}
			retval += event->retval;
		}

		if (success)
		{
			globals().ctrDiskBytesWritten.Update(retval);
			mCallbackObj->HandleEvent(EVENT_DISK_WROTE, &retval);
		}
		else
		{
			globals().ctrDiskIOErrors.Update(1);
			mCallbackObj->HandleEvent(EVENT_DISK_ERROR, NULL);
		}
	}

	return 0;
}

///
/// Schedule an asynchronous sync on the underlying fd by sending a
/// request to the DiskManager.
///
int DiskConnection::Sync(bool notifyDone)
{
	int res;
	DiskEventPtr event;
	DiskIORequest r(OP_SYNC, 0, 0);

	res = globals().diskManager.Sync(this, mHandle->mFd, event);
	if (res < 0)
	{
		// XXX: we have a problem...
		return res;
	}
	event->notifyDone = notifyDone;
	r.diskEvents.push_back(event);
	mDiskIO.push_back(r);

	return res;
}

///
/// DiskManager calls back with a sync done.  So, callback the
/// associated KfsCallbackObj and let it know that the sync finished.
///
int DiskConnection::SyncDone(DiskEventPtr &doneEvent, int res)
{
	DiskIORequest r;
	DiskEventPtr event;

	assert(!mDiskIO.empty());
	// sync better be the head of the list.  reads/writes can return
	// in any order, but when we submit a sync, all the preceding
	// writes must've finished.
	r = mDiskIO.front();
	assert(r.op == OP_SYNC);

	assert(!r.diskEvents.empty());
	event = r.diskEvents.front();
	assert(doneEvent == event);
	mDiskIO.pop_front();

	if (event->notifyDone)
		mCallbackObj->HandleEvent(EVENT_SYNC_DONE, NULL);
	return 0;
}
