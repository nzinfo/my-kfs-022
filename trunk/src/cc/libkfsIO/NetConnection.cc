//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: NetConnection.cc 77 2008-07-12 01:23:54Z sriramsrao $
//
// Created 2006/03/14
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

#include "Globals.h"
#include "NetConnection.h"
#include "Globals.h"

using namespace KFS;
using namespace KFS::libkfsio;

/// 处理读事件。注意：若mListenOnly为真，则只是为了建立连接；否则，需要从socket中读取数据
void NetConnection::HandleReadEvent(bool isSystemOverloaded)
{
	NetConnectionPtr conn;
	TcpSocket *sock;
	int nread;

	if (mListenOnly)
	{	// 此时的Connection只是为了接受新的连接，故不考虑是否过载，也不建立缓存
		// 接受对方的连接请求
		sock = mSock->Accept();
#ifdef DEBUG
		if (sock == NULL)
		{
			KFS_LOG_VA_DEBUG("# of open-fd's: disk=%d, net=%d",
					globals().ctrOpenDiskFds.GetValue(),
					globals().ctrOpenNetFds.GetValue());
		}
#endif
		if (sock == NULL)
			return;

		// 根据返回的sock，建立Connection
		conn.reset(new NetConnection(sock, NULL));
		mCallbackObj->HandleEvent(EVENT_NEW_CONNECTION, (void *) &conn);
	}
	else
	{
		if (isSystemOverloaded && (!mEnableReadIfOverloaded))
			return;

		if (mInBuffer == NULL)
		{
			mInBuffer = new IOBuffer();
		}
		// XXX: Need to control how much we read...
		nread = mInBuffer->Read(mSock->GetFd());
		if (nread == 0)
		{
			KFS_LOG_DEBUG("Read 0 bytes...connection dropped");
			mCallbackObj->HandleEvent(EVENT_NET_ERROR, NULL);
		}
		else if (nread > 0)
		{
			mCallbackObj->HandleEvent(EVENT_NET_READ, (void *) mInBuffer);
		}
	}
}

// 处理写事件：
void NetConnection::HandleWriteEvent()
{
	int nwrote;

	// if non-blocking connect was set, then the first callback
	// signaling write-ready means that connect() has succeeded.
	mDoingNonblockingConnect = false;

	// clear the value so we can let flushes thru when possible
	mLastFlushResult = 0;

	if (!IsWriteReady())
		return;

	// XXX: Need to pay attention to mNumBytesOut---that is, write out
	// only as much as is asked for.
	nwrote = mOutBuffer->Write(mSock->GetFd());
	if (nwrote == 0)
	{
		KFS_LOG_DEBUG("Wrote 0 bytes...connection dropped");
		mCallbackObj->HandleEvent(EVENT_NET_ERROR, NULL);
	}
	else if (nwrote > 0)
	{
		mCallbackObj->HandleEvent(EVENT_NET_WROTE, (void *) mOutBuffer);
	}
}

void NetConnection::HandleErrorEvent()
{
	KFS_LOG_DEBUG("Got an error on socket.  Closing connection");
	mSock->Close();
	mCallbackObj->HandleEvent(EVENT_NET_ERROR, NULL);
}

/// 如果系统没有过载或者允许过载之后继续读，则说明“读就绪”
bool NetConnection::IsReadReady(bool isSystemOverloaded)
{
	if (!isSystemOverloaded)
		return true;
	// under load, this instance variable controls poll state
	return mEnableReadIfOverloaded;
}

bool NetConnection::IsWriteReady()
{
	// 如果采用的是none-blocking connection
	if (mDoingNonblockingConnect)
		return true;

	// 如果输出buffer是空
	if (mOutBuffer == NULL)
		return false;

	// 输出buffer有数据可以输出
	return (mOutBuffer->BytesConsumable() > 0);
}

int NetConnection::GetNumBytesToWrite()
{
	// force addition to the poll vector
	if (mDoingNonblockingConnect)
		return 1;

	if (mOutBuffer == NULL)
		return 0;

	return mOutBuffer->BytesConsumable();
}

/// 如果socket是没有问题的，则该连接是没有问题的
bool NetConnection::IsGood()
{
	return mSock->IsGood();
}

