/*!
 * $Id: startup.cc 153 2008-09-17 19:08:16Z sriramsrao $
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
 *
 * \file startup.cc
 * \brief code for starting up the metadata server
 * \author Blake Lewis and Sriram Rao
 *
 */
#include "startup.h"
#include "thread.h"
#include "logger.h"
#include "checkpoint.h"
#include "kfstree.h"
#include "request.h"
#include "restore.h"
#include "replay.h"
#include "util.h"
#include "LayoutManager.h"

#include <cassert>

extern "C"
{
#include <sys/resource.h>
#include <signal.h>
}

using namespace KFS;

/*!
 * \brief rebuild the metatree at startup
 *
 * If the latest CP file exists, use it to restore the contents
 * of the metatree, otherwise create a fresh tree with just "/"
 * and its associated "." and ".." links.
 *
 * Eventually, we should include an option here to restore from
 * a specified CP file instead of just the latest.
 *
 * After restoring from the checkpoint file, apply any log
 * records that are from after the CP.
 */
// 初始化全局树, 如果checkpoint文件存在, 则从checkpoint文件中恢复树; 否则, 创建文件系统
// 的根目录
static int setup_initial_tree(uint32_t minNumReplicasPerFile)
{
	string logfile;
	int status;
	if (file_exists(LASTCP))
	{
		Restorer r;
		// 从checkpoint文件恢复树, 并记录恢复时间
		status = r.rebuild(LASTCP, minNumReplicasPerFile) ? 0 : -EIO;
		gLayoutManager.InitRecoveryStartTime();
	}
	else
	{
		// 创建根目录
		status = metatree.new_tree();
	}
	return status;
}

static MetaThread request_processor; //<! request processing thread

/*!
 * \brief request-processing main loop
 */
static void *request_consumer(void *dummy)
{
	for (;;)
	{
		process_request();
	}
	return NULL;
}

/*!
 * \brief call init functions and start threads
 *
 * Before starting any threads, block SIGALRM so that it is caught
 * only by the checkpoint timer thread; since the start_CP code
 * acquires locks, we have to be careful not to call it asynchronously
 * in other thread contexts.  Afterwards, initialize metadata request
 * handlers and start the various helper threads going.
 *
 * XXX Eventually, we will want more options here, for instance,
 * specifying a checkpoint file instead of just using "latest".
 */
/*
 * 启动metaserver(需要进行的工作如下):
 */
void KFS::kfs_startup(const string &logdir, const string &cpdir,
		uint32_t minChunkServers, uint32_t numReplicasPerFile)
{
	struct rlimit rlim;

	// 资源限制, 限制该进程可以打开的最大文件描述符
	int status = getrlimit(RLIMIT_NOFILE, &rlim);

	// 1. 将该进程可以打开的最大文件描述符调到最大
	if (status == 0)
	{
		// bump up the # of open fds to as much as possible
		rlim.rlim_cur = rlim.rlim_max;
		setrlimit(RLIMIT_NOFILE, &rlim);
	}
	std::cout << "Setting the # of open files to: " << rlim.rlim_cur
			<< std::endl;

	// 2. 设置信号阻塞(将SIGALRM阻塞), 用于实现alarm的定时器(日志部分).
	sigset_t sset;
	sigemptyset(&sset);
	sigaddset(&sset, SIGALRM);
	status = sigprocmask(SIG_BLOCK, &sset, NULL);
	if (status != 0)
		panic("kfs_startup: sigprocmask", true);


	// get the paths setup before we get going
	// 3. 建立日志文件和checkpoint文件的路径
	logger_setup_paths(logdir);
	checkpointer_setup_paths(cpdir);

	// 4. 初始化metatree: 如果checkpoint文件存在, 则进行恢复; 否则, 创建文件系统根目录
	status = setup_initial_tree(numReplicasPerFile);
	if (status != 0)
		panic("setup_initial_tree failed", false);

	// 5. 重做由上一个checkpoint开始的所有Logged操作
	status = replayer.playAllLogs();
	if (status != 0)
		panic("log replay failed", false);

	// 6. 设置increment number
	ChangeIncarnationNumber(NULL);

	// 7. 设置MinChunkserversToExitRecovery. ???
	gLayoutManager.SetMinChunkserversToExitRecovery(minChunkServers);

	// 8. 清空回收站
	// empty the dumpster dir on startup; if it doesn't exist, create it
	// whatever is in the dumpster needs to be nuked anyway; if we
	// remove all the file entries from that dir, the space for the
	// chunks of the file will get reclaimed: chunkservers will tell us
	// about chunks we don't know and those will nuked due to staleness
	emptyDumpsterDir();

	// 9. 注册系统需要的计数器
	RegisterCounters();

	// 10. 初始化各个操作类型对应的操作函数
	initialize_request_handlers();

	// 11. 启动线程, 处理submit_request提交的请求.
	request_processor.start(request_consumer, NULL);

	// 12. 初始化日志系统(用于系统备份和恢复)
	logger_init();

	// 13. 初始化checkpointer
	checkpointer_init();
}
