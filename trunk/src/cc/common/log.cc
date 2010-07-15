//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: log.cc 165 2008-09-22 19:52:38Z sriramsrao $
//
// Created 2005/03/01
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
//----------------------------------------------------------------------------

#include "log.h"
#include <stdlib.h>
#include <log4cpp/RollingFileAppender.hh>
#include <log4cpp/OstreamAppender.hh>
#include <log4cpp/PatternLayout.hh>

using namespace KFS;

log4cpp::Category* KFS::MsgLogger::logger = NULL;

/*
 * 手动使用log4cpp的基本步骤如下：
 * 1. 实例化一个layout 对象；
 * 2. 初始化一个appender 对象；
 * 3. 把layout对象附着在appender对象上；
 * 4. 调用log4cpp::Category::getInstance("name"). 实例化一个category对象；
 * 5. 把appender对象附到category上（根据additivity的值取代其他appender或者附加
 * 	  在其他appender后）。
 * 6. 设置category的优先级；
 * 详细内容参考：http://www.ibm.com/developerworks/cn/linux/l-log4cpp/index.html
 */
/// 初始化Logger，filename指定要输入到的文件目录和名称
void MsgLogger::Init(const char *filename, log4cpp::Priority::Value priority)
{
	log4cpp::Appender* appender;
	// (*.*) 1. 创建一个layout
	log4cpp::PatternLayout* layout = new log4cpp::PatternLayout();
	// 使用自定义的日志格式
	layout->setConversionPattern("%d{%m-%d-%Y %H:%M:%S.%l} %p - %m %n");

	if (filename != NULL)
	{
		// set the max. log file size to be 100M before it rolls over
		// to the next; save the last 10 log files.
		// (*.*) 2. 初始化一个appender
		// log4cpp::RollingFileAppender: 输出到回卷文件，即当文件到达某个大小后回卷
		appender = new log4cpp::RollingFileAppender("default", std::string(
				filename), 100 * 1024 * 1024, 10);
	}
	else
		appender = new log4cpp::OstreamAppender("default", &std::cerr);

	// (*.*) 3. 把layout对象附着在appender对象上
	appender->setLayout(layout);

	// (*.*) 4. 初始化category对象
	logger = &(log4cpp::Category::getInstance(std::string("kfs")));

	// (*.*) 5. 将appender添加到category对象上
	logger->addAppender(appender);
	// logger->setAdditivity(false);

	// (*.*) 6. 设置category的优先级
	logger->setPriority(priority);
}

/// 设置category的优先级
void MsgLogger::SetLevel(log4cpp::Priority::Value priority)
{
	logger->setPriority(priority);
}
