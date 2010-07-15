//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id:$
//
// Author: Sriram Rao
//
// Copyright 2008 Quantcast Corp.
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
// Implementation of the net kicker object.
//----------------------------------------------------------------------------

#include "NetKicker.h"
#include "Globals.h"

using namespace KFS;
using namespace KFS::libkfsio;

//
NetKicker::NetKicker()
{
	int res;

	// 创建一个无名管道
	// mPipeFds[0]是用来读的文件描述符,而mPipeFds[1]是用来写的文件描述符
	res = pipe(mPipeFds);
	if (res < 0)
	{
		perror("Pipe: ");
		return;
	}

	// 设置管道输入输出属性为：none-blocking
	fcntl(mPipeFds[0], F_SETFL, O_NONBLOCK);
	fcntl(mPipeFds[1], F_SETFL, O_NONBLOCK);

	// Add the read part of the fd to the net manager's poll loop
	TcpSocket *sock = new TcpSocket(mPipeFds[0]);
	mNetConnection.reset(new NetConnection(sock, this));
	// force this fd to be part of the poll loop.
	mNetConnection->EnableReadIfOverloaded();

	// SET_HANDLER(this, &NetKicker::Drain);
}

// 该类的初始化就是将自己加入到一个NetManager中
void NetKicker::Init(NetManager &netManager)
{
	netManager.AddConnection(mNetConnection);
}

void NetKicker::Kick()
{
	char buf = 'k';

	// 将'k'字符发送到管道
	write(mPipeFds[1], &buf, sizeof(char));
}

// 删除管道中的字符
int NetKicker::Drain()
{
	int bufsz = 512, res;
	char buf[512];

	// 删除管道当中的字符，并返回已经删除的字符的个数
	res = read(mPipeFds[0], buf, bufsz);
	return res;
}

#if 0
int
NetKicker::Drain(int code, void *data)
{
	if (code == EVENT_NET_READ)
	{
		IOBuffer *buffer = (IOBuffer *) data;
		int nread = buffer->BytesConsumable();
		buffer->Consume(nread);
		if (nread> 1)
		{
			// we got multiple kicks together...put one back.  This
			// ensures that the subsequent kicks are handled by the
			// net-manager in the next round (without waiting for a
			// full poll delay).
			Kick();
		}
	}
	return 0;
}
#endif
