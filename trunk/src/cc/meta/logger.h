/*!
 * $Id: logger.h 71 2008-07-07 15:49:14Z sriramsrao $
 *
 * \file logger.h
 * \brief metadata logger
 * \author Blake Lewis (Kosmix Corp.)
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
 */
#if !defined(KFS_LOGGER_H)
#define KFS_LOGGER_H

#include <fstream>
#include <sstream>
#include <string>

#include "kfstypes.h"
#include "queue.h"
#include "thread.h"
#include "request.h"
#include "util.h"

using std::string;
using std::ofstream;

namespace KFS
{

/*!
 * \brief Class for logging metadata updates
 *
 * This class consists of two threads:
 *  - one thread that writes the updates to the logs and then dispatches the
 *  logged results to the network thread
 *  - another thread that runs a timer to cause periodic log rollover.  Whenever
 *  the log rollover occurs, after we close the log file, we create a link from
 *  "LAST" to the recently closed log file.  This is used by the log compactor
 *  to determine the set of files that can be compacted.
 */
class Logger
{
	/// 日志存放目录
	string logdir; //!< directory where logs are kept
	/// 用于生成日志文件名称
	int lognum; //!< for generating log file names
	/// 当前正在使用的日志文件(不包含完整路径)
	string logname; //!< name of current log file
	/// 用于写日志文件
	ofstream file; //!< the current log file
	/// 下一个请求的序号???
	seq_t nextseq; //!< next request sequence no.
	/// 磁盘上所有日志文件的最大的序列号
	seq_t committed; //!< highest request known to be on disk
	/// 请求的日志的最大序号
	seq_t incp; //!< highest request in a checkpoint
	/// 还没有写入日志文件的请求的队列
	MetaQueue<MetaRequest> pending; //!< list of still-unlogged results
	/// 已经写入日志的请求的队列
	MetaQueue<MetaRequest> logged; //!< list of logged results
	/// 完成checkpoint的请求列表
	MetaQueue<MetaRequest> cpdone; //!< completed CP (aka log rollover)
	/// 用来扫描日志系统中是否有Request要写入日志的线程(logger_main)
	MetaThread thread; //!< thread synchronization
	/// 用来更换新的日志文件的定时器, 每600秒切换一次(logtimer)
	MetaThread timer; //!< timer to rollover log files
	/// 根据n创建一个日志文件名称(此时不创建日志文件): logdir/log.XXX
	string genfile(int n) //!< generate a log file name
	{
		std::ostringstream f(std::ostringstream::out);
		f << n;
		return logdir + "/log." + f.str();
	}
	/// 将内存中的日志写入日志文件(磁盘)
	void flushLog();
	/// 如果当前请求的序号大于磁盘上已知的日志文件的最大序号, 则调用flushLog()
	void flushResult(MetaRequest *r);
public:
	static const int VERSION = 1;
	Logger(string d) :
		logdir(d), lognum(-1), nextseq(0), committed(0)
	{
	}
	~Logger()
	{
		file.close();
	}
	void setLogDir(const string &d)
	{
		logdir = d;
	}

	string logfile(int n) //!< generate a log file name
	{
		return makename(logdir, "log", n);
	}
	/*!
	 * \brief check whether request is stored on disk
	 * \param[in] r the request of interest
	 * \return	whether it is on disk
	 */
	bool iscommitted(MetaRequest *r)
	{
		return r->seqno != 0 && r->seqno <= committed;
	}
	//!< log a request
	int log(MetaRequest *r);
	void add_pending(MetaRequest *r)
	{
		pending.enqueue(r);
	}
	/*!
	 * \brief get a pending request and assign it a sequence number
	 * \return the request
	 */
	MetaRequest *get_pending()
	{
		MetaRequest *r = pending.dequeue();
		r->seqno = ++nextseq;
		return r;
	}
	bool isPendingEmpty()
	{
		return pending.empty();
	}
	MetaRequest *next_result();
	MetaRequest *next_result_nowait();
	seq_t checkpointed()
	{
		return incp;
	} //!< highest seqno in CP
	void add_logged(MetaRequest *r)
	{
		logged.enqueue(r);
	}
	void save_cp(MetaRequest *r)
	{
		cpdone.enqueue(r);
	}
	MetaRequest *wait_for_cp()
	{
		return cpdone.dequeue();
	}
	void start(MetaThread::thread_start_t func) //!< start thread
	{
		thread.start(func, NULL);
	}
	void setLog(int seqno); //!< set the log filename based on seqno
	int startLog(int seqno); //!< start a new log file
	int finishLog(); //!< tie off log file before CP
	const string name() const
	{
		return logname;
	} //!< name of log file
	/*!
	 * \brief set initial sequence numbers at startup
	 * \param[in] last last sequence number from checkpoint or log
	 */
	void set_seqno(seq_t last)
	{
		incp = committed = nextseq = last;
	}
	/*!
	 * Use a timer to rollover the log files every N minutes
	 */
	void start_timer(MetaThread::thread_start_t func)
	{
		timer.start(func, NULL);
	}
};

extern string LOGDIR;	// 存放日志的目录
extern string LASTLOG;	// LOGDIR/last
/// 切换日志文件的时间: 每600秒启用一个新的日志文件
const unsigned int LOG_ROLLOVER_MAXSEC = 600; //!< max. seconds between CP's/log rollover
/// 日志系统的主体
extern Logger oplog;
extern void logger_setup_paths(const string &logdir);
extern void logger_init();
extern MetaRequest *next_result();

}
#endif // !defined(KFS_LOGGER_H)
