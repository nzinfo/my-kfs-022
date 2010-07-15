//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: ChunkServer.cc 155 2008-09-18 04:47:20Z sriramsrao $
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

// order of #includes here is critical.  don't change it
#include "libkfsIO/Counter.h"
#include "libkfsIO/Globals.h"

#include "ChunkServer.h"
#include "Utils.h"

#include <netdb.h>
#include <arpa/inet.h>

using std::list;

using namespace KFS;
using namespace KFS::libkfsio;

// single network thread that manages connections and net I/O
/// 管理网络和网络读写的线程
static MetaThread netProcessor;

/// 本地全局chunkServer
ChunkServer KFS::gChunkServer;

/// 启动网络监听，监听所有的socket读写操作
static void * netWorker(void *dummy)
{
	globals().netManager.MainLoop();
	return NULL;
}

/// 使用netProcessor启动一个线程，对网络进行管理
static void StartNetProcessor()
{
	// 调用start建立一个线程
	netProcessor.start(netWorker, NULL);
}

void ChunkServer::Init()
{
	// 建立一系列的命令和函数指针的对应关系
	InitParseHandlers();
	// Register the counters
	// 建立命令和Counter的对应关系并且注册到全局变量的counterManager中
	RegisterCounters();
}

/// 常驻函数，具体操作列表如下：
/*
 * 1. 获取本地host IP和接收客户请求的端口
 * 2. 启动监听socket
 * 3. 启动日志系统：Logger
 * 4. 启动chunkManager
 * 5. 启动管理MetaServer请求和相应的管理器
 * 6. 使用netProcessor启动一个线程，对网络进行管理
 * 7. 暂停当前线程，等待其他线程运行
 */
void ChunkServer::MainLoop(int clientAcceptPort)
{
#if !defined (__sun__)
	static const int MAXHOSTNAMELEN = 256;
#endif
	char hostname[MAXHOSTNAMELEN];

	// 指定接收用户socket连接的端口号
	mClientAcceptPort = clientAcceptPort;

	// 把自己的主机名导入到hostname中，hostname最大长度为MAXHOSTNAMELEN，
	// 若超出长度，则进行截尾操作
	if (gethostname(hostname, MAXHOSTNAMELEN))
	{
		perror("gethostname: ");
		exit(-1);
	}

	// convert to IP address
	// 将hostname转换成hostent类，主要目的是提取其IP地址
	struct hostent *hent = gethostbyname(hostname);
	in_addr ipaddr;

	// 提取出IP地址到ipaddr中
	memcpy(&ipaddr, hent->h_addr, hent->h_length);

	mLocation.Reset(inet_ntoa(ipaddr), clientAcceptPort);

	// 1. 启动socket，监听网络上的连接请求
	gClientManager.StartAcceptor(clientAcceptPort);
	// 2. 启动日志记录器
	gLogger.Start();
	// 3. 启动chunkManager，即向全局变量的netManager中注册定时处理程序（含Loop）
	gChunkManager.Start();
	// gMetaServerSM.SendHello(clientAcceptPort);
	// 4. 启动管理MetaServer请求和相应的管理器（含Loop，常驻）
	gMetaServerSM.Init(clientAcceptPort);

	// 5. 使用netProcessor启动一个线程，对网络进行管理（含Loop，单独线程）
	StartNetProcessor();

	/*
	 * pthread_join()的调用者将挂起并等待th线程终止，retval是pthread_exit()调用者
	 * 线程（线程ID为th）的返回值，如果thread_return不为NULL，则
	 * *thread_return=retval。需要注意的是一个线程仅允许唯一的一个线程使用
	 * pthread_join()等待它的终止，并且被等待的线程应该处于可join状态，即非DETACHED状态。
	 */
	// 6. 暂停当前线程，等待指定线程（netProcessor）运行
	netProcessor.join();
}

/// 比较两个RemoteSyncSM是否相同：RemoteSyncSM通过mLocation唯一标识
class RemoteSyncSMMatcher
{
	ServerLocation myLoc;
public:
	RemoteSyncSMMatcher(const ServerLocation &loc) :
		myLoc(loc)
	{
	}
	bool operator()(RemoteSyncSMPtr other)
	{
		return other->GetLocation() == myLoc;
	}
};

/// 在mRemoteSyncers中查找对应于location的RemoteSyncSM
/// 如果找到了，就直接返回；如果没有找到，并且connect为真，则创建一个location相关的
/// RemoteSyncSM
RemoteSyncSMPtr ChunkServer::FindServer(const ServerLocation &location,
		bool connect)
{
	list<RemoteSyncSMPtr>::iterator i;
	RemoteSyncSMPtr peer;

	// 查找location对应的RemoteSyncSM
	i = find_if(mRemoteSyncers.begin(), mRemoteSyncers.end(),
			RemoteSyncSMMatcher(location));
	// 如果在mRemoteSyncers中找到了指定的RemoteSyncSM，则直接返回其指针即可
	if (i != mRemoteSyncers.end())
	{
		peer = *i;
		return peer;
	}

	// 如果没有找到对应的RemoteSyncSM，并且不需要进行连接，则直接返回找空指针即可
	if (!connect)
		return peer;

	// 建立location相关的RemoteSyncSM
	peer.reset(new RemoteSyncSM(location));
	// 对远端服务器进行连接
	if (peer->Connect())
	{
		mRemoteSyncers.push_back(peer);
	}
	else
	{
		// we couldn't connect...so, force destruction
		peer.reset();
	}
	return peer;
}

/// 从mRemoteSyncers中删除指定的RemoteSyncSM
void ChunkServer::RemoveServer(RemoteSyncSM *target)
{
	list<RemoteSyncSMPtr>::iterator i;

	i = find_if(mRemoteSyncers.begin(), mRemoteSyncers.end(),
			RemoteSyncSMMatcher(target->GetLocation()));
	if (i != mRemoteSyncers.end())
	{
		mRemoteSyncers.erase(i);
	}
}

/// 验证当前线程是否属于netProcessor
void KFS::verifyExecutingOnNetProcessor()
{
	assert(netProcessor.isEqual(pthread_self()));
	if (!netProcessor.isEqual(pthread_self()))
	{
		die("FATAL: Not executing on net processor");
	}
}

/// 退出NetProcessor线程
void KFS::StopNetProcessor(int status)
{
	netProcessor.exit(status);
}
