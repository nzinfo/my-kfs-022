//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: ChunkServerFactory.h 71 2008-07-07 15:49:14Z sriramsrao $
//
// Created 2006/06/06
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
// \file ChunkServerFactory.h
// \brief Create ChunkServer objects whenever a chunk server connects
// to us (namely, the meta server).
//
//----------------------------------------------------------------------------

#ifndef META_CHUNKSERVERFACTORY_H
#define META_CHUNKSERVERFACTORY_H

#include <list>
using std::list;

#include "libkfsIO/Acceptor.h"
#include "libkfsIO/KfsCallbackObj.h"
#include "kfstypes.h"
#include "ChunkServer.h"

namespace KFS
{
///
/// ChunkServerFactory creates a ChunkServer object whenever
/// a chunk server connects to us.  The ChunkServer object is
/// responsible for all the communication with that chunk
/// server.
///
class ChunkServerFactory: public IAcceptorOwner
{
public:
	// 初始化时, 并不创建监听端口, 监听端口的创建通过StartAcceptor创建
	ChunkServerFactory()
	{
		mAcceptor = NULL;
	}

	virtual ~ChunkServerFactory()
	{
		delete mAcceptor;
	}

	/// Start an acceptor to listen on the specified port.
	void StartAcceptor(int port)
	{
		mAcceptor = new Acceptor(port, this);
	}

	/// Callback that gets invoked whenever a chunkserver
	/// connects to the acceptor port.  The accepted socket
	/// connection is passed in.
	/// @param[in] conn: The accepted connection
	/// @retval The continuation object that was created as a
	/// result of this call.
	/// 根据一个网络连接, 创建ChunkServer, 存放到mChunkServers中
	KfsCallbackObj *CreateKfsCallbackObj(NetConnectionPtr &conn)
	{
		ChunkServerPtr cs(new ChunkServer(conn));
		mChunkServers.push_back(cs);
		return cs.get();
	}
	void RemoveServer(const ChunkServer *target);
private:
	// The socket object which is setup to accept connections from
	/// chunkserver.
	/// 创建一个端口, 用来监听chunkserver发出的连接请求.
	Acceptor *mAcceptor;
	// List of connected chunk servers.
	/// 已经连接的chunkserver的指针
	list<ChunkServerPtr> mChunkServers;
};
}

#endif // META_CHUNKSERVERFACTORY_H
