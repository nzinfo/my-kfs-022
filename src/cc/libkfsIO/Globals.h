//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: Globals.h 71 2008-07-07 15:49:14Z sriramsrao $
//
// Created 2006/10/09
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
// \brief Define the globals needed by the KFS IO library.  These
//globals are also available to any app that uses the KFS IO library.
//----------------------------------------------------------------------------

#ifndef LIBKFSIO_GLOBALS_H
#define LIBKFSIO_GLOBALS_H

#include "DiskManager.h"
#include "NetManager.h"
#include "EventManager.h"
#include "NetKicker.h"
#include "Counter.h"

namespace KFS
{
namespace libkfsio
{
/// 在系统中的每一个部分都可能会用到这个全局量，通过函数globals()唯一获得；
/// 每一个进程使用一个全局量
struct Globals_t
{
	/// 用来管理整个系统的本地磁盘读写操作
	DiskManager diskManager;
	/// 管理系统中的所有TcpSocket的事件
	NetManager netManager;
	/// 管理系统中的事件：排队，安排事件发生顺序等
	EventManager eventManager;
	/// 用来管理系统的全局计数器
	CounterManager counterManager;
	/// 要关注一下NetKicker管道中数据的来源！！！>>>
	NetKicker netKicker;
	/// Commonly needed counters
	/// 本地系统计数器（为什么有一个CounterManager了，还需要这些？？）

	Counter ctrOpenNetFds;
	Counter ctrOpenDiskFds;
	Counter ctrNetBytesRead;
	Counter ctrNetBytesWritten;
	Counter ctrDiskBytesRead;
	Counter ctrDiskBytesWritten;
	/// track the # of failed read/writes
	Counter ctrDiskIOErrors;
};

void InitGlobals();

/// 在获取该类的一个实例时调用这个函数，则总是能得到同一个实例
Globals_t & globals();
}
}

#endif // LIBKFSIO_GLOBALS_H
