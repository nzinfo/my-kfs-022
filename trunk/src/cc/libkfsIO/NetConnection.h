//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: NetConnection.h 92 2008-07-21 21:20:48Z sriramsrao $
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

#ifndef _LIBIO_NETCONNECTION_H
#define _LIBIO_NETCONNECTION_H

#include "KfsCallbackObj.h"
#include "Event.h"
#include "IOBuffer.h"
#include "TcpSocket.h"

#include "common/log.h"

#include <boost/shared_ptr.hpp>

namespace KFS
{
///
/// \file NetConnection.h
/// \brief A network connection uses TCP sockets for doing I/O.
///
/// A network connection contains a socket and data in buffers.
/// Whenever data is read from the socket it is held in the "in"
/// buffer; whenever data needs to be written out on the socket, that
/// data should be dropped into the "out" buffer and it will
/// eventually get sent out.
///

///
/// \class NetConnection
/// A net connection contains an underlying socket and is associated
/// with a KfsCallbackObj.  Whenever I/O is done on the socket (either
/// for read or write) or when an error occurs (such as the remote
/// peer closing the connection), the associated KfsCallbackObj is
/// called back with an event notification.
///
/// 网络连接：主要负责socket和buffer之间的数据交互
/// 每一个NetConnection关联一个socket
class NetConnection
{
public:
	/// @param[in] sock TcpSocket on which I/O can be done
	/// @param[in] c KfsCallbackObj associated with this connection
	NetConnection(TcpSocket *sock, KfsCallbackObj *c) :
		mListenOnly(false), mEnableReadIfOverloaded(false),
				mDoingNonblockingConnect(false), mCallbackObj(c), mSock(sock),
				mInBuffer(NULL), mOutBuffer(NULL),
				mNumBytesOut(0), mLastFlushResult(0)
	{
		mPollVectorIndex = -1;
	}

	/// @param[in] sock TcpSocket on which I/O can be done
	/// @param[in] c KfsCallbackObj associated with this connection
	/// @param[in] listenOnly boolean that specifies whether this
	/// connection is setup only for accepting new connections.
	/// 如果是ListenOnly，则该Connection只是用来接受新连接
	NetConnection(TcpSocket *sock, KfsCallbackObj *c, bool listenOnly) :
		mListenOnly(listenOnly), mDoingNonblockingConnect(false), mCallbackObj(
				c), mSock(sock), mInBuffer(NULL), mOutBuffer(NULL),
		mNumBytesOut(0), mLastFlushResult(0)
	{
		mPollVectorIndex = -1;
		if (listenOnly)
			mEnableReadIfOverloaded = true;
	}

	~NetConnection()
	{
		delete mSock;
		delete mOutBuffer;
		delete mInBuffer;
	}

	void SetDoingNonblockingConnect()
	{
		mDoingNonblockingConnect = true;
	}
	void SetOwningKfsCallbackObj(KfsCallbackObj *c)
	{
		mCallbackObj = c;
	}

	void EnableReadIfOverloaded()
	{
		mEnableReadIfOverloaded = true;
	}

	/// 获取该Connection指向的socket描述符
	int GetFd()
	{
		return mSock->GetFd();
	}

	/// Callback for handling a read.  That is, select() thinks that
	/// data is available for reading. So, do something.  If system is
	/// overloaded and we don't have a special pass, leave the data in
	/// the buffer alone.
	/// 处理读事件。注意：若mListenOnly为真，则只是为了建立连接；否则，需要从socket中读取数据
	void HandleReadEvent(bool isSystemOverloaded);

	/// Callback for handling a writing.  That is, select() thinks that
	/// data can be sent out.  So, do something.
	/// 处理写事件：
	void HandleWriteEvent();

	/// Callback for handling errors.  That is, select() thinks that
	/// an error occurred.  So, do something.
	void HandleErrorEvent();

	/// Do we expect data to be read in?  If the system is overloaded,
	/// check the connection poll state to determine the return value.
	/// 如果系统没有过载或者允许过载之后继续读，则说明“读就绪”
	bool IsReadReady(bool isSystemOverloaded);
	/// Is data available for writing?
	bool IsWriteReady();

	/// # of bytes available for writing
	int GetNumBytesToWrite();

	/// Is the connection still good?
	/// 如果socket是没有问题的，则该连接是没有问题的
	bool IsGood();

	/// Enqueue data to be sent out.
	void Write(IOBufferDataPtr &ioBufData)
	{
		mOutBuffer->Append(ioBufData);
		mNumBytesOut += ioBufData->BytesConsumable();
	}

	/// 写数据时，先把数据写入缓存，此时并不建立连接
	void Write(IOBuffer *ioBuf, int numBytes)
	{
		mOutBuffer->Append(ioBuf);
		mNumBytesOut += numBytes;
	}

	/// Enqueue data to be sent out.
	void Write(const char *data, int numBytes)
	{
		if (mOutBuffer == NULL)
		{
			mOutBuffer = new IOBuffer();
		}
		mOutBuffer->CopyIn(data, numBytes);
		mNumBytesOut += numBytes;
	}

	/// 将输出缓存中的数据输出到socket中，同时缓存就会被清空
	void StartFlush()
	{
		if ((mLastFlushResult < 0) || (mDoingNonblockingConnect))
			return;
		// if there is any data to be sent out, start the send
		if (mOutBuffer && mOutBuffer->BytesConsumable() > 0)
			mLastFlushResult = mOutBuffer->Write(mSock->GetFd());
	}
	/// Close the connection.
	void Close()
	{
		// KFS_LOG_DEBUG("Closing socket: %d", mSock->GetFd());
		mSock->Close();
	}
	/// index into the poll fd's vector
	/// 该连接在poll（NetManager中）向量中的位置
	int mPollVectorIndex;
private:
	bool mListenOnly;
	/// should we add this connection to the poll vector for reads
	/// even when the system is overloaded?
	bool mEnableReadIfOverloaded;

	/// Set if the connect was done in non-blocking manner.  In this
	/// case, the first callback for "write ready" will mean that we
	/// connect has finished.
	bool mDoingNonblockingConnect;

	/// KfsCallbackObj that will be notified whenever "events" occur.
	KfsCallbackObj *mCallbackObj;
	/// Socket on which I/O will be done.
	TcpSocket *mSock;
	/// Buffer that contains data read from the socket
	IOBuffer *mInBuffer;
	/// Buffer that contains data that should be sent out on the socket.
	IOBuffer *mOutBuffer;
	/// # of bytes from the out buffer that should be sent out.
	int mNumBytesOut;
	// 上一次清空缓存的结果
	int mLastFlushResult;
};

typedef boost::shared_ptr<NetConnection> NetConnectionPtr;

}
#endif // LIBIO_NETCONNECTION_H
