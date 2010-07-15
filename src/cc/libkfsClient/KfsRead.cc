//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: KfsRead.cc 164 2008-09-21 20:17:57Z sriramsrao $
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
// All the code to deal with read.
//----------------------------------------------------------------------------

#include "KfsClientInt.h"

#include "common/config.h"
#include "common/properties.h"
#include "common/log.h"
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

using namespace KFS;

///
/// 计算两个时间的差，单位：秒，精度：double
/// @param [in] startTime
/// @param [in] endTime
/// @return 两个时间的差
/// @note 返回值为double
///
static double ComputeTimeDiff(const struct timeval &startTime,
		const struct timeval &endTime)
{
	float timeSpent;

	timeSpent = (endTime.tv_sec * 1e6 + endTime.tv_usec) - (startTime.tv_sec
			* 1e6 + startTime.tv_usec);
	return timeSpent / 1e6;
}

///
/// 判断读操作是否要重试
/// @param [in] status 操作状态
///
static bool NeedToRetryRead(int status)
{
	return ((status == -KFS::EBADVERS) || (status == -KFS::EBADCKSUM)
			|| (status == -KFS::ESERVERBUSY) || (status == -EHOSTUNREACH)
			|| (status == -ETIMEDOUT));
}

///
/// @param [in] fd 文件描述符
/// @param [out] buf 存放数据的地址
/// @param [in] numBytes 要读入的字节数
/// @return 读入的字节数
///
ssize_t KfsClientImpl::Read(int fd, char *buf, size_t numBytes)
{
	MutexLock l(&mMutex);

	size_t nread = 0, nleft;
	ssize_t numIO;

	if (!valid_fd(fd) || mFileTable[fd] == NULL || mFileTable[fd]->openMode
			== O_WRONLY)
	{
		KFS_LOG_VA_INFO("Read to fd: %d failed---fd is likely closed", fd);
		return -EBADF;
	}

	FilePosition *pos = FdPos(fd);
	FileAttr *fa = FdAttr(fd);
	if (fa->isDirectory)
		return -EISDIR;

	// flush buffer so sizes are updated properly
	// 同步文件，保证提供的文件大小正确
	ChunkBuffer *cb = FdBuffer(fd);
	if (cb->dirty)
		FlushBuffer(fd);

	cb->allocate();

	// Loop thru chunk after chunk until we either get the desired #
	// of bytes or we hit EOF.
	while (nread < numBytes)
	{
		//
		// Basic invariant: when we enter this loop, the connections
		// we have to the chunkservers (if any) are correct.  As we
		// read thru a file, we call seek whenever we have data to
		// hand out to the client.  As we cross chunk boundaries, seek
		// will invalidate our current set of connections and force us
		// to get new ones via a call to OpenChunk().   This same
		// principle holds for write code path as well.
		//
		// 如果系统中没有该chunk的信息，则从metaserver请求该chunk信息
		if (!IsChunkReadable(fd))
			break;

		if (pos->fileOffset >= (off_t) fa->fileSize)
		{
			KFS_LOG_VA_DEBUG("Current pointer (%lld) is past EOF (%lld) ...so, done",
					pos->fileOffset, fa->fileSize);
			break;
		}

		nleft = numBytes - nread;
		numIO = ReadChunk(fd, buf + nread, nleft);
		if (numIO < 0)
			break;

		nread += numIO;
		Seek(fd, numIO, SEEK_CUR);
	}

	/*
	 KFS_LOG_DEBUG("----Read done: asked: %d, got: %d----------",
	 numBytes, nread);
	 */
	return nread;
}

///
/// 判断一个chunk是否可读
/// @param fd 文件描述符
/// @return 是否可读
/// @note 可读条件： 如果可以定位一个chunk的物理位置，并且租约有效
///
bool KfsClientImpl::IsChunkReadable(int fd)
{
	FilePosition *pos = FdPos(fd);
	int res;

	res = LocateChunk(fd, pos->chunkNum);

	// we can't locate the chunk...so, fail
	if (res < 0)
		return false;

	ChunkAttr *chunk = GetCurrChunk(fd);

	if (pos->preferredServer == NULL && chunk->chunkId != (kfsChunkId_t) -1)
	{
		// use nonblocking connect to chunkserver; if one fails to
		// connect, we switch to another replica.
		int status = OpenChunk(fd, true);
		if (status < 0)
			return false;
	}

	return IsChunkLeaseGood(chunk->chunkId);
}

///
/// @brief 判断某个租约是否过期
/// @param [in] chunkId chunk号码
/// @return 是否过期
/// @note 如果租约过期，则重新申请；如果应该更新租约，则更新租约
///
bool KfsClientImpl::IsChunkLeaseGood(kfsChunkId_t chunkId)
{
	if (chunkId > 0)
	{
		if ((!mLeaseClerk.IsLeaseValid(chunkId)) && (GetLease(chunkId) < 0))
		{
			// couldn't get a valid lease
			return false;
		}
		if (mLeaseClerk.ShouldRenewLease(chunkId))
		{
			RenewLease(chunkId);
		}
	}
	return true;
}

///
/// @brief 从chunk中读入numBytes个字节到buf中
/// @param [in] fd 文件描述符
/// @param [out] buf 存放数据的地址
/// @param [in] numBytes 要读入的字节数
/// @return 读入的字节数
///
ssize_t KfsClientImpl::ReadChunk(int fd, char *buf, size_t numBytes)
{
	ssize_t numIO;
	ChunkAttr *chunk;
	FilePosition *pos = FdPos(fd);
	int retryCount = 0;

	assert(valid_fd(fd));
	assert(pos->fileOffset < (off_t) mFileTable[fd]->fattr.fileSize);

	// 尝试从缓存中读入数据
	numIO = CopyFromChunkBuf(fd, buf, numBytes);
	if (numIO > 0)
		return numIO;

	chunk = GetCurrChunk(fd);

	// 否则，从chunkserver中读入数据
	while (retryCount < NUM_RETRIES_PER_OP)
	{
		if (pos->preferredServer == NULL)
		{
			// 如果默认chunkserver未指定，则重新定位chunkservers，并且确定默认server
			int status;

			// we come into this function with a connection to some
			// chunkserver; as part of the read, the connection
			// broke.  so, we need to "re-figure" where the chunk is.
			if (chunk->chunkId < 0)
			{
				status = LocateChunk(fd, pos->chunkNum);
				if (status < 0)
				{
					retryCount++;
					Sleep(RETRY_DELAY_SECS);
					continue;
				}
			}
			// we know where the chunk is....
			assert(chunk->chunkId != (kfsChunkId_t) -1);
			// we are here because we are handling failover/version #
			// mismatch
			retryCount++;
			Sleep(RETRY_DELAY_SECS);

			// open failed..so, bail....we use non-blocking connect to
			// switch another replica
			status = OpenChunk(fd, true);
			if (status < 0)
				return status;
		}

		// 零填充buf中的数据，填充的字节数为：min(文件当前指针到文件末尾的字节数，
		// 块中剩余的可用字节数)
		numIO = ZeroFillBuf(fd, buf, numBytes);
		if (numIO > 0)
			return numIO;

		// 如果数据量小于4MB，则将数据读入缓存，再从缓存中读出数据
		if (numBytes < ChunkBuffer::BUF_SIZE)
		{
			// small reads...so buffer the data
			ChunkBuffer *cb = FdBuffer(fd);
			numIO = ReadFromServer(fd, cb->buf, ChunkBuffer::BUF_SIZE);
			if (numIO > 0)
			{
				cb->chunkno = pos->chunkNum;
				cb->start = pos->chunkOffset;
				cb->length = numIO;
				numIO = CopyFromChunkBuf(fd, buf, numBytes);
			}
		}
		else
		{
			// big read...forget buffering
			// 大数据，直接读出到buf中
			numIO = ReadFromServer(fd, buf, numBytes);
		}

		if ((numIO >= 0) || (!NeedToRetryRead(numIO)))
		{
			// either we got data or it is an error which doesn't
			// require a retry of the read.
			break;
		}

		// KFS_LOG_DEBUG("Need to retry read...");
		// Ok...so, we need to retry the read.  so, re-determine where
		// the chunk went and then retry.
		chunk->chunkId = -1;

		// 清空服务器
		pos->ResetServers();
	}
	return numIO;
}

///
/// @brief 从服务器读入数据
/// @param [in] fd 文件描述符
/// @param [out] buf 存放数据的地址
/// @param [in] numBytes 要读入的字节数
/// @return 读入的字节数
/// @note 每次最多读入一个chunk的数据
///
ssize_t KfsClientImpl::ReadFromServer(int fd, char *buf, size_t numBytes)
{
	size_t numAvail;
	ChunkAttr *chunk = GetCurrChunk(fd);
	FilePosition *pos = FdPos(fd);
	int res;

	assert(chunk->chunkSize - pos->chunkOffset >= 0);

	numAvail = min((size_t)(chunk->chunkSize - pos->chunkOffset), numBytes);

	// Align the reads to checksum block boundaries, so that checksum
	// verification on the server can be done efficiently: if the read falls
	// within a checksum block, issue it as one read; otherwise, split
	// the read into multiple reads.
	// 再按校验块读入数据
	if (pos->chunkOffset + numAvail <= OffsetToChecksumBlockEnd(
			pos->chunkOffset))
		res = DoSmallReadFromServer(fd, buf, numBytes);
	else
		res = DoLargeReadFromServer(fd, buf, numBytes);

	return res;
}


///
/// @brief 向metaserver发送读取命令
/// @param [in] fd 文件描述符
/// @param [out] buf 存放数据的地址
/// @param [in] numBytes 要读入的字节数
/// @return 读入的字节数
///
ssize_t KfsClientImpl::DoSmallReadFromServer(int fd, char *buf, size_t numBytes)
{
	ChunkAttr *chunk = GetCurrChunk(fd);

	ReadOp op(nextSeq(), chunk->chunkId, chunk->chunkVersion);
	op.offset = mFileTable[fd]->currPos.chunkOffset;

	op.numBytes = min(chunk->chunkSize, (off_t) numBytes);
	op.AttachContentBuf(buf, numBytes);

	// make sure we aren't overflowing...
	assert(buf + op.numBytes <= buf + numBytes);

	(void) DoOpCommon(&op, mFileTable[fd]->currPos.preferredServer);
	ssize_t numIO = (op.status >= 0) ? op.contentLength : op.status;
	op.ReleaseContentBuf();

	return numIO;
}

///
/// @brief 零填充buf中的数据，填充的字节数为：min(文件当前指针到文件末尾的字节数，块中剩余的可\n
/// @brief 用字节数)
/// @brief 向metaserver发送读取命令
/// @param [in] fd 文件描述符
/// @param [out] buf 存放数据的地址
/// @param [in] numBytes 要读入的字节数
/// @return 读入的字节数
///
size_t KfsClientImpl::ZeroFillBuf(int fd, char *buf, size_t numBytes)
{
	size_t numIO, bytesInFile, bytesInChunk;
	ChunkAttr *chunk = GetCurrChunk(fd);

	if (mFileTable[fd]->currPos.chunkOffset < (off_t) chunk->chunkSize)
		return 0; // more data in chunk


	// We've hit End-of-chunk.  There are two cases here:
	// 1. There is more data in the file and that data is in
	// the next chunk
	// 2. This chunk was filled with less data than what was
	// "promised".  (Maybe, write got lost).
	// In either case, zero-fill: the amount to zero-fill is
	// in the min. of the two.
	//
	// Also, we can hit the end-of-chunk if we fail to locate a
	// chunk.  This can happen if there is a hole in the file.
	//

	assert(mFileTable[fd]->currPos.fileOffset <=
			(off_t) mFileTable[fd]->fattr.fileSize);

	// 从当前位置到文件结尾还有的字节数
	bytesInFile = mFileTable[fd]->fattr.fileSize
			- mFileTable[fd]->currPos.fileOffset;

	assert(chunk->chunkSize <= (off_t) KFS::CHUNKSIZE);

	// chunk中还有的可用空间字节数
	bytesInChunk = KFS::CHUNKSIZE - chunk->chunkSize;
	numIO = min(bytesInChunk, bytesInFile);
	// Fill in 0's based on space in the buffer....
	numIO = min(numIO, numBytes);

	// KFS_LOG_DEBUG("Zero-filling %d bytes for read @ %lld", numIO, mFileTable[fd]->currPos.chunkOffset);

	memset(buf, 0, numIO);
	return numIO;
}

///
/// @brief 从缓存中拷贝尽量多的数据到buf中
/// @param [in] fd 文件描述符
/// @param [out] buf 存放数据的地址
/// @param [in] numBytes 要读入的字节数
/// @return 读入的字节数
///
size_t KfsClientImpl::CopyFromChunkBuf(int fd, char *buf, size_t numBytes)
{
	size_t numIO;
	FilePosition *pos = FdPos(fd);
	ChunkBuffer *cb = FdBuffer(fd);
	size_t start = pos->chunkOffset - cb->start;

	// Wrong chunk in buffer or if the starting point in the buffer is
	// "BEYOND" the current location of the file pointer, we don't
	// have the data.  "BEYOND" => offset is before the starting point
	// or offset is after the end of the buffer
	// 如果当前文件指针所在块号和缓冲块号不一致 OR 当前指针位置小于缓存开始的位置 OR
	// 当前文件指针在chunk中的偏移大于缓存中最末位置的地址
	if ((pos->chunkNum != cb->chunkno) || (pos->chunkOffset < cb->start)
			|| (pos->chunkOffset >= (off_t) (cb->start + cb->length)))
		return 0;

	// first figure out how much data is available in the buffer
	// to be copied out.
	numIO = min(cb->length - start, numBytes);
	// chunkBuf[0] corresponds to some offset in the chunk,
	// which is defined by chunkBufStart.
	// chunkOffset corresponds to the position in the chunk
	// where the "filepointer" is currently at.
	// Figure out where the data we want copied out starts
	memcpy(buf, &cb->buf[start], numIO);

	// KFS_LOG_DEBUG("Copying out data from chunk buf...%d bytes", numIO);

	return numIO;
}

///
/// @param [in] fd 文件描述符
/// @param [out] buf 存放数据的地址
/// @param [in] numBytes 要读入的字节数
/// @return 读入的字节数
///
ssize_t KfsClientImpl::DoLargeReadFromServer(int fd, char *buf, size_t numBytes)
{
	FilePosition *pos = FdPos(fd);
	ChunkAttr *chunk = GetCurrChunk(fd);
	// 大数据需要多个ReadOp
	vector<ReadOp *> ops;

	assert(chunk->chunkSize - pos->chunkOffset >= 0);

	size_t numAvail = min((size_t)(chunk->chunkSize - pos->chunkOffset),
			numBytes);
	size_t numRead = 0;

	while (numRead < numAvail)
	{
		ReadOp *op = new ReadOp(nextSeq(), chunk->chunkId, chunk->chunkVersion);

		op->numBytes = min(MAX_BYTES_PER_READ_IO, numAvail - numRead);
		// op->numBytes = min(KFS::CHUNKSIZE, numAvail - numRead);
		assert(op->numBytes> 0);

		op->offset = pos->chunkOffset + numRead;

		// if the read is going to straddle checksum block boundaries,
		// break up the read into multiple reads: this simplifies
		// server side code.  for each read request, a single checksum
		// block will need to be read and after the checksum verifies,
		// the server can "trim" the data that wasn't asked for.
		// 如果要读入的字节数不是一个完整的校验块
		if (OffsetToChecksumBlockStart(op->offset) != op->offset)
		{
			op->numBytes = OffsetToChecksumBlockEnd(op->offset) - op->offset;
		}

		// 指定读入数据的指针，服务器响应时，会把数据放入到指定的指针位置
		op->AttachContentBuf(buf + numRead, op->numBytes);
		numRead += op->numBytes;

		ops.push_back(op);
	}
	// make sure we aren't overflowing...
	assert(buf + numRead <= buf + numBytes);

	struct timeval readStart, readEnd;

	gettimeofday(&readStart, NULL);
	{
		ostringstream os;

		os << pos->GetPreferredServerLocation().ToString().c_str() << ':'
				<< " c=" << chunk->chunkId << " o=" << pos->chunkOffset
				<< " n=" << numBytes;
		KFS_LOG_VA_INFO("Reading from %s", os.str().c_str());
	}

	// 做流水线读
	// 这次是将命令发送给chunkserver的
	ssize_t numIO = DoPipelinedRead(ops, pos->preferredServer);
	/*
	 if (numIO < 0) {
	 KFS_LOG_DEBUG("Pipelined read from server failed...");
	 }
	 */

	int retryStatus = 0;

	// 检测所有的操作是否都已经完成
	for (vector<KfsOp *>::size_type i = 0; i < ops.size(); ++i)
	{
		ReadOp *op = static_cast<ReadOp *> (ops[i]);
		if (op->status < 0)
		{
			if (NeedToRetryRead(op->status))
				retryStatus = op->status;
			numIO = op->status;
		}
		else if (numIO >= 0)
			numIO += op->status;

		// 将content指针置空，但不释放原来指针指向的内存空间
		op->ReleaseContentBuf();
		delete op;
	}

	// If the op needs to be retried, pass that up
	if (retryStatus != 0)
		numIO = retryStatus;

	gettimeofday(&readEnd, NULL);

	// 输出完成信息，返回读入的数据量
	double timeSpent = ComputeTimeDiff(readStart, readEnd);
	{
		ostringstream os;

		os << pos->GetPreferredServerLocation().ToString().c_str() << ':'
				<< " c=" << chunk->chunkId << " o=" << pos->chunkOffset
				<< " n=" << numBytes << " got=" << numIO << " time="
				<< timeSpent;

		KFS_LOG_VA_INFO("Read done from %s", os.str().c_str());
	}

	return numIO;
}


///
/// @brief 流水线方式请求数据，先发送一小批，然后完成一个再发送一个，知道所有的请求均发送完毕
/// @param[in] ops 要完成的操作队列
/// @param[in] sock 通信的server socket
/// @return 操作结果
/// @retval 0: 成功; 1: 失败
///
int KfsClientImpl::DoPipelinedRead(vector<ReadOp *> &ops, TcpSocket *sock)
{
	vector<ReadOp *>::size_type first = 0, next, minOps;
	int res;
	uint32_t cksum;
	ReadOp *op;
	bool leaseExpired = false;

	// plumb the pipe with 1MB
	minOps = min((size_t)(MIN_BYTES_PIPELINE_IO / MAX_BYTES_PER_READ_IO),
			ops.size());
	// plumb the pipe with a few ops
	// 将一部分请求发送到服务器
	for (next = 0; next < minOps; ++next)
	{
		op = ops[next];

		res = DoOpSend(op, sock);
		if (res < 0)
			return -1;
	}

	// run the pipe: whenever one op finishes, queue another
	// 当一个请求完成接收时，再发送另一个请求
	while (next < ops.size())
	{
		op = ops[first];

		// 逐个接收服务器的响应
		res = DoOpResponse(op, sock);
		if (res < 0)
			return -1;
		++first;

		if ((op->status >= 0) && (op->checksum != 0))
		{
			// 逐个校验数据的正确性
			cksum = ComputeBlockChecksum(op->contentBuf, op->status);
			if (cksum != op->checksum)
			{
				KFS_LOG_VA_INFO("Checksum mismatch: got = %d, computed = %d for %s",
						op->checksum, cksum, op->Show().c_str());
				op->status = -KFS::EBADCKSUM;
			}
		}

		op = ops[next];

		if (!IsChunkLeaseGood(op->chunkId))
		{
			leaseExpired = true;
			break;
		}

		res = DoOpSend(op, sock);
		if (res < 0)
			return -1;
		++next;
	}

	// get the response for the remaining ones
	// 再获取最后几个请求的回复
	while (first < next)
	{
		op = ops[first];

		res = DoOpResponse(op, sock);
		if (res < 0)
			return -1;

		if (leaseExpired)
			op->status = 0;

		++first;
	}
	return 0;
}
