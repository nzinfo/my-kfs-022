//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: ChunkServer.h 92 2008-07-21 21:20:48Z sriramsrao $
//
// Created 2006/03/16
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

#ifndef _CHUNKSERVER_H
#define _CHUNKSERVER_H

#include "libkfsIO/Acceptor.h"
#include "libkfsIO/DiskManager.h"
#include "libkfsIO/EventManager.h"
#include "libkfsIO/NetManager.h"
#include "libkfsIO/Globals.h"

#include "ChunkManager.h"
#include "ClientManager.h"
#include "ClientSM.h"
#include "MetaServerSM.h"
#include "RemoteSyncSM.h"

namespace KFS
{
class ChunkServer
{
public:
	ChunkServer() :
		mOpCount(0), mKickNetThread(false)
	{
	}
	;

	void Init();

	void MainLoop(int clientAcceptPort);

	bool IsLocalServer(const ServerLocation &location) const
	{
		return mLocation == location;
	}
	RemoteSyncSMPtr FindServer(const ServerLocation &location, bool connect =
			true);
	void RemoveServer(RemoteSyncSM *target);

	std::string GetMyLocation() const
	{
		return mLocation.ToString();
	}

	void ToggleNetThreadKicking(bool v)
	{
		mKickNetThread = v;
	}

	bool NeedToKickNetThread()
	{
		return mKickNetThread;
	}

	void OpInserted()
	{
		mOpCount++;
	}

	void OpFinished()
	{
		mOpCount--;
		if (mOpCount < 0)
			mOpCount = 0;
	}
	int GetNumOps() const
	{
		return mOpCount;
	}

private:
	/// 接收client请求的端口
	int mClientAcceptPort;
	// 系统中操作(op)计数器
	int mOpCount;
	/// Unknown!!
	bool mKickNetThread;
	// 当前主机的ServerLocation
	ServerLocation mLocation;

	/// 用于向远端服务器发送消息和处理远端服务器返回的信息，处理完成后调用
	/// KFS::SubmitOpResponse()函数.
	/// @note 每一个server会对应一个RemoteSyncSM
	std::list<RemoteSyncSMPtr> mRemoteSyncers;
};

extern void verifyExecutingOnNetProcessor();
extern void verifyExecutingOnEventProcessor();
extern void StopNetProcessor(int status);

extern ChunkServer gChunkServer;
}

#endif // _CHUNKSERVER_H
