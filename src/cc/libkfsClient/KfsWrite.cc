//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: KfsWrite.cc 163 2008-09-21 20:06:37Z sriramsrao $
//
// Created 2006/10/02
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
// All the code to deal with writes from the client.
//----------------------------------------------------------------------------

#include "KfsClient.h"
#include "KfsClientInt.h"
#include "common/properties.h"
#include "common/log.h"
#include "meta/kfstypes.h"
#include "libkfsIO/Checksum.h"
#include "Utils.h"

#include <cerrno>
#include <iostream>
#include <string>

using std::string;
using std::ostringstream;
using std::istringstream;
using std::min;
using std::max;

using std::cout;
using std::endl;

using namespace KFS;

static bool NeedToRetryWrite(int status)
{
	return ((status == -EHOSTUNREACH) || (status == -ETIMEDOUT) || (status
			== -KFS::EBADCKSUM) || (status == -KFS::ESERVERBUSY));
}

static bool NeedToRetryAllocation(int status)
{
	return ((status == -EHOSTUNREACH) || (status == -ETIMEDOUT) || (status
			== -EBUSY) || (status == -KFS::EBADVERS) || (status
			== -KFS::EALLOCFAILED));
}

///
/// @brief 将numBytes的数据写入chunkserver
/// @note 不需要为数据分块，WriteToServer函数已经完成分块工作了
///
ssize_t KfsClientImpl::Write(int fd, const char *buf, size_t numBytes)
{
	MutexLock l(&mMutex);

	size_t nwrote = 0;
	ssize_t numIO = 0;
	int res;

	if (!valid_fd(fd) || mFileTable[fd] == NULL || mFileTable[fd]->openMode
			== O_RDONLY)
	{
		KFS_LOG_VA_INFO("Write to fd: %d failed---fd is likely closed", fd);
		return -EBADF;
	}
	FileAttr *fa = FdAttr(fd);
	if (fa->isDirectory)
		return -EISDIR;

	FilePosition *pos = FdPos(fd);
	//
	// Loop thru chunk after chunk until we write the desired #
	// of bytes.
	// 写数据到服务器，由WriteToServer决定，每次最多写入一个块
	while (nwrote < numBytes)
	{

		size_t nleft = numBytes - nwrote;

		// Don't need this: if we don't have the lease, don't
		// know where the chunk is, allocation will get that info.
		// LocateChunk(fd, pos->chunkNum);

		// need to retry here...
		// 为数据分配存储块，如果需要的话
		if ((res = DoAllocation(fd)) < 0)
		{
			// allocation failed...bail
			break;
		}

		// 指定默认chunkserver
		if (pos->preferredServer == NULL)
		{
			numIO = OpenChunk(fd);
			if (numIO < 0)
			{
				// KFS_LOG_VA_DEBUG("OpenChunk(%lld)", numIO);
				break;
			}
		}

		if (nleft < ChunkBuffer::BUF_SIZE || FdBuffer(fd)->dirty)
		{
			// either the write is small or there is some dirty
			// data...so, aggregate as much as possible and then it'll
			// get flushed
			numIO = WriteToBuffer(fd, buf + nwrote, nleft);
		}
		else
		{
			// write is big and there is nothing dirty...so,
			// write-thru
			numIO = WriteToServer(fd, pos->chunkOffset, buf + nwrote, nleft);
		}

		if (numIO < 0)
		{
			string errstr = ErrorCodeToStr(numIO);
			// KFS_LOG_VA_DEBUG("WriteToXXX:%s", errstr.c_str());
			break;
		}

		nwrote += numIO;
		numIO = Seek(fd, numIO, SEEK_CUR);
		if (numIO < 0)
		{
			// KFS_LOG_VA_DEBUG("Seek(%lld)", numIO);
			break;
		}
	}

	if (nwrote == 0 && numIO < 0)
		return numIO;

	if (nwrote != numBytes)
	{
		KFS_LOG_VA_DEBUG("----Write done: asked: %llu, got: %llu-----",
				numBytes, nwrote);
	}
	return nwrote;
}

///
/// @brief 将数据写入缓冲区，如果需要则同步一些数据
/// @note 每次最多写满一个缓冲区的数据
///
ssize_t KfsClientImpl::WriteToBuffer(int fd, const char *buf, size_t numBytes)
{
	ssize_t numIO;
	size_t lastByte;
	FilePosition *pos = FdPos(fd);
	ChunkBuffer *cb = FdBuffer(fd);

	// if the buffer has dirty data and this write doesn't abut it,
	// or if buffer is full, flush the buffer before proceeding.
	// XXX: Reconsider buffering to do a read-modify-write of
	// large amounts of data.
	// 如果写入的数据位置与缓冲区位置不是相同的，并且缓冲区数据已经修改过了，这时需要同部服务器
	if (cb->dirty && cb->chunkno != pos->chunkNum)
	{
		int status = FlushBuffer(fd);
		if (status < 0)
			return status;
	}

	// 为buffer分配内存
	cb->allocate();

	off_t start = pos->chunkOffset - cb->start;
	size_t previous = cb->length;

	// 如果缓冲区已满，并且需要同步数据
	if (cb->dirty && ((start != (off_t) previous) || (previous
		== ChunkBuffer::BUF_SIZE)))
	{
		int status = FlushBuffer(fd);
		if (status < 0)
			return status;
	}

	if (!cb->dirty)
	{
		cb->chunkno = pos->chunkNum;
		cb->start = pos->chunkOffset;
		cb->length = 0;
		cb->dirty = true;
	}

	// ensure that write doesn't straddle chunk boundaries
	numBytes = min(numBytes, (size_t)(KFS::CHUNKSIZE - pos->chunkOffset));
	if (numBytes == 0)
		return 0;

	// max I/O we can do
	numIO = min(ChunkBuffer::BUF_SIZE - cb->length, numBytes);
	assert(numIO> 0);

	// KFS_LOG_VA_DEBUG("Buffer absorbs write...%d bytes", numIO);

	// chunkBuf[0] corresponds to some offset in the chunk,
	// which is defined by chunkBufStart.
	// chunkOffset corresponds to the position in the chunk
	// where the "filepointer" is currently at.
	// Figure out where the data we want copied into starts
	start = pos->chunkOffset - cb->start;
	assert(start >= 0 && start < (off_t) ChunkBuffer::BUF_SIZE);
	memcpy(&cb->buf[start], buf, numIO);

	lastByte = start + numIO;

	// update the size according to where the last byte just
	// got written.
	if (lastByte > cb->length)
		cb->length = lastByte;

	return numIO;
}

///
/// @brief 将fd指定的文件的缓存同步到服务器上，并清空该缓存
///
ssize_t KfsClientImpl::FlushBuffer(int fd)
{
	ssize_t numIO = 0;
	ChunkBuffer *cb = FdBuffer(fd);

	if (cb->dirty)
	{
		numIO = WriteToServer(fd, cb->start, cb->buf, cb->length);
		if (numIO >= 0)
		{
			cb->dirty = false;
			// we just flushed the buffer...so, there is no data in it
			cb->length = 0;
		}
	}
	return numIO;
}

///
/// @param [in] fd 文件描述符
/// @param [in] offset 在chunk内的偏移地址
/// @param [in] buf 要写入的数据
/// @param [in] 要传输的数据量
/// @note 每次写入的数据最多有一个块！！这个是由DoLargeWriteToServer这个函数决定的！\n
/// @note 在该版本中，DoLargeWriteToServer和DoSmallWriteToServer是同一个函数
///
ssize_t KfsClientImpl::WriteToServer(int fd, off_t offset, const char *buf,
		size_t numBytes)
{
	assert(KFS::CHUNKSIZE - offset >= 0);

	// 最大可用字节数只可能是给定字节数或者最大字节数中较小的一个
	size_t numAvail = min(numBytes, (size_t)(KFS::CHUNKSIZE - offset));
	int res = 0;

	for (int retryCount = 0; retryCount < NUM_RETRIES_PER_OP; retryCount++)
	{
		// Same as read: align writes to checksum block boundaries
		// 如果写完之后，写入的数据不会跨过一个校验块，则直接写入数据到服务器
		if (offset + numAvail <= OffsetToChecksumBlockEnd(offset))
		{
			res = DoSmallWriteToServer(fd, offset, buf, numBytes);
			KFS_LOG_VA_INFO("Doing small write to server result: %d\n", res);
		}
		else
		{
			struct timeval startTime, endTime;
			double timeTaken;

			gettimeofday(&startTime, NULL);

			res = DoLargeWriteToServer(fd, offset, buf, numBytes);
			KFS_LOG_VA_INFO("Doing large write to server result: %d\n", res);

			gettimeofday(&endTime, NULL);

			timeTaken = (endTime.tv_sec - startTime.tv_sec) + (endTime.tv_usec
					- startTime.tv_usec) * 1e-6;

			if (timeTaken > 5.0)
			{
				KFS_LOG_VA_INFO("Writes thru chain for chunk %lld are taking: %.3f secs",
						GetCurrChunk(fd)->chunkId, timeTaken);
			}

			KFS_LOG_VA_DEBUG("Total Time to write data to server(s): %.4f secs", timeTaken);
		}

		if (res >= 0)
			break;

		if ((res == -EHOSTUNREACH) || (res == -KFS::EBADVERS))
		{
			// one of the hosts is non-reachable (aka dead); so, wait
			// and retry.  Since one of the servers has a write-lease, we
			// need to wait for the lease to expire before retrying.
			Sleep(KFS::LEASE_INTERVAL_SECS);
		}

		if (res == -KFS::ELEASEEXPIRED)
		{
			ChunkAttr *chunk = GetCurrChunk(fd);
			ServerLocation loc = chunk->chunkServerLoc[0];

			KFS_LOG_VA_INFO("Server %s says lease expired for %lld.%lld ...re-doing allocation",
					loc.ToString().c_str(), chunk->chunkId, chunk->chunkVersion);
			Sleep(KFS::LEASE_INTERVAL_SECS / 2);
		}
		if ((res == -EHOSTUNREACH) || (res == -KFS::EBADVERS) || (res
				== -KFS::ELEASEEXPIRED))
		{
			// save the value of res; in case we tried too many times
			// and are giving up, we need the error to propogate
			int r;
			if ((r = DoAllocation(fd, true)) < 0)
				return r;

			continue;
		}

		if (res < 0)
		{
			// any other error
			string errstr = ErrorCodeToStr(res);
			KFS_LOG_VA_INFO("Write failed because of error: %s", errstr.c_str());
			break;
		}
	}

	return res;
}

///
/// @brief 为文件分配存储位置
/// @note 若force为真，则不论该块是否已经分配，都为之分配一个块
///
int KfsClientImpl::DoAllocation(int fd, bool force)
{
	ChunkAttr *chunk = NULL;
	FileAttr *fa = FdAttr(fd);
	int res = 0;

	if (IsCurrChunkAttrKnown(fd))
		chunk = GetCurrChunk(fd);

	// 如果force为真，则强制执行
	if (chunk && force)
		chunk->didAllocation = false;

	if ((chunk == NULL) || (chunk->chunkId == (kfsChunkId_t) -1)
			|| (!chunk->didAllocation))
	{
		// if we couldn't locate the chunk, it must be a new one.
		// also, if it is an existing chunk, force an allocation
		// if needed (which'll cause version # bumps, lease
		// handouts etc).
		for (uint8_t retryCount = 0; retryCount < NUM_RETRIES_PER_OP; retryCount++)
		{
			if (retryCount)
			{
				KFS_LOG_DEBUG("Allocation failed...will retry after a few secs");
				if (res == -EBUSY)
					// the metaserver says it can't get us a lease for
					// the chunk.  so, wait for a lease interval to
					// expire and then retry
					Sleep(KFS::LEASE_INTERVAL_SECS);
				else
					Sleep(RETRY_DELAY_SECS);
			}

			// 为fd分配存储位置
			res = AllocChunk(fd);
			if ((res >= 0) || (!NeedToRetryAllocation(res)))
			{
				break;
			}
			// allocation failed...retry
		}
		if (res < 0)
			// allocation failed...bail
			return res;
		chunk = GetCurrChunk(fd);
		assert(chunk != NULL);
		chunk->didAllocation = true;
		if (force)
		{
			KFS_LOG_VA_DEBUG("Forced allocation version: %lld",
					chunk->chunkVersion);
		}
		// XXX: This is incorrect...you may double-count for
		// allocations that occurred due to lease expirations.
		++fa->chunkCount;
	}
	return 0;

}

#if 0
ssize_t
KfsClientImpl::DoSmallWriteToServer(int fd, off_t offset, const char *buf, size_t numBytes)
{
	ssize_t numIO;
	// 获取到文件当前读写位置的指针
	FilePosition *pos = FdPos(fd);
	// 获取当前文件指针所在chunk的chunk号
	ChunkAttr *chunk = GetCurrChunk(fd);

	// 生成写数据的命令，要写的数据也放入到命令当中
	vector<WriteInfo> w;
	WritePrepareOp op(nextSeq(), chunk->chunkId, chunk->chunkVersion);
	// get the socket for the master
	ServerLocation loc = chunk->chunkServerLoc[0];
	TcpSocket *masterSock = pos->GetChunkServerSocket(loc);

	op.offset = offset;
	op.numBytes = min(numBytes, KFS::CHUNKSIZE);
	op.AttachContentBuf(buf, numBytes);
	op.contentLength = op.numBytes;

	w.resize(chunk->chunkServerLoc.size());

	// 向各个chunk服务器中写入数据，并向master发送同步命令
	for (uint8_t retryCount = 0; retryCount < NUM_RETRIES_PER_OP; retryCount++)
	{
		if (retryCount)
		{
			KFS_LOG_VA_DEBUG("Will retry write after %d secs",
					RETRY_DELAY_SECS);
			Sleep(RETRY_DELAY_SECS);
			op.seq = nextSeq();
		}

		for (uint32_t i = 0; i < chunk->chunkServerLoc.size(); i++)
		{
			// chunk是当前指针所在的chunk，所以s得到的是对应的chunkserver
			ServerLocation l = chunk->chunkServerLoc[i];
			TcpSocket *s = pos->GetChunkServerSocket(l);

			assert(op.contentLength == op.numBytes);

			numIO = DoOpCommon(&op, s);

			w[i].serverLoc = l;

			// 若操作成功，则记录主机名：端口 写操作序号
			if (op.status == 0)
			{
				istringstream ist(op.writeIdStr);
				ServerLocation loc;
				int64_t id;

				ist >> loc.hostname;
				ist >> loc.port;
				ist >> id;

				w[i].writeId = id;
				continue;
			}

			if (NeedToRetryWrite(op.status))
			{
				break;
			}

			w[i].writeId = -1;
		}

		// 如果在上边的操作中有一个需要重试，则进行重试操作
		if (NeedToRetryWrite(op.status))
		{
			// do a retry
			continue;
		}

		// 向master chunk server发送同步数据命令
		WriteSyncOp cop(nextSeq(), chunk->chunkId, chunk->chunkVersion, w);

		numIO = DoOpCommon(&cop, masterSock);
		op.status = cop.status;

		if (!NeedToRetryWrite(op.status))
		{
			break;
		} // else ...retry
	}

	// op.status > 0，说明该chunk的大小应该括大了
	if (op.status >= 0 && (off_t)chunk->chunkSize < offset + op.status)
	{
		// grow the chunksize only if we wrote past the last byte in the chunk
		chunk->chunkSize = offset + op.status;

		// if we wrote past the last byte of the file, then grow the
		// file size.  Note that, chunks 0..chunkNum-1 are assumed to
		// be full.  So, take the size of the last chunk and to that
		// add the size of the "full" chunks to get the size
		FileAttr *fa = FdAttr(fd);
		off_t eow = chunk->chunkSize + (pos->chunkNum * KFS::CHUNKSIZE);
		fa->fileSize = max(fa->fileSize, eow);
	}

	numIO = op.status;
	op.ReleaseContentBuf();

	if (numIO >= 0)
	{
		KFS_LOG_VA_DEBUG("Wrote to server (fd = %d), %lld bytes",
				fd, numIO);
	}
	return numIO;
}
#endif

///
/// @note 小数据写和大数据写合并，用同一种方法
///
ssize_t KfsClientImpl::DoSmallWriteToServer(int fd, off_t offset,
		const char *buf, size_t numBytes)
{
	return DoLargeWriteToServer(fd, offset, buf, numBytes);
}

///
/// @brief 向文件末尾写入数据
/// @param [in] fd 文件描述符
/// @param [in] offset chunk内偏移
/// @return 返回写入的字节数目
/// @note 写入的数据只能小于或等于一个chunk的最大值
///
ssize_t KfsClientImpl::DoLargeWriteToServer(int fd, off_t offset,
		const char *buf, size_t numBytes)
{
	size_t numAvail, numWrote = 0;
	ssize_t numIO;
	FilePosition *pos = FdPos(fd);	// 文件当前读写位置
	ChunkAttr *chunk = GetCurrChunk(fd);	// 文件当前chunk

	// 指定master服务器为第一个服务器
	ServerLocation loc = chunk->chunkServerLoc[0];
	TcpSocket *masterSock = FdPos(fd)->GetChunkServerSocket(loc);
	vector<WritePrepareOp *> ops;	// 写操作
	vector<WriteInfo> writeId;

	assert(KFS::CHUNKSIZE - offset >= 0);

	// 保证写入的数据不超过一个chunk的大小
	numAvail = min(numBytes, (size_t)(KFS::CHUNKSIZE - offset));

	// cout << "Pushing to server: " << offset << ' ' << numBytes << endl;

	// get the write id
	// 为每一个chunkserver分配一个写ID
	numIO = AllocateWriteId(fd, offset, numBytes, writeId, masterSock);
	if (numIO < 0)
	{
		KFS_LOG_VA_INFO("AllocateWriteId failed: %d", numIO);
		return numIO;
	}

	// Split the write into a bunch of smaller ops
	// 将写数据的操作按校验块分配成一个一个的小操作，存储在ops中；这样就可以形成一个
	// 流水线操作方法，能够充分利用带宽
	cout << " ### Start to write perpared Ops with write ID: " << numIO << endl;

	while (numWrote < numAvail)
	{
		WritePrepareOp *op = new WritePrepareOp(nextSeq(), chunk->chunkId,
				chunk->chunkVersion, writeId);

		op->numBytes = min(MAX_BYTES_PER_WRITE_IO, numAvail - numWrote);

		if ((op->numBytes % CHECKSUM_BLOCKSIZE) != 0)
		{
			// if the write isn't aligned to end on a checksum block
			// boundary, then break the write into two parts:
			//(1) start at offset and end on a checksum block boundary
			//(2) is the rest, which is less than the size of checksum
			//block
			// This simplifies chunkserver code: either the writes are
			// multiples of checksum blocks or a single write which is
			// smaller than a checksum block.
			if (op->numBytes > CHECKSUM_BLOCKSIZE)
				op->numBytes = (op->numBytes / CHECKSUM_BLOCKSIZE)
						* CHECKSUM_BLOCKSIZE;
			// else case #2 from above comment and op->numBytes is setup right
		}

		assert(op->numBytes > 0);

		op->offset = offset + numWrote;

		// similar to read, breakup the write if it is straddling
		// checksum block boundaries.
		if (OffsetToChecksumBlockStart(op->offset) != op->offset)
		{
			op->numBytes = min((size_t)(OffsetToChecksumBlockEnd(op->offset)
					- op->offset), op->numBytes);
		}

		op->AttachContentBuf(buf + numWrote, op->numBytes);
		op->contentLength = op->numBytes;
		op->checksum = ComputeBlockChecksum(op->contentBuf, op->contentLength);
		// op->checksum = 0;

		numWrote += op->numBytes;
		ops.push_back(op);
	}

	// For pipelined data push to work, we break the write into a
	// sequence of smaller ops and push them to the master; the master
	// then forwards each op to one replica, who then forwards to
	// next.
	printf("\033[34m\033[1m" "Start to do pipelining!\n" "\033[0m");
	for (int retryCount = 0; retryCount < NUM_RETRIES_PER_OP; retryCount++)
	{
		if (retryCount != 0)
		{
			// 若第一次写数据失败，则重新分配写序号，然后重试
			KFS_LOG_VA_INFO("Will retry write after %d secs",
					RETRY_DELAY_SECS);
			Sleep(RETRY_DELAY_SECS);

			KFS_LOG_DEBUG("Starting retry sequence...");

			// get the write id
			numIO = AllocateWriteId(fd, offset, numBytes, writeId, masterSock);

			if (numIO < 0)
			{
				KFS_LOG_INFO("Allocate write id failed...retrying");
				continue;
			}

			// for each op bump the sequence #
			for (vector<WritePrepareOp *>::size_type i = 0; i < ops.size(); i++)
			{
				ops[i]->seq = nextSeq();
				ops[i]->status = 0;
				ops[i]->writeInfo = writeId;
				assert(ops[i]->contentLength == ops[i]->numBytes);
			}
		}
		// 将数据通过流水线发送给master服务器，并获取发送结果
		printf("\033[34m\033[1m" "DoPipelinedWrite!\n" "\033[0m");
		numIO = DoPipelinedWrite(fd, ops, masterSock);
		cout << " ### " << "Doing pipelined write: " << numIO << endl;

		assert(numIO != -KFS::EBADVERS);

		if ((numIO == 0) || (numIO == -KFS::ELEASEEXPIRED))
		{
			// all good or the server lease expired; so, we have to
			// redo the allocation and such
			break;
		}
		if (NeedToRetryWrite(numIO) || (numIO == -EINVAL))
		{
			// retry; we can get an EINVAL if the server died in the
			// midst of a push: after we got write-id and sent it
			// data, it died and restarted; so, when we send commit,
			// it doesn't know the write-id and returns an EINVAL
			string errstr = ErrorCodeToStr(numIO);
			KFS_LOG_VA_INFO("Retrying write because of error: %s", errstr.c_str());
			continue;
		}
		if (numIO < 0)
		{
			KFS_LOG_VA_INFO("Write failed...chunk = %lld, version = %lld, offset = %lld, bytes = %lld",
					ops[0]->chunkId, ops[0]->chunkVersion, ops[0]->offset,
					ops[0]->numBytes);
			assert(numIO != -EBADF);
			break;
		}
		KFS_LOG_VA_INFO("Pipelined write did: %d", numIO);
	}

	//KFS_LOG_INFO("Start to do WritePerparedOps!");
	printf("\033[34m\033[1m" "Start to do recycle Prepared Ops!\n" "\033[0m");
	// figure out how much was committed
	// 统计发送的数据量，并且清空已经发送了的数据缓存
	numIO = 0;
	for (vector<KfsOp *>::size_type i = 0; i < ops.size(); ++i)
	{
		WritePrepareOp *op = static_cast<WritePrepareOp *> (ops[i]);
		if (op->status < 0)
		{
			cout << "Ops with no." << i << " failed: " << op->status << endl;
			numIO = op->status;
		}
		else if (numIO >= 0)
		{
			numIO += op->status;
		}
		op->ReleaseContentBuf();
		delete op;
	}

	cout << "Num of bytes wrote: " << numIO << endl;

	KFS_LOG_INFO("Set the size of chunks!");

	if (numIO >= 0 && (off_t) chunk->chunkSize < offset + numIO)
	{
		// grow the chunksize only if we wrote past the last byte in the chunk
		chunk->chunkSize = offset + numIO;

		// if we wrote past the last byte of the file, then grow the
		// file size.  Note that, chunks 0..chunkNum-1 are assumed to
		// be full.  So, take the size of the last chunk and to that
		// add the size of the "full" chunks to get the size
		FileAttr *fa = FdAttr(fd);

		// 更新文件大小：我们假定当前写的块就是文件的最后一个块
		off_t eow = chunk->chunkSize + (pos->chunkNum * KFS::CHUNKSIZE);
		fa->fileSize = max(fa->fileSize, eow);
	}

	if (numIO != (ssize_t) numBytes)
	{
		KFS_LOG_VA_INFO("Wrote to server (fd = %d), %lld bytes, was asked %llu bytes",
				fd, numIO, numBytes);
	}

	KFS_LOG_VA_INFO("Wrote to server (fd = %d), %lld bytes",
			fd, numIO);

	return numIO;
}

///
/// @brief 为该chunk的写数据分配写序号
/// @note 每一个chunk都要分配一个属于自己的序号，通过writeId返回
///
int KfsClientImpl::AllocateWriteId(int fd, off_t offset, size_t numBytes,
		vector<WriteInfo> &writeId, TcpSocket *masterSock)
{
	ChunkAttr *chunk = GetCurrChunk(fd);	// 文件当前读写位置（所在的chunk）
	WriteIdAllocOp op(nextSeq(), chunk->chunkId, chunk->chunkVersion, offset,
			numBytes);
	int res;

	// 向master服务器请求写数据，这个请求中包含该chunk所有的拷贝的服务器地址
	op.chunkServerLoc = chunk->chunkServerLoc;
	res = DoOpSend(&op, masterSock);
	if (res < 0)
		return res;
	if (op.status < 0)
		return op.status;

	// 接收服务器的回复
	res = DoOpResponse(&op, masterSock);
	if (res < 0)
		return res;
	if (op.status < 0)
		return op.status;

	// get rid of any old stuff
	writeId.clear();

	writeId.reserve(op.chunkServerLoc.size());
	istringstream ist(op.writeIdStr);
	for (uint32_t i = 0; i < chunk->chunkServerLoc.size(); i++)
	{
		ServerLocation loc;
		int64_t id;

		ist >> loc.hostname;
		ist >> loc.port;
		ist >> id;

		cout << " ### Write ID: " << id << endl;
		writeId.push_back(WriteInfo(loc, id));
	}
	return 0;
}

///
/// @brief 分别将数据发送到masterSock中
///
int KfsClientImpl::PushData(int fd, vector<WritePrepareOp *> &ops,
		uint32_t start, uint32_t count, TcpSocket *masterSock)
{
	uint32_t last = min((size_t)(start + count), ops.size());
	int res = 0;

	for (uint32_t i = start; i < last; i++)
	{
		cout << ops[i]->Show() << endl;

		printf("\033[1m" "DoOpSend from PushData->masterSock\n" "\033[0m");
		res = DoOpSend(ops[i], masterSock);
		if (res < 0)
			break;
	}
	return res;
}

///
/// @brief 向服务器发送写数据确认命令
///
int KfsClientImpl::SendCommit(int fd, vector<WriteInfo> &writeId,
		TcpSocket *masterSock, WriteSyncOp &sop)
{
	ChunkAttr *chunk = GetCurrChunk(fd);
	int res = 0;

	sop.Init(nextSeq(), chunk->chunkId, chunk->chunkVersion, writeId);

	res = DoOpSend(&sop, masterSock);
	if (res < 0)
		return sop.status;

	return 0;
}

int KfsClientImpl::GetCommitReply(WriteSyncOp &sop, TcpSocket *masterSock)
{
	int res;

	// 返回传输的数据量
	res = DoOpResponse(&sop, masterSock);
	if (res < 0)
		return sop.status;
	return sop.status;

}

///
/// @brief 发送一系列数据
/// @return 返回发送结果
/// @note 将数据分成更小的分组，每次发送一个分组的数据，同时发送第二个分组的数据，并对前一组\n
/// @note 进行确认; 如果一组数据失败，则所有发送均失败
///
int KfsClientImpl::DoPipelinedWrite(int fd, vector<WritePrepareOp *> &ops,
		TcpSocket *masterSock)
{
	int res;
	vector<WritePrepareOp *>::size_type next, minOps;
	WriteSyncOp syncOp[2];

	if (ops.size() == 0)
	{
		return 0;
	}

	printf("\033[34m\033[1m" "There are %d ops to be done!\n" "\033[0m", ops.size());

	// push the data to the server; to avoid bursting the server with
	// a full chunk, do it in a pipelined fashion:
	//  -- send 512K of data; do a flush; send another 512K; do another flush
	//  -- every time we get an ack back, we send another 512K
	//

	// we got 2 commits: current is the one we just sent; previous is
	// the one for which we are expecting a reply
	int prevCommit = 0;
	int currCommit = 1;

	// 再将一系列的写操作分成不同的组，分别发送
	minOps = min((size_t)(MIN_BYTES_PIPELINE_IO / MAX_BYTES_PER_WRITE_IO) / 2,
			ops.size());

	// 向服务器发送0到minOps这几个数据
	printf("\033[34m\033[1m" "Start to push data to server!\n" "\033[0m");
	res = PushData(fd, ops, 0, minOps, masterSock);
	if (res < 0)
		goto error_out;

	// 发送确认请求，确认刚才的操作已经完成：确认是否所有的chunkserver都完成了这次操作
	res = SendCommit(fd, ops[0]->writeInfo, masterSock, syncOp[0]);

	if (res < 0)
		goto error_out;

	for (next = minOps; next < ops.size(); next += minOps)
	{
		res = PushData(fd, ops, next, minOps, masterSock);
		if (res < 0)
			goto error_out;

		res = SendCommit(fd, ops[next]->writeInfo, masterSock,
				syncOp[currCommit]);
		if (res < 0)
			goto error_out;

		// 接收上一次发出的确认结果
		res = GetCommitReply(syncOp[prevCommit], masterSock);
		prevCommit = currCommit;
		currCommit++;
		currCommit %= 2;

		// 上一次的发送失败，退出循环，对刚才发出的命令进行确认
		if (res < 0)
			// the commit for previous failed; there is still the
			// business of getting the reply for the "current" one
			// that we sent out.
			break;
	}

	res = GetCommitReply(syncOp[prevCommit], masterSock);

	error_out: if (res < 0)
	{	// 找出错误信息
		// res will be -1; we need to pick out the error from the op that failed
		for (uint32_t i = 0; i < ops.size(); i++)
		{
			if (ops[i]->status < 0)
			{
				res = ops[i]->status;
				break;
			}
		}
	}

	// 如果有错误出现，则所有的操作均标记为错误；否则，状态位标记成发送的字节数
	// set the status for each op: either all were successful or none was.
	for (uint32_t i = 0; i < ops.size(); i++)
	{
		if (res < 0)
			ops[i]->status = res;
		else
			ops[i]->status = ops[i]->numBytes;
	}
	return res;
}

int KfsClientImpl::IssueCommit(int fd, vector<WriteInfo> &writeId,
		TcpSocket *masterSock)
{
	ChunkAttr *chunk = GetCurrChunk(fd);
	WriteSyncOp sop(nextSeq(), chunk->chunkId, chunk->chunkVersion, writeId);
	int res;

	res = DoOpSend(&sop, masterSock);
	if (res < 0)
		return sop.status;

	res = DoOpResponse(&sop, masterSock);
	if (res < 0)
		return sop.status;
	return sop.status;
}

