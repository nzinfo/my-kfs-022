//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: BufferedSocket.cc 71 2008-07-07 15:49:14Z sriramsrao $
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
// \brief Code for doing buffered reads from socket.
//
//----------------------------------------------------------------------------

#include <iostream>
#include <strings.h>

#include "BufferedSocket.h"

using namespace KFS;
using std::string;

/// 从指定的端口中读入一行数据：先从缓存中查找是否有换行。若有，则读入缓存中的一行，放到result
/// 中，然后返回；否则，先把缓存中的所有数据读入到result，再从端口中读入数据直到发现一个换行为止
/// @param[out] result 读入的数据存放的位置
int BufferedSocket::ReadLine(string &result)
{
	int navail, nread = 0;
	char *lineEnd;

	// 查找从mHead开始的第一个'\n'.
	lineEnd = index(mHead, '\n');
	if (lineEnd != NULL)
	{
		// 连同最后的换行符一起读入
		nread = lineEnd - mHead + 1;
		result.append(mHead, nread);
		Consume(nread);
		return nread;
	}

	// 此时没有结束符，则拷贝出缓存中的所有数据，然后继续从端口中读入数据，直到有一个换行符为止
	if (mAvail > 0)
	{
		nread = mAvail;
		result.append(mHead, nread);
		Consume(nread);
	}

	// read until we get a new-line
	while (1)
	{
		// 从Socket中读入指定大小的数据
		navail = Recv(mBuffer, BUF_SIZE);
		if (navail == 0)
		{
			// socket is down
			return nread;
		}
		if ((navail < 0) && (errno == EAGAIN))
			continue;
		if (navail < 0)
			break;

		// 可用数据变多
		Fill(navail);

		lineEnd = index(mBuffer, '\n');
		if (lineEnd == NULL)
		{
			// haven't hit a line boundary...so, keep going
			result.append(mBuffer, navail);
			nread += navail;
			Consume(navail);
			continue;
		}
		navail = (lineEnd - mBuffer + 1);
		nread += navail;
		result.append(mBuffer, navail);
		Consume(navail);
		break;
	}
	return nread;
}

/// 接收数据，与Revc的不同在于调用TcpSocket中接收数据的函数不同
int BufferedSocket::DoSynchRecv(char *buf, int bufLen, struct timeval &timeout)
{
	int nread = 0, res;

	// 先从缓存中读入数据，若读入的数据不够，再从socket中读入数据
	if (mAvail > 0)
	{
		nread = bufLen < mAvail ? bufLen : mAvail;
		memcpy(buf, mHead, nread);
		Consume(nread);
	}

	if ((bufLen - nread) <= 0)
		return nread;

	assert(mAvail == 0);

	res = TcpSocket::DoSynchRecv(buf + nread, bufLen - nread, timeout);
	if (res > 0)
		nread += res;
	return nread;

}

/// 从socket中读入指定长度的数据：若缓存中有足够的可用数据，那么从缓存中读入数据并返回；否则，
/// 从socket中读入
int BufferedSocket::Recv(char *buf, int bufLen)
{
	int nread = 0, res;

	// Copy out of the buffer and then, if needed, get from the socket.
	if (mAvail > 0)
	{
		// 从缓存中读入足够多的数据
		nread = bufLen < mAvail ? bufLen : mAvail;
		memcpy(buf, mHead, nread);
		Consume(nread);
	}

	if ((bufLen - nread) <= 0)
		return nread;

	assert(mAvail == 0);

	// 从端口中读入数据（执行BufferedSocket的基类中的Recv）
	res = TcpSocket::Recv(buf + nread, bufLen - nread);
	if (res > 0)
	{
		nread += res;
		return nread;
	}
	if (nread == 0)
		return res;
	return nread;
}
