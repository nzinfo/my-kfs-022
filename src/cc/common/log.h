//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: log.h 71 2008-07-07 15:49:14Z sriramsrao $
//
// Created 2007/10/17
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
// \brief A logging facility that uses log4cpp.
//
//----------------------------------------------------------------------------

#ifndef COMMON_LOG_H
#define COMMON_LOG_H

#define LOG4CPP_FIX_ERROR_COLLISION 1
#include <log4cpp/Category.hh>
#include <log4cpp/Priority.hh>

namespace KFS
{
// Have a singleton logger for an application
// 一个程序中就使用一个Logger
class MsgLogger
{
private:
	MsgLogger();
	MsgLogger(const MsgLogger &other);
	const MsgLogger & operator =(const MsgLogger &other);
public:
	static log4cpp::Category *logger;

	// log4cpp中权限： NOTSET < DEBUG < INFO < NOTICE < WARN < ERROR < CRIT
	// < ALERT < FATAL = EMERG
#ifdef NDEBUG
	static void Init(const char *filename)
	{
		Init(filename, log4cpp::Priority::INFO);
	}
#else
	// filename指定文件存放的地址，如果不指定filename，则输出到ostream中
	static void Init(const char *filename)
	{
		Init(filename, log4cpp::Priority::DEBUG);
	}
#endif
	static void Init(const char *filename, log4cpp::Priority::Value priority);

	static void SetLevel(log4cpp::Priority::Value priority);

};

/// 各种权限的日志发布过程
/// 相关API见: http://log4cpp.sourceforge.net/api/classlog4cpp_1_1Category.html

// 调试
#ifndef KFS_LOG_DEBUG
#define KFS_LOG_DEBUG(msg) MsgLogger::logger->debug("(%s:%d) " msg, __FILE__, __LINE__);
#define KFS_LOG_VA_DEBUG(msg, ...) \
	do { \
         MsgLogger::logger->debug("(%s:%d) " msg, __FILE__, __LINE__, __VA_ARGS__); \
        } while(0)
#endif

// 信息
#ifndef KFS_LOG_INFO
#define KFS_LOG_INFO(msg) MsgLogger::logger->info("(%s:%d) " msg, __FILE__, __LINE__);
#define KFS_LOG_VA_INFO(msg, ...) \
	do { \
         MsgLogger::logger->info("(%s:%d) " msg, __FILE__, __LINE__, __VA_ARGS__); \
        } while(0)
#endif

// 警告
#ifndef KFS_LOG_WARN
#define KFS_LOG_WARN(msg) MsgLogger::logger->warn("(%s:%d) " msg, __FILE__, __LINE__);
#define KFS_LOG_VA_WARN(msg, ...) \
	do { \
         MsgLogger::logger->warn("(%s:%d) " msg, __FILE__, __LINE__, __VA_ARGS__); \
        } while(0)
#endif

// 一般错误
#ifndef KFS_LOG_ERROR
#define KFS_LOG_ERROR(msg) MsgLogger::logger->error("(%s:%d) " msg, __FILE__, __LINE__);
#define KFS_LOG_VA_ERROR(msg, ...) \
	do { \
         MsgLogger::logger->error("(%s:%d) " msg, __FILE__, __LINE__, __VA_ARGS__); \
        } while(0)
#endif

// 严重错误
#ifndef KFS_LOG_FATAL
#define KFS_LOG_FATAL(msg) MsgLogger::logger->fatal("(%s:%d) " msg, __FILE__, __LINE__);
#define KFS_LOG_VA_FATAL(msg, ...) \
	do { \
         MsgLogger::logger->fatal("(%s:%d) " msg, __FILE__, __LINE__, __VA_ARGS__); \
        } while(0)
#endif

}

#endif // COMMON_LOG_H
