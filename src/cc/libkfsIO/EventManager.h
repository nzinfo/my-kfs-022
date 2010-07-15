//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: EventManager.h 71 2008-07-07 15:49:14Z sriramsrao $
//
// Created 2006/03/31
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

#ifndef _LIBKFSIO_EVENTMANAGER_H
#define _LIBKFSIO_EVENTMANAGER_H

#include "Event.h"
#include "ITimeout.h"
#include "NetManager.h"

namespace KFS
{

///
/// @file EventManager.h
/// @brief EventManager supports execution of time-based events.
/// Events can be scheduled at milli-second granularity and they are
/// notified whenever the time to execute them arises.
///

class EventManagerTimeoutImpl;

/// 这个类只能有一个实例，在调用init()时注册到netManager中
/// 事件管理器，管理每一个事件发生的开始时间等
class EventManager
{
public:

	static const int MAX_EVENT_SLOTS = 2000;
	/// 10 ms granularity for events
	/// 时间的计算精度，每10ms为一个阶段
	static const int EVENT_GRANULARITY_MS = 10;

	EventManager();
	~EventManager();

	///
	/// Schedule an event for execution.  If the event is periodic, it
	/// will be re-scheduled for execution.
	/// @param[in] event A reference to the event that has to be scheduled.
	/// @param[in] afterMs # of milli-seconds after which the event
	/// should be executed.
	///
	/// 定时器：系统将在afterMs毫秒后执行事件event
	void Schedule(EventPtr &event, int afterMs);

	/// Register a timeout handler with the NetManager.  The
	/// NetManager will call the handler whenever a timeout occurs.
	void Init();

	/// Whenever a timeout occurs, walk the list of scheduled events
	/// to determine if any need to be signaled.
	void Timeout();

private:

	EventManagerTimeoutImpl *mEventManagerTimeoutImpl;

	/// Events are held in a calendar queue: The calendar queue
	/// consists of a circular array of "slots".  Slots are a
	/// milli-second apart.  At each timeout, the list of events in
	/// the current slot are signaled.
	/// 记录每一个时间片所要处理的事件
	/// 这是一个任务列表的数组，每一个index对应一组任务
	std::list<EventPtr> mSlots[MAX_EVENT_SLOTS];

	/// Index into the above array that points to where we are
	/// currently.
	int mCurrentSlot;

	/// Events for which the time of occurence is after the last slot
	/// (i.e., after 20 seconds).
	/// 存储等待时间超过MAX_EVENT_SLOTS * EVENT_GRANULARITY_MS的事件列表
	std::list<EventPtr> mLongtermEvents;
};

///
/// @class EventManagerTimeoutImpl
/// @brief Implements the ITimeout interface (@see ITimeout).
///
/// 带Timeout的EventManager
/// 构造该类的实例时，需指定该类所在的母类，故该类不能脱离母类存在
class EventManagerTimeoutImpl: public ITimeout
{
public:
	/// The owning object of a EventManagerTimeoutImpl is the EventManager.
	EventManagerTimeoutImpl(EventManager *mgr)
	{
		mEventManager = mgr;
	}
	;
	/// Callback the owning object whenever a timeout occurs.
	/// 调用本实例所在的母类的Timeout()函数，最终通过这个函数起作用
	virtual void Timeout()
	{
		mEventManager->Timeout();
	}
	;
private:
	/// Owning object.
	// 指定当前实例所在的母类，方便调用
	EventManager *mEventManager;

};

}

#endif // _LIBKFSIO_EVENTMANAGER_H
