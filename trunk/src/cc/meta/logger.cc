/*!
 * $Id: logger.cc 71 2008-07-07 15:49:14Z sriramsrao $
 *
 * Copyright 2008 Quantcast Corp.
 * Copyright 2006-2008 Kosmix Corp.
 *
 * This file is part of Kosmos File System (KFS).
 *
 * Licensed under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * \file logger.cc
 * \brief thread for logging metadata updates
 * \author Blake Lewis (Kosmix Corp.)
 */

#include <csignal>

#include "logger.h"
#include "queue.h"
#include "checkpoint.h"
#include "util.h"
#include "replay.h"
#include "common/log.h"
#include "libkfsIO/Globals.h"

using namespace KFS;

// default values
string KFS::LOGDIR("./kfslog");
string KFS::LASTLOG(LOGDIR + "/last");

// 用于记录整个系统的日志
Logger KFS::oplog(LOGDIR);

/*!
 * \brief log the request and flush the result to the fs buffer.
 */
int Logger::log(MetaRequest *r)
{
	// 将r的信息写入日志(内存中)
	int res = r->log(file);
	// 如果获取该请求的信息没有错误, 则将该请求的日志写入磁盘(先写入内存, 接着写入磁盘)
	if (res >= 0)
		flushResult(r);
	return res;
}

/*!
 * \brief flush log entries to disk
 *
 * Make sure that all of the log entries are on disk and
 * update the highest sequence number logged.
 */
// 将内存中的日志写入日志文件(磁盘)
void Logger::flushLog()
{
	// 获取下一个序号
	thread.lock();
	seq_t last = nextseq;
	thread.unlock();

	// 将内存中的日志写入文件
	file.flush();
	if (file.fail())
		panic("Logger::flushLog", true);

	thread.lock();
	committed = last;
	thread.unlock();
}

/*!
 * \brief set the log filename/log # to seqno
 * \param[in] seqno	the next log sequence number (lognum)
 */
/// 设置日志的序号, 进而由该序号生成当前日志文件的名称
void Logger::setLog(int seqno)
{
	assert(seqno >= 0);
	lognum = seqno;
	logname = logfile(lognum);
}

/*!
 * \brief open a new log file for writing
 * \param[in] seqno	the next log sequence number (lognum)
 * \return		0 if successful, negative on I/O error
 */
/// 启动一个新的日志文件(指定该日志文件的next log sequence), 并打开该日志文件
/// @note 如果该文件已经存在, 则以扩充的模式打开.
int Logger::startLog(int seqno)
{
	assert(seqno >= 0);
	lognum = seqno;
	logname = logfile(lognum);
	// 如果该文件已经存在, 则以append的方式打开
	if (file_exists(logname))
	{
		// following log replay, until the next CP, we
		// should continue to append to the logfile that we replayed.
		// seqno will be set to the value we got from the chkpt file.
		// So, don't overwrite the log file.
		KFS_LOG_VA_DEBUG("Opening %s in append mode", logname.c_str());
		file.open(logname.c_str(), std::ios_base::app);
		return (file.fail()) ? -EIO : 0;
	}

	// 如果该日志文件不存在, 则创建该文件, 并写入日志文件头
	file.open(logname.c_str());
	file << "version/" << VERSION << '\n';

	// for debugging, record when the log was opened
	time_t t = time(NULL);

	file << "time/" << ctime(&t);
	return (file.fail()) ? -EIO : 0;
}

/*!
 * \brief close current log file and begin a new one
 */
/// 关闭当前的日志文件, 开始一个新的日志文件
int Logger::finishLog()
{
	thread.lock();
	// for debugging, record when the log was closed
	time_t t = time(NULL);

	file << "time/" << ctime(&t);
	file.close();
	link_latest(logname, LASTLOG);
	if (file.fail())
		warn("link_latest", true);
	incp = committed;
	int status = startLog(lognum + 1);

	// 该checkpoint的修改次数为0, 此时则没有必要进行checkpoint
	cp.resetMutationCount();
	thread.unlock();
	// cp.start_CP();
	return status;
}

/*!
 * \brief make sure result is on disk
 * \param[in] r	the result of interest
 *
 * If this result has a higher sequence number than what is
 * currently known to be on disk, flush the log to disk.
 */
/// 如果当前请求的序号大于磁盘上已知的日志文件的最大序号, 则将日志写入磁盘(日志文件)
void Logger::flushResult(MetaRequest *r)
{
	if (r->seqno > committed)
	{
		flushLog();
		assert(r->seqno <= committed);
	}
}

/*!
 * \brief return next available result
 * \return result that was at the head of the result queue
 *
 * Return the next available result.
 */
// logged中的下一个元素(MetaRequest)
MetaRequest *Logger::next_result()
{
	MetaRequest *r = logged.dequeue();
	return r;
}

/*!
 * \brief return next result, non-blocking
 *
 * Same as Logger::next_result() except that it returns NULL
 * if the result queue is empty.
 */
// 升级版的next_result(): 如果logged为空, 则返回NULL.
MetaRequest *Logger::next_result_nowait()
{
	MetaRequest *r = logged.dequeue_nowait();
	return r;
}

/*!
 * \brief logger main loop
 *
 * Pull requests from the pending queue and call the appropriate
 * log routine to write them into the log file.  Note any mutations
 * in the checkpoint structure (so that it will realize that there
 * are changes to record), and finally, move the request onto the
 * result queue for return to the clients.  Before the result is released
 * to the network dispatcher, we flush the log.
 */
/// 将等待队列中的操作写入日志文件
void * logger_main(void *dummy)
{
	for (;;)
	{
		// 获取logger的pending队列中的下一个请求
		MetaRequest *r = oplog.get_pending();

		// 是否要切换至新的日志文件
		bool is_cp = (r->op == META_LOG_ROLLOVER);

		// 如果该请求修改了metatree的信息并且成功完成, 则把该请求记入日志
		if (r->mutation && r->status == 0)
		{
			oplog.log(r);
			if (!is_cp)
			{
				// 如果这个操作不是checkpoint, 则从上一个checkpoint开始系统的修改次数+1
				cp.note_mutation();
			}
		}
		if (is_cp)
		{
			// 如果是checkpoint操作, 则在cpdone中加入该操作
			oplog.save_cp(r);
		}
		else
		{
			// 否则, 加入到已经完成日志记录的Request中
			oplog.add_logged(r);
		}

		// 如果没有需要进行日志记录的Request了...
		if (oplog.isPendingEmpty())
			// notify the net-manager that things are ready to go
			libkfsio::globals().netKicker.Kick();
	}
	return NULL;
}

/// 用于切换到新的日志文件: 每个日志文件使用600秒
void * logtimer(void *dummy)
{
	int status, sig;
	sigset_t sset;

	// 初始化并清空sset集合
	sigemptyset(&sset);
	// 将SIGALRM添加到信号集合当中
	sigaddset(&sset, SIGALRM);

	alarm(LOG_ROLLOVER_MAXSEC);

	// 启动定时器: 每600秒切换一个新的日志文件
	for (;;)
	{
		// 因为在系统初始化的时候执行了sigprocmask(), 所以系统不会在这里退出
		status = sigwait(&sset, &sig);
		if (status == EINTR) // happens under gdb for some reason
			continue;
		assert(status == 0 && sig == SIGALRM);
		alarm(LOG_ROLLOVER_MAXSEC);
		MetaLogRollover logreq;
		// if the metatree hasn't been mutated, avoid a log file
		// rollover
		if (!cp.isCPNeeded())
			continue;
		submit_request(&logreq);
		(void) oplog.wait_for_cp();
	}

	return NULL;
}

/// 设置日志文件的存放目录
void KFS::logger_setup_paths(const string &logdir)
{
	if (logdir != "")
	{
		LOGDIR = logdir;
		LASTLOG = LOGDIR + "/last";
		oplog.setLogDir(LOGDIR);
	}
}

/// 日志系统初始化:
void KFS::logger_init()
{
	if (oplog.startLog(replayer.logno()) < 0)
		panic("KFS::logger_init, startLog", true);
	// 启动日志记录线程: 不断扫描等待记录日志的队列, 将需要写入日志的操作写入队列.
	oplog.start(logger_main);
	// use a timer to rotate logs
	// 启动定时器, 每600秒切换一个新的日志文件.
	oplog.start_timer(logtimer);
}
