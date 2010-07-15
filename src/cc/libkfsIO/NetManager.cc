//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: NetManager.cc 121 2008-08-11 17:39:23Z sriramsrao $
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

#include <sys/poll.h>
#include <cerrno>
#include <boost/scoped_array.hpp>

#include "NetManager.h"
#include "TcpSocket.h"
#include "Globals.h"

#include "common/log.h"

using std::mem_fun;
using std::list;

using namespace KFS;
using namespace KFS::libkfsio;

// 主要初始化一个递归型互斥信号量，可是该信号量的作用是什么？？
NetManager::NetManager() :
	mDiskOverloaded(false), mNetworkOverloaded(false), mMaxOutgoingBacklog(0)
{
	pthread_mutexattr_t mutexAttr;
	int rval;

	rval = pthread_mutexattr_init(&mutexAttr);
	assert(rval == 0);
	// 递归型互斥信号量：http://fifywang.blogspot.com/2008/12/linux.html
	rval = pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE);
	assert(rval == 0);

	mSelectTimeout.tv_sec = 10;
	mSelectTimeout.tv_usec = 0;
}

NetManager::NetManager(const struct timeval &selectTimeout) :
	mDiskOverloaded(false), mNetworkOverloaded(false), mMaxOutgoingBacklog(0)
{
	mSelectTimeout.tv_sec = selectTimeout.tv_sec;
	mSelectTimeout.tv_usec = selectTimeout.tv_usec;
}

NetManager::~NetManager()
{
	NetConnectionListIter_t iter;
	NetConnectionPtr conn;

	mTimeoutHandlers.clear();
	mConnections.clear();
}

void NetManager::AddConnection(NetConnectionPtr &conn)
{
	mConnections.push_back(conn);
}

void NetManager::RegisterTimeoutHandler(ITimeout *handler)
{
	mTimeoutHandlers.push_back(handler);
}

void NetManager::UnRegisterTimeoutHandler(ITimeout *handler)
{
	list<ITimeout *>::iterator iter;
	ITimeout *tm;

	for (iter = mTimeoutHandlers.begin(); iter != mTimeoutHandlers.end(); ++iter)
	{
		tm = *iter;
		if (tm == handler)
		{
			mTimeoutHandlers.erase(iter);
			return;
		}
	}
}

// NetManager常驻函数
// 监听系统中所有的TcpSocket，如果有读或写事件，则为之处理
void NetManager::MainLoop()
{
	boost::scoped_array<struct pollfd> pollfds;
	uint32_t pollFdSize = 1024;
	int numPollFds, fd, res;
	NetConnectionPtr conn;
	NetConnectionListIter_t iter, eltToRemove;
	int pollTimeoutMs;

	// if we have too many bytes to send, throttle incoming
	int64_t totalNumBytesToSend = 0;
	bool overloaded = false;

	pollfds.reset(new struct pollfd[pollFdSize]);
	while (1)
	{
		// 若连接数大于默认值（1024），则需要为pollfds重新分配内存
		if (mConnections.size() > pollFdSize)
		{
			pollFdSize = mConnections.size();
			pollfds.reset(new struct pollfd[pollFdSize]);
		}
		// build poll vector:

		// make sure we are listening to the net kicker
		// 第一个位置放NetKicker的读数据端的TCP连接
		fd = globals().netKicker.GetFd();
		pollfds[0].fd = fd;
		pollfds[0].events = POLLIN;
		pollfds[0].revents = 0;

		numPollFds = 1;

		overloaded = IsOverloaded(totalNumBytesToSend);
		int numBytesToSend;

		totalNumBytesToSend = 0;

		// 给每一个NetConnection建立一个poll
		for (iter = mConnections.begin(); iter != mConnections.end(); ++iter)
		{
			conn = *iter;
			fd = conn->GetFd();

			assert(fd> 0);
			if (fd < 0)
				continue;

			// 如果这个当前连接和第一个相同，则跳过（因为第一个已经通过NetKicker处理）
			if (fd == globals().netKicker.GetFd())
			{
				conn->mPollVectorIndex = -1;
				continue;
			}

			// 记录当前connection在poll向量中的下标
			conn->mPollVectorIndex = numPollFds;
			pollfds[numPollFds].fd = fd;
			pollfds[numPollFds].events = 0;
			pollfds[numPollFds].revents = 0;

			// 如果负载不是太高，可以进行读数据操作
			if (conn->IsReadReady(overloaded))
			{
				// By default, each connection is read ready.  We
				// expect there to be 2-way data transmission, and so
				// we are read ready.  In overloaded state, we only
				// add the fd to the poll vector if the fd is given a
				// special pass
				pollfds[numPollFds].events |= POLLIN;
			}

			// 统计该NetConnection中要发送的字节数，通过这个来判断该操作是读还是写
			numBytesToSend = conn->GetNumBytesToWrite();
			if (numBytesToSend > 0)
			{
				totalNumBytesToSend += numBytesToSend;
				// An optimization: if we are not sending any data for
				// this fd in this round of poll, don't bother adding
				// it to the poll vector.
				// 发送事件
				pollfds[numPollFds].events |= POLLOUT;
			}
			numPollFds++;
		}
		// 创建Connection和poll的对应关系结束

		// 如果原来状态没有过载，而现在过载（当前连接中的将要发送的字节数过多）了，
		// 则继续下一次循环
		if (!overloaded)
		{
			overloaded = IsOverloaded(totalNumBytesToSend);
			if (overloaded)
				continue;
		}

		// 发送数据并且获取开始发送和发送结束的时间（若发送成功）
		struct timeval startTime, endTime;

		gettimeofday(&startTime, NULL);

		pollTimeoutMs = mSelectTimeout.tv_sec * 1000;
		// 在规定时间内检测所有事件是否要发生
		res = poll(pollfds.get(), numPollFds, pollTimeoutMs);

		// 有错误发生
		if ((res < 0) && (errno != EINTR))
		{
			perror("poll(): ");
			continue;
		}

		gettimeofday(&endTime, NULL);

		/*
		 if (res == 0) {
		 float timeTaken = (endTime.tv_sec - startTime.tv_sec) +
		 (endTime.tv_usec - startTime.tv_usec) * 1e-6;

		 KFS_LOG_VA_DEBUG("Select returned 0 and time blocked: %f", timeTaken);
		 }
		 */
		// list of timeout handlers...call them back

		fd = globals().netKicker.GetFd();
		if (pollfds[0].revents & POLLIN)
		{
			// 删除管道中的数据（管道中的数据从何而来？）
			globals().netKicker.Drain();
			// 处理已经完成的IO
			globals().diskManager.ReapCompletedIOs();
		}

		// 检测并执行所有的超时事件
		for_each(mTimeoutHandlers.begin(), mTimeoutHandlers.end(), mem_fun(
				&ITimeout::TimerExpired));

		iter = mConnections.begin();
		while (iter != mConnections.end())
		{
			conn = *iter;
			// 如果是第0个Connection（专门用来处理管道的）或者出错了的Connection，
			// 则跳过处理下一个Connection
			if ((conn->GetFd() == globals().netKicker.GetFd())
					|| (conn->mPollVectorIndex < 0))
			{
				++iter;
				continue;
			}

			if (pollfds[conn->mPollVectorIndex].revents & POLLIN)
			{	// 如果当前事件可以发生(Ready to read)，则处理该事件
				fd = conn->GetFd();
				if (fd > 0)
				{
					conn->HandleReadEvent(overloaded);
				}
			}
			// conn could have closed due to errors during read.  so,
			// need to re-get the fd and check that all is good
			if (pollfds[conn->mPollVectorIndex].revents & POLLOUT)
			{	// 如果当前事件可以发生(Ready to write)，则处理该事件
				fd = conn->GetFd();
				if (fd > 0)
				{
					conn->HandleWriteEvent();
				}
			}

			// 如果Connection出现错误或者被挂起
			if ((pollfds[conn->mPollVectorIndex].revents & POLLERR)
					|| (pollfds[conn->mPollVectorIndex].revents & POLLHUP))
			{
				fd = conn->GetFd();
				if (fd > 0)
				{
					conn->HandleErrorEvent();
				}
			}

			// Something happened and the connection has closed.  So,
			// remove the connection from our list.
			// 删除错误的连接
			if (conn->GetFd() < 0)
			{
				KFS_LOG_DEBUG("Removing fd from poll list");
				eltToRemove = iter;
				++iter;
				mConnections.erase(eltToRemove);
			}
			else
			{
				++iter;
			}
		}

	}
}

// 系统是否过载：发送字节数多于mMaxOutgoingBacklog时，负载过大；发送字节数小于
// mMaxOutgoingBacklog / 2时，系统过载标记清除
bool NetManager::IsOverloaded(int64_t numBytesToSend)
{
	static bool wasOverloaded = false;

	// 现判断网络是否过载
	if (mMaxOutgoingBacklog > 0)
	{
		if (!mNetworkOverloaded)
		{
			mNetworkOverloaded = (numBytesToSend > mMaxOutgoingBacklog);
		}
		else if (numBytesToSend <= mMaxOutgoingBacklog / 2)
		{
			// network was overloaded and that has now cleared
			mNetworkOverloaded = false;
		}
	}

	// 磁盘过载或者网络过载都可以导致系统过载
	bool isOverloaded = mDiskOverloaded || mNetworkOverloaded;

	// 原来不过载，现在过载了
	if (!wasOverloaded && isOverloaded)
	{
		KFS_LOG_VA_INFO("System is now in overloaded state (%ld bytes to send; %d disk IO's) ",
				numBytesToSend, globals().diskManager.NumDiskIOOutstanding());
	}
	// 原来过载，现在过载状态取消
	else if (wasOverloaded && !isOverloaded)
	{
		KFS_LOG_VA_INFO("Clearing system overload state (%ld bytes to send; %d disk IO's)",
				numBytesToSend, globals().diskManager.NumDiskIOOutstanding());
	}
	wasOverloaded = isOverloaded;
	return isOverloaded;
}

void NetManager::ChangeDiskOverloadState(bool v)
{
	if (mDiskOverloaded == v)
		return;
	mDiskOverloaded = v;
}
