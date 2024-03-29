//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: Acceptor.cc 71 2008-07-07 15:49:14Z sriramsrao $
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

#include "Acceptor.h"
#include "NetManager.h"
#include "Globals.h"

using namespace KFS;
using namespace KFS::libkfsio;
///
/// Create a TCP socket, bind it to the port, and listen for incoming connections.
///
/// 创建一个半开连接，对指定的端口进行监听
Acceptor::Acceptor(int port, IAcceptorOwner *owner)
{
	TcpSocket *sock;

	sock = new TcpSocket();

	mAcceptorOwner = owner;
	sock->Listen(port);
	mConn.reset(new NetConnection(sock, this, true));
	SET_HANDLER(this, &Acceptor::RecvConnection);
	globals().netManager.AddConnection(mConn);
}

Acceptor::~Acceptor()
{
	mConn->Close();
	mConn.reset();
}

///
/// Event handler that gets called back whenever a new connection is
/// received.  In response, the AcceptorOwner object is first notified of
/// the new connection and then, the new connection is added to the
/// list of connections owned by the NetManager. @see NetManager
///
int Acceptor::RecvConnection(int code, void *data)
{
	NetConnectionPtr conn = *(NetConnectionPtr *) data;
	KfsCallbackObj *callbackObj;

	// Shut-up g++
	(void) code;

	callbackObj = mAcceptorOwner->CreateKfsCallbackObj(conn);
	conn->SetOwningKfsCallbackObj(callbackObj);

	///
	/// Add the connection to the net manager's list of "polling"
	/// fd's.
	///
	globals().netManager.AddConnection(conn);

	return 0;
}
