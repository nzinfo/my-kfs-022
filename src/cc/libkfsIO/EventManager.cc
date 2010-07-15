//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: EventManager.cc 71 2008-07-07 15:49:14Z sriramsrao $
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

#include "EventManager.h"
#include "Globals.h"

using std::list;
using namespace KFS;
using namespace KFS::libkfsio;

EventManager::EventManager()
{
	mCurrentSlot = 0;

	// 构造EventManagerTimeoutImpl时，需指定其所在的父类。
	mEventManagerTimeoutImpl = new EventManagerTimeoutImpl(this);
}

EventManager::~EventManager()
{
	for (int i = 0; i < MAX_EVENT_SLOTS; i++)
		mSlots[i].clear();

	mLongtermEvents.clear();
}

void EventManager::Init()
{
	// The event manager schedules events at 10ms granularity.
	mEventManagerTimeoutImpl->SetTimeoutInterval(EVENT_GRANULARITY_MS);
	// 设置全局量中netManager的超时处理事件为当前类
	globals().netManager.RegisterTimeoutHandler(mEventManagerTimeoutImpl);
}

void EventManager::Schedule(EventPtr &event, int afterMs)
{
	int slotDelta;

	assert(afterMs >= 0);

	event->SetStatus(EVENT_SCHEDULED);

	// 获取需要等待的时间片
	if (afterMs <= 0)
		slotDelta = 1;
	else
		slotDelta = afterMs / EVENT_GRANULARITY_MS;

	if (slotDelta > MAX_EVENT_SLOTS)
	{	// 需要等待一个或者多个处理完所有事件锁需要的时间片
		event->SetLongtermWait(afterMs);
		mLongtermEvents.push_back(event);
		return;
	}
	int slot = (mCurrentSlot + slotDelta) % MAX_EVENT_SLOTS;
	// 当前事件将在slot处开始执行
	mSlots[slot].push_back(event);

}

void EventManager::Timeout()
{
	list<EventPtr>::iterator iter, eltToRemove;
	EventPtr event;
	int waitMs;
	int msElapsed = mEventManagerTimeoutImpl->GetTimeElapsed();

	for (iter = mSlots[mCurrentSlot].begin(); iter
			!= mSlots[mCurrentSlot].end(); ++iter)
	{	// 处理所有在这一个时间片应该发生的事件。
		event = *iter;
		event->EventOccurred();
		// Reschedule it if it is a periodic event
		// 若该事件是周期性的，则重新安排时间片以供其运行
		if (event->IsPeriodic())
			Schedule(event, event->GetTimeout());

	}
	mSlots[mCurrentSlot].clear();
	mCurrentSlot++;
	if (mCurrentSlot == MAX_EVENT_SLOTS)
		mCurrentSlot = 0;

	/*
	 if ((mLongtermEvents.size() > 0) &&
	 (msElapsed - EVENT_GRANULARITY_MS >= 3 * EVENT_GRANULARITY_MS)) {
	 KFS_LOG_VA_DEBUG("Elapsed ms = %d", msElapsed);
	 }
	 */

	// Now, pull all the long-term events
	iter = mLongtermEvents.begin();
	while (iter != mLongtermEvents.end())
	{
		event = *iter;
		// count down for each ms that ticks by
		waitMs = event->DecLongtermWait(msElapsed);
		if (waitMs < 0)
			waitMs = 0;

		// 我觉得这里可以优化：if (waitMs >= MAX_EVENT_SLOTS * EVENT_GRANULARITY_MS)
		if (waitMs >= MAX_EVENT_SLOTS)
		{
			++iter;
			continue;
		}
		// we have counted down to "short-term" amount
		// 此时，到这个事件的发生只有不到一个时间片了，则可以安排此事件运行了
		Schedule(event, waitMs);
		eltToRemove = iter;
		++iter;
		mLongtermEvents.erase(eltToRemove);
	}
}
