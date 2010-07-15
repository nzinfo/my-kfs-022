//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: Event.h 71 2008-07-07 15:49:14Z sriramsrao $
//
// Created 2006/03/22
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

#ifndef _LIBKFSIO_EVENT_H
#define _LIBKFSIO_EVENT_H

#include "KfsCallbackObj.h"
#include <boost/shared_ptr.hpp>

namespace KFS
{
///
/// \enum EventCode_t
/// Various event codes that a KfsCallbackObj is notified with when
/// events occur.
///
enum EventCode_t
{
	EVENT_NEW_CONNECTION,	// 新建连接
	EVENT_NET_READ,			// 网络读
	EVENT_NET_WROTE,		// 网络写
	EVENT_NET_ERROR,		// 网络错误
	EVENT_DISK_READ,		// 磁盘读
	EVENT_DISK_WROTE,		// 磁盘写
	EVENT_DISK_ERROR,		// 磁盘错误
	EVENT_SYNC_DONE,		// 同步完成
	EVENT_CMD_DONE,			// 命令完成
	EVENT_TIMEOUT			// 超时
};

///
/// @enum EventStatus_t
/// @brief Code corresponding to the status of an event:
/// scheduled/done/cancelled.
///
enum EventStatus_t
{
	EVENT_STATUS_NONE, EVENT_SCHEDULED, EVENT_DONE, EVENT_CANCELLED
};

class Event
{
public:
	Event(KfsCallbackObj *callbackObj, void *data, int timeoutMs, bool periodic)
	{
		mCallbackObj = callbackObj;
		mEventData = data;
		mEventStatus = EVENT_STATUS_NONE;
		mTimeoutMs = timeoutMs;
		mPeriodic = periodic;
		mLongtermWait = 0;
	}
	;

	~Event()
	{
		// 队列中的事件是不可以销毁的！
		assert(mEventStatus != EVENT_SCHEDULED);
		Cancel();
		mEventData = NULL;
	}

	void SetStatus(EventStatus_t status)
	{
		mEventStatus = status;
	}

	/// 执行事件，调用CallbackObj中的基类的execute函数
	int EventOccurred()
	{
		if (mEventStatus == EVENT_CANCELLED)
			return 0;
		mEventStatus = EVENT_DONE;

		// 调用基类定义个execute虚函数
		return mCallbackObj->HandleEvent(EVENT_TIMEOUT, mEventData);
	}

	void Cancel()
	{
		mEventStatus = EVENT_CANCELLED;
	}

	bool IsPeriodic()
	{
		return mPeriodic;
	}

	int GetTimeout()
	{
		return mTimeoutMs;
	}

	void SetLongtermWait(int waitMs)
	{
		mLongtermWait = waitMs;
	}

	/// 减少等待时间
	int DecLongtermWait(int numMs)
	{
		mLongtermWait -= numMs;
		if (mLongtermWait < 0)
			mLongtermWait = 0;
		return mLongtermWait;
	}

private:
	/// 事件状态：队列中，已完成，已撤销
	EventStatus_t mEventStatus;
	/// 回调函数体，封装了一个函数
	KfsCallbackObj *mCallbackObj;
	/// 执行事件所需要的数据
	void *mEventData;
	/// 事件超时时间
	int mTimeoutMs;
	/// 该时间是否是周期循环的
	bool mPeriodic;
	int mLongtermWait;
};

typedef boost::shared_ptr<Event> EventPtr;

}

#endif // _LIBKFSIO_EVENT_H
