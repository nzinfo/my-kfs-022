/*!
 * $Id: checkpoint.cc 71 2008-07-07 15:49:14Z sriramsrao $
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
 * \file checkpoint.cc
 * \brief KFS metadata checkpointing
 * \author Sriram Rao and Blake Lewis
 *
 * The metaserver during its normal operation writes out log records.  Every
 * N minutes, the metaserver rolls over the log file.  Periodically, a sequence
 * of log files are compacted to create a checkpoint: a previous checkpoint is
 * loaded and subsequent log files are replayed to update the tree.  At the end
 * of replay, a checkpoint is saved to disk.  To save a checkpoint, we iterate
 * through the leaf nodes of the tree copying the contents of each node to a
 * checkpoint file.
 */

#include <iostream>
#include <ctime>
#include <csignal>
#include "checkpoint.h"
#include "kfstree.h"
#include "request.h"
#include "logger.h"
#include "util.h"

using namespace KFS;

// default values
string KFS::CPDIR("./kfscp"); //!< directory for CP files
string KFS::LASTCP(CPDIR + "/latest"); //!< most recent CP file (link)

Checkpoint KFS::cp(CPDIR);

/// 将metatree中所有的叶子节点输出到file中
int Checkpoint::write_leaves()
{
	// metatree的第一层节点的第一个孩子
	LeafIter li(metatree.firstLeaf(), 0);
	Node *p = li.parent();
	Meta *m = li.current();
	int status = 0;
	while (status == 0 && m != NULL)
	{
		// 如果该节点的skip位置位, 则清除该位; 否则, 将该节点的信息输出到file中
		if (m->skip())
			m->clearskip();
		else
			status = m->checkpoint(file);
		li.next();
		p = li.parent();
		// 如果p为空, 则说明该节点为root节点
		m = (p == NULL) ? NULL : li.current();
	}
	return status;
}

/*
 * At system startup, take a CP if the file that corresponds to the
 * latest CP doesn't exist.
 */
// 初始化, 如果文件已经存在, 则不需要进行任何操作; 否则,
void Checkpoint::initial_CP()
{
	if (file_exists(LASTCP))
		return;
	do_CP();
}

// 进行一次checkpoint操作.
int Checkpoint::do_CP()
{
	// 获取checkpoint序列号
	seq_t highest = oplog.checkpointed();
	// 获取由highest生成的文件名
	cpname = cpfile(highest);
	file.open(cpname.c_str());
	int status = file.fail() ? -EIO : 0;
	if (status == 0)
	{
		file << "checkpoint/" << highest << '\n';
		file << "version/" << VERSION << '\n';
		file << "fid/" << fileID.getseed() << '\n';
		file << "chunkId/" << chunkID.getseed() << '\n';
		file << "chunkVersionInc/" << chunkVersionInc << '\n';
		time_t t = time(NULL);
		// 创建时间
		file << "time/" << ctime(&t);
		// 日志文件名
		file << "log/" << oplog.name() << '\n' << '\n';
		// 保存整个metatree
		status = write_leaves();
		file.close();
		link_latest(cpname, LASTCP);
	}
	++cpcount;
	return status;
}

/// 设置checkpoint的路径: CPDIR, LASTCP和KFS::cp的dir
/// @note 默认的CPDIR是:  "./kfscp"
void KFS::checkpointer_setup_paths(const string &cpdir)
{
	if (cpdir != "")
	{
		CPDIR = cpdir;
		LASTCP = cpdir + "/latest";
		cp.setCPDir(cpdir);
	}
}

// 调用initial_CP()初始化CP
void KFS::checkpointer_init()
{
	// start a CP on restart.
	cp.initial_CP();

}
