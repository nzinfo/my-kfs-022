//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: BufferedSocket.h 71 2008-07-07 15:49:14Z sriramsrao $
//
// Created 2006/07/03
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
// \brief Helper class that has a socket with a buffer.  This enables
// API such as readLine() where we read N bytes from the socket,
// return a line and leave the rest buffered for subsequent access.
// NOTE: This class only does buffered I/O for reads; for writes, it
// is all pass-thru to the TcpSocket class.
//----------------------------------------------------------------------------

#ifndef LIBKFSIO_BUFFEREDSOCKET_H
#define LIBKFSIO_BUFFEREDSOCKET_H

#include <cerrno>
#include <cassert>
#include "TcpSocket.h"
#include <string>

namespace KFS
{

/// 该类和TcpSocket实现的功能相同，不同点在于这个Socket有一个本地缓存。除缓存之外，所有的功能
/// 都是通过TcpSocket实现的
class BufferedSocket: public TcpSocket
{
public:
	BufferedSocket()
	{
		Reset();
	}
	BufferedSocket(int fd) :
		TcpSocket(fd)
	{
		Reset();
	}
	/// Read a line of data which is terminated by a '\n' from the
	/// socket.

	/// 从指定的端口中读入一行数据：先从缓存中查找是否有换行。若有，则读入缓存中的一行，放到result
	/// 中，然后返回；否则，先把缓存中的所有数据读入到result，再从端口中读入数据直到发现一个换行为止
	/// @param[out] result   The string that corresponds to data read
	/// from the network
	/// @retval # of bytes read; -1 on error
	int ReadLine(std::string &result);

	/// Synchronously (blocking) receive for the desired # of bytes.
	/// Note that we first pull data out the buffer and if there is
	/// more to be read, we get it from the socket.
	/// @param[out] buf  The buffer to be filled with data from the
	/// socket.
	/// @param[in] bufLen  The desired # of bytes to be read in
	/// @param[in] timeout  The max amount of time to wait for data
	/// @retval # of bytes read; -1 on error; -ETIMEOUT if timeout
	/// expires and no data is read in
	/// 接收数据，与Revc的不同在于调用TcpSocket中接收数据的函数不同
	int DoSynchRecv(char *buf, int bufLen, struct timeval &timeout);

	/// Read at most the specified # of bytes from the socket.
	/// Note that we first pull data out the buffer and if there is
	/// more to be read, we get it from the socket.  The read is
	/// non-blocking: if recv() returns EAGAIN (to indicate that no
	/// data is available), we return how much ever data we have read
	/// so far.
	/// @param[out] buf  The buffer to be filled with data from the
	/// socket.
	/// @param[in] bufLen  The desired # of bytes to be read in
	/// @retval # of bytes read; -1 on error
	/// 从socket中读入指定长度的数据：若缓存中有足够的可用数据，那么从缓存中读入数据并返回；否则，
	/// 从socket中读入
	int Recv(char *buf, int bufLen);

private:
	const static int BUF_SIZE = 4096;
	/// The buffer into which data has been read from the socket.
	/// 从端口中读入数据之后，放到这个缓存中
	char mBuffer[BUF_SIZE];
	/// Since we have read from the buffer, head tracks where the next
	/// character is available for read from mBuffer[]
	/// 缓存中有效数据的起始地址
	char *mHead;
	/// How much data is in the buffer
	/// 缓存中可用数据的字节数
	int mAvail;

	/// 重置可用数据的起始位置为缓存的起始地址，可用数据量为0，缓存中无数据
	void Reset()
	{
		mHead = mBuffer;
		mAvail = 0;
		memset(mBuffer, '\0', BUF_SIZE);
	}

	/// 增加可用数据的长度
	void Fill(int nbytes)
	{
		mAvail += nbytes;
		assert(mAvail <= BUF_SIZE);
	}

	/// 消费指定数量的可用数据
	void Consume(int nbytes)
	{
		mHead += nbytes;
		mAvail -= nbytes;
		if (mAvail == 0)
			Reset();
		assert(mAvail >= 0);
	}

};

}

#endif // LIBKFSIO_BUFFEREDSOCKET_H
