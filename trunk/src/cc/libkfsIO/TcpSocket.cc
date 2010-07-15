//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: TcpSocket.cc 152 2008-09-17 19:03:51Z sriramsrao $
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

#include "TcpSocket.h"
#include "common/log.h"

#include "Globals.h"

#include <cerrno>
#include <poll.h>
#include <netdb.h>
#include <arpa/inet.h>

using std::min;
using std::max;
using std::vector;

using namespace KFS;
using namespace KFS::libkfsio;

TcpSocket::~TcpSocket()
{
	Close();
}

int TcpSocket::Listen(int port)
{
	// 本地地址（hostname, port）
	struct sockaddr_in ourAddr;
	int reuseAddr = 1;

	mSockFd = socket(PF_INET,SOCK_STREAM, 0);
	if (mSockFd == -1)
	{
		perror("Socket: ");
		return -1;
	}

	memset(&ourAddr, 0, sizeof(struct sockaddr_in));
	ourAddr.sin_family = AF_INET; // 协议族

	// 防止出现多字节数据存储不一致的错误
	ourAddr.sin_addr.s_addr = htonl(INADDR_ANY); // 帮定到任意一块网卡上，而不指定地址
	ourAddr.sin_port = htons(port); // host to net short

	/*
	 * A piece of magic here: Before we bind the fd to a port, setup
	 * the fd to reuse the address. If we move this line to after the
	 * bind call, then things don't work out.  That is, we bind the fd
	 * to a port; we panic; on restart, bind will fail with the address
	 * in use (i.e., need to wait 2MSL for TCP's time-wait).  By tagging
	 * the fd to reuse an address, everything is happy.
	 */
	// 设置closesocket后可以继续重用该socket，目的？？
	if (setsockopt(mSockFd, SOL_SOCKET, SO_REUSEADDR, (char *) &reuseAddr,
	        sizeof(reuseAddr)) < 0)
	{
		perror("Setsockopt: ");
	}

	// bind()将套接字地址（包括本地主机地址和本地端口地址）与所创建的套接字号联系起来，即将名字赋予套接字
	if (bind(mSockFd, (struct sockaddr *) &ourAddr, sizeof(ourAddr)) < 0)
	{
		perror("Bind: ");
		close(mSockFd);
		mSockFd = -1;
		return -1;
	}

	if (listen(mSockFd, 5) < 0)
	{
		perror("listen: ");
	}

	globals().ctrOpenNetFds.Update(1);

	return 0;

}

TcpSocket* TcpSocket::Accept()
{
	int fd;
	struct sockaddr_in cliAddr;
	TcpSocket *accSock;
	socklen_t cliAddrLen = sizeof(cliAddr);

	if ((fd = accept(mSockFd, (struct sockaddr *) &cliAddr, &cliAddrLen)) < 0)
	{
		perror("Accept: ");
		return NULL;
	}
	accSock = new TcpSocket(fd);

	accSock->SetupSocket();

	// “打开的网路”计数器加一
	globals().ctrOpenNetFds.Update(1);

	return accSock;
}

int TcpSocket::Connect(const struct sockaddr_in *remoteAddr,
        bool nonblockingConnect)
{
	int res = 0;

	// 关闭连接，如果已经连接，避免造成空连接
	Close();

	// 使用TCP/IP，流socket
	mSockFd = socket(PF_INET,SOCK_STREAM, 0);
	if (mSockFd == -1)
	{
		return -1;
	}

	// 决定是否启用nonblocking
	if (nonblockingConnect)
	{
		// when we do a non-blocking connect, we mark the socket
		// non-blocking; then call connect and it wil return
		// EINPROGRESS; the fd is added to the select loop to check
		// for completion
		fcntl(mSockFd, F_SETFL, O_NONBLOCK);
	}

	res = connect(mSockFd, (struct sockaddr *) remoteAddr,
	        sizeof(struct sockaddr_in));
	if ((res < 0) && (errno != EINPROGRESS))
	{
		perror("Connect: ");
		close(mSockFd);
		mSockFd = -1;
		return -1;
	}

	if (nonblockingConnect)
		res = -errno;

	SetupSocket();

	// “已经建立的连接”数加一
	globals().ctrOpenNetFds.Update(1);

	return res;
}

// 封装通过ServerLocation建立连接的方法
int TcpSocket::Connect(const ServerLocation &location, bool nonblockingConnect)
{
	struct hostent *hostInfo;
	struct sockaddr_in remoteAddr;

	remoteAddr.sin_addr.s_addr = inet_addr(location.hostname.c_str());

	if (remoteAddr.sin_addr.s_addr == (in_addr_t) -1)
	{
		// do the conversion if we weren't handed an IP address

		hostInfo = gethostbyname(location.hostname.c_str());

		if (hostInfo == NULL)
		{
#if defined __APPLE__
			KFS_LOG_VA_DEBUG("herrno: %d, errstr = %s", h_errno, hstrerror(h_errno));
#endif
			perror("gethostbyname: ");
			return -1;
		}

		memcpy(&remoteAddr.sin_addr.s_addr, hostInfo->h_addr,
		        sizeof(struct in_addr));
	}
	remoteAddr.sin_port = htons(location.port);
	remoteAddr.sin_family = AF_INET;

	return Connect(&remoteAddr, nonblockingConnect);
}

// 修改mSockFd的属性，
// 包括：SO_SNDBUF, SO_RCVBUF, SO_KEEPALIVE, NONBLOCK, TCP_NODELAY
void TcpSocket::SetupSocket()
{
#if defined(__sun__)
	int bufSize = 512 * 1024;
#else
	int bufSize = 65536;
#endif
	int flag = 1;

	// get big send/recv buffers and setup the socket for non-blocking I/O
	// 在send()的时候，返回的是实际发送出去的字节(同步)或发送到socket缓冲区的字节
	// (异步);系统默认的状态发送和接收一次为8688字节(约为8.5K)；在实际的过程中发送数据
	// 和接收数据量比较大，可以设置socket缓冲区，而避免了send(),recv()不断的循环收发：
	// SO_SNDBUF指定发送缓冲区大小，SO_RCVBUF指定接收缓冲区的大小
	if (setsockopt(mSockFd, SOL_SOCKET, SO_SNDBUF, (char *) &bufSize,
	        sizeof(bufSize)) < 0)
	{
		perror("Setsockopt: ");
	}
	if (setsockopt(mSockFd, SOL_SOCKET, SO_RCVBUF, (char *) &bufSize,
	        sizeof(bufSize)) < 0)
	{
		perror("Setsockopt: ");
	}
	// enable keep alive so we can socket errors due to detect network partitions
	if (setsockopt(mSockFd, SOL_SOCKET, SO_KEEPALIVE, (char *) &flag,
	        sizeof(flag)) < 0)
	{
		perror("Disabling NAGLE: ");
	}

	// 把socket的属性设置为：nonblocking
	fcntl(mSockFd, F_SETFL, O_NONBLOCK);
	// turn off NAGLE
	// 把选项设置为TCP层(IPPROTO_TCP)，关闭NAGLE(TCP_NODELAY):
	// send frequent small bursts of information without
	// getting an immediate response
	if (setsockopt(mSockFd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag,
	        sizeof(flag)) < 0)
	{
		perror("Disabling NAGLE: ");
	}
}

// 获取由socket指定的peer的地址
int TcpSocket::GetPeerName(struct sockaddr *peerAddr)
{
	socklen_t peerLen;

	if (getpeername(mSockFd, peerAddr, &peerLen) < 0)
	{
		perror("getpeername: ");
		return -1;
	}
	return 0;
}

int TcpSocket::Send(const char *buf, int bufLen)
{
	int nwrote;

	nwrote = send(mSockFd, buf, bufLen, 0);
	if (nwrote > 0)
	{
		globals().ctrNetBytesWritten.Update(nwrote);
	}
	return nwrote;
}

int TcpSocket::Recv(char *buf, int bufLen)
{
	int nread;

	nread = recv(mSockFd, buf, bufLen, 0);
	if (nread > 0)
	{
		globals().ctrNetBytesRead.Update(nread);
	}

	return nread;
}

// 查看但并不删除缓冲区数据
int TcpSocket::Peek(char *buf, int bufLen)
{
	int nread;

	// MSG_PEEK查看当前数据。数据将被复制到缓冲区中，但并不从输入队列中删除。
	nread = recv(mSockFd, buf, bufLen, MSG_PEEK);
	return nread;
}

bool TcpSocket::IsGood()
{
	if (mSockFd < 0)
		return false;

#if 0
	char c;

	// the socket could've been closed by the system because the peer
	// died.  so, tell if the socket is good, peek to see if any data
	// can be read; read returns 0 if the socket has been
	// closed. otherwise, will get -1 with errno=EAGAIN.

	if (Peek(&c, 1) == 0)
	return false;
#endif
	return true;
}

void TcpSocket::Close()
{
	if (mSockFd < 0)
	{
		return;
	}
	close(mSockFd);
	mSockFd = -1;
	globals().ctrOpenNetFds.Update(-1);
}

// 同步数据发送，返回值为发送的字节数
int TcpSocket::DoSynchSend(const char *buf, int bufLen)
{
	int numSent = 0;
	int res = 0, nfds;
	fd_set fds;
	struct timeval timeout;

	while (numSent < bufLen)
	{
		if (mSockFd < 0)
			break;
		if (res < 0)
		{
			// 初始化集合fds
			FD_ZERO(&fds);
			// 将mSockFd加入fds集合
			FD_SET(mSockFd, &fds);
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
			// 集合的最大fd+1，监视可读，监视可写，监视异常，超时时长
			nfds = select(mSockFd + 1, NULL, &fds, &fds, &timeout);

			// 若超时，则返回0，说明此时文件不可写，则继续进行忙等
			if (nfds == 0)
				continue;
		}

		// 发送内容的首地址，发送内容的长度
		res = Send(buf + numSent, bufLen - numSent);

		// 说明没有被传输的数据，直接返回（不需要进行计数统计）
		if (res == 0)
			return 0;
		if ((res < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno
		        == EINTR)))
			continue;
		if (res < 0)
			break;
		numSent += res;
	}
	if (numSent > 0)
	{
		globals().ctrNetBytesWritten.Update(numSent);
	}
	return numSent;
}

//
// Receive data within a certain amount of time.  If the server is too slow in responding, bail
//
// 同步数据接收。timeout设置每次读数据的超时时间
int TcpSocket::DoSynchRecv(char *buf, int bufLen, struct timeval &timeout)
{
	int numRecd = 0;
	int res = 0, nfds;
	struct pollfd pfd;

	while (numRecd < bufLen)
	{
		if (mSockFd < 0)
			break;

		if (res < 0)
		{
			pfd.fd = mSockFd;
			pfd.events = POLLIN;
			pfd.revents = 0;

			// 判断有没有数据可以读
			nfds = poll(&pfd, 1, timeout.tv_sec * 1000);
			// get a 0 when timeout expires
			if (nfds == 0)
			{
				KFS_LOG_DEBUG("Timeout in synch recv");
				return numRecd > 0 ? numRecd : -ETIMEDOUT;
			}
		}

		res = Recv(buf + numRecd, bufLen - numRecd);
		if (res == 0)
			return 0;
		if ((res < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno
		        == EINTR)))
			continue;
		if (res < 0)
			break;
		numRecd += res;
	}
	if (numRecd > 0)
	{
		globals().ctrNetBytesRead.Update(numRecd);
	}

	return numRecd;
}

//
// Receive data within a certain amount of time and discard them.  If
// the server is too slow in responding, bail
//
// 从socket中取出nbytes的数据，并且丢弃。返回丢弃的数据的字节数
int TcpSocket::DoSynchDiscard(int nbytes, struct timeval &timeout)
{
	int numRecd = 0, ntodo, res;
	const int bufSize = 4096;
	char buf[bufSize];

	while (numRecd < nbytes)
	{
		ntodo = min(nbytes - numRecd, bufSize);
		res = DoSynchRecv(buf, ntodo, timeout);
		if (res == -ETIMEDOUT)
			return numRecd;
		assert(numRecd >= 0);
		if (numRecd < 0)
			break;
		numRecd += res;
	}
	return numRecd;
}

//
// Peek data within a certain amount of time.  If the server is too slow in responding, bail
//
// 监听信道上有没有需要传输的数据
int TcpSocket::DoSynchPeek(char *buf, int bufLen, struct timeval &timeout)
{
	int numRecd = 0;
	int res, nfds;
	struct pollfd pfd;

	while (1)
	{
		pfd.fd = mSockFd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		nfds = poll(&pfd, 1, timeout.tv_sec * 1000);
		// get a 0 when timeout expires
		if (nfds == 0)
		{
			return -ETIMEDOUT;
		}

		res = Peek(buf + numRecd, bufLen - numRecd);
		if (res == 0)
			return 0;

		// 由于是非阻塞的模式,所以当errno为EAGAIN时,表示当前缓冲区已无数据可读
		if ((res < 0) && (errno == EAGAIN))
			continue;
		if (res < 0)
			break;
		numRecd += res;
		if (numRecd > 0)
			break;
	}
	return numRecd;
}
