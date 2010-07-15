//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: ChunkReplicator.cc 71 2008-07-07 15:49:14Z sriramsrao $
//
// Created 2007/01/18
// Author: Sriram Rao
//
// Copyright 2008 Quantcast Corp.
// Copyright 2007-2008 Kosmix Corp.
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

#include "ChunkReplicator.h"
#include "libkfsIO/Globals.h"
#include <cassert>

using namespace KFS;
using namespace libkfsio;

/// 创建一个ChunkReplicator: 设置回调函数, 创建一个超时处理进程, 注册到netManager中
ChunkReplicator::ChunkReplicator() :
	mInProgress(false), mOp(1, this)
{
	// 设置该ChunkReplicator的回调函数, 在timeout时调用
	SET_HANDLER(this, &ChunkReplicator::HandleEvent);
	mTimer = new ChunkReplicatorTimeoutImpl(this);
	globals().netManager.RegisterTimeoutHandler(mTimer);
}

/// 撤销该类时, 销毁注册在netManager中的timeout程序
ChunkReplicator::~ChunkReplicator()
{
	globals().netManager.UnRegisterTimeoutHandler(mTimer);
	delete mTimer;
}

/// Use the main loop to process the request.
/// 处理ChunkReplicator的事务, 合法事务包括检查完成和超时
int ChunkReplicator::HandleEvent(int code, void *data)
{
	static seq_t seqNum = 1;
	switch (code)
	{
	case EVENT_CMD_DONE:
		// 如果完成, 则设置mInProgress
		mInProgress = false;
		return 0;
	case EVENT_TIMEOUT:
		// 如果是超时, 则需要启动该处理函数
		if (mInProgress)
			return 0;
		mOp.opSeqno = seqNum;
		++seqNum;
		mInProgress = true;
		// 提交检测chunk冗余数的操作
		submit_request(&mOp);
		return 0;
	default:
		assert(!"Unknown event");
		break;
	}
	return 0;
}
