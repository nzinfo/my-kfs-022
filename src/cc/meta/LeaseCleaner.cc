//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: LeaseCleaner.cc 71 2008-07-07 15:49:14Z sriramsrao $
//
// Created 2006/10/16
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


#include "LeaseCleaner.h"
#include "libkfsIO/Globals.h"
#include <cassert>

using namespace KFS;
using namespace libkfsio;

// 设置回调函数, 将定时器注册到netManager中
LeaseCleaner::LeaseCleaner() :
	mInProgress(false), mOp(1, this)
{
	SET_HANDLER(this, &LeaseCleaner::HandleEvent);
	/// setup a periodic event to do the cleanup
	mTimer = new LeaseCleanerTimeoutImpl(this);
	globals().netManager.RegisterTimeoutHandler(mTimer);
}

LeaseCleaner::~LeaseCleaner()
{
	globals().netManager.UnRegisterTimeoutHandler(mTimer);
	delete mTimer;
}

/// Use the main looop to submit the cleanup request.
/// 处理事件: 命令完成事件和超时事件
int LeaseCleaner::HandleEvent(int code, void *data)
{
	static seq_t seqNum = 1;
	switch (code)
	{
	case EVENT_CMD_DONE:
		mInProgress = false;
		return 0;
	case EVENT_TIMEOUT:
		if (mInProgress)
			return 0;

		mOp.opSeqno = seqNum;
		++seqNum;
		mInProgress = true;
		submit_request(&mOp);
		return 0;
	default:
		assert(!"Unknown event");
		break;
	}
	return 0;
}
