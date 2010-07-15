//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: TcpSocket.h 152 2008-09-17 19:03:51Z sriramsrao $
//
// Created 2006/03/10
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

#ifndef _LIBIO_TCP_SOCKET_H
#define _LIBIO_TCP_SOCKET_H

extern "C"
{
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
}

#include <boost/shared_ptr.hpp>

#include <string>
#include <sstream>
#include <vector>

#include "common/kfsdecls.h"

namespace KFS
{

///
/// \file TcpSocket.h
/// \brief Class that hides the internals of doing socket I/O.
///


///
/// \class TcpSocket
/// \brief TCP sockets used in KFS are (1) non-blocking for I/O and (2) have
/// large send/receive buffers.
///
// 提供封装了socket的TcpSocket连接
// 特点：(1)none-blocking (2)large snd/rcv buffers（默认4096 bytes）
// 每个socket关联一个sock描述符
class TcpSocket
{
public:
	TcpSocket()
	{
		mSockFd = -1;
	}
	/// Wrap the passed in file descriptor in a TcpSocket
	/// @param[in] fd file descriptor corresponding to a TCP socket.
	TcpSocket(int fd)
	{
		mSockFd = fd;
	}

	~TcpSocket();

	/// Setup a TCP socket that listens for connections
	/// @param port Port on which to listen for incoming connections
	int Listen(int port);

	/// Accept connection on a socket.
	/// @retval A TcpSocket pointer that contains the accepted
	/// connection.  It is the caller's responsibility to free the
	/// pointer returned by this method.
	///
	TcpSocket* Accept();

	/// Connect to the remote address.  If non-blocking connect is
	/// set, the socket is first marked non-blocking and then we do
	/// the connect call.  Then, you use select() to check for connect() completion
	/// @retval 0 on success; -1 on failure; -EINPROGRESS if we do a
	/// nonblockingConnect and connect returned that error code
	int Connect(const struct sockaddr_in *remoteAddr, bool nonblockingConnect =
			false);
	int Connect(const ServerLocation &location, bool nonblockingConnect =
					false);

	/// Do block-IO's, where # of bytes to be send/recd is the length
	/// of the buffer.
	/// @retval Returns # of bytes sent or -1 if there was an error.
	/// 同步数据发送，返回值为发送的字节数
	int DoSynchSend(const char *buf, int bufLen);

	/// For recv/peek, specify a timeout within which data should be received.
	/// 同步数据接收。timeout设置每次读数据的超时时间
	int DoSynchRecv(char *buf, int bufLen, struct timeval &timeout);
	/// 监听信道上有没有需要传输的数据
	int DoSynchPeek(char *buf, int bufLen, struct timeval &timeout);

	/// Discard a bunch of bytes that are coming down the pike.
	/// 从socket中取出nbytes的数据，并且丢弃。返回丢弃的数据的字节数
	int DoSynchDiscard(int len, struct timeval &timeout);

	/// Peek to see if any data is available.  This call will not
	/// remove the data from the underlying socket buffers.
	/// @retval Returns # of bytes copied in or -1 if there was an error.
	/// 查看但并不删除缓冲区数据
	int Peek(char *buf, int bufLen);

	/// Get the file descriptor associated with this socket.
	inline int GetFd()
	{
		return mSockFd;
	}
	;

	/// Return true if socket is good for read/write. false otherwise.
	bool IsGood();

	/// 获取由socket指定的peer的地址
	int GetPeerName(struct sockaddr *peerAddr);

	/// Sends at-most the specified # of bytes.
	/// @retval Returns the result of calling send().
	int Send(const char *buf, int bufLen);

	/// Receives at-most the specified # of bytes.
	/// @retval Returns the result of calling recv().
	int Recv(char *buf, int bufLen);

	/// Close the TCP socket.
	void Close();

private:
	int mSockFd;

	void SetupSocket();
};

typedef boost::shared_ptr<TcpSocket> TcpSocketPtr;

}

#endif // _LIBIO_TCP_SOCKET_H
