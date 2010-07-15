/*!
 * $Id: replay.cc 71 2008-07-07 15:49:14Z sriramsrao $
 *
 * \file replay.cc
 * \brief log replay
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

#include <cassert>
#include <cstdlib>
#include <iostream>

#include "logger.h"
#include "replay.h"
#include "restore.h"
#include "util.h"
#include "entry.h"
#include "kfstree.h"
#include "LayoutManager.h"

using namespace KFS;

Replay KFS::replayer;

/*!
 * \brief open saved log file for replay
 * \param[in] p	a path in the form "<logdir>/log.<number>"
 */
/// 打开p指定的日志文件: 从p中提取出日志序列号和指定path, 然后打开该日志文件
void Replay::openlog(const string &p)
{
	// 如果当前已经打开了一个日志文件, 则关闭该文件, 用于读取日志
	if (file.is_open())
		file.close();

	if (!file_exists(p))
	{
		std::cerr << "log file " << p << " not found!\n";
		number = 0;
		path = oplog.logfile(0);
	}
	else
	{
		KFS_LOG_VA_INFO("Doing log replay from file: %s", p.c_str());
		string::size_type dot = p.rfind('.');
		assert(dot != string::npos);

		// 提取出日志的序列号
		number = std::atoi(p.substr(dot + 1).c_str());
		assert(number >= 0);
		path = p;
		file.open(path.c_str());
		assert(!file.fail());
	}
}

/*!
 * \brief check log version
 * format: version/<number>
 */
static bool replay_version(deque<string> &c)
{
	fid_t vers;
	bool ok = pop_fid(vers, "version", c, true);
	return (ok && vers == Logger::VERSION);
}

/*!
 * \brief handle common prefix for all log records
 */
static bool pop_parent(fid_t &id, deque<string> &c)
{
	c.pop_front(); // get rid of record type
	return pop_fid(id, "dir", c, true);
}

/*!
 * \brief update the seed of a UniqueID with what is passed in.
 * Since this function is called in the context of log replay, it
 * better be the case that the seed passed in is higher than
 * the id's seed (which was set from a checkpoint file).
 */
/// 将一个UniqueID的种子值设置成指定的数值: 这个数值应该比原来的种子数值要大. 否则, 不执行操作
static void updateSeed(UniqueID &id, seqid_t seed)
{
	if (seed < id.getseed())
	{
		KFS_LOG_VA_ERROR("Seed from log: %lld < id's seed: %lld",
				seed, id.getseed());
		panic("Seed", false);
	}
	id.setseed(seed);
}

/*!
 * \brief replay a file create
 * format: create/dir/<parentID>/name/<name>/id/<myID>
 */
/// 创建文件
/// @note 如果该文件的ID大于fileID的种子, 则需要更新该种子.
static bool replay_create(deque<string> &c)
{
	fid_t parent, me;
	string myname;
	int status = 0;
	int16_t numReplicas;

	bool ok = pop_parent(parent, c);
	ok = pop_name(myname, "name", c, ok);
	ok = pop_fid(me, "id", c, ok);
	ok = pop_short(numReplicas, "numReplicas", c, ok);
	if (ok)
	{
		// for all creates that were successful during normal operation,
		// when we replay it should work; so, exclusive = false
		status = metatree.create(parent, myname, &me, numReplicas, false);
		if (status == 0)
			updateSeed(fileID, me);
	}
	KFS_LOG_VA_DEBUG("Replay create: name=%s, id=%lld", myname.c_str(), me);
	return (ok && status == 0);
}

/*!
 * \brief replay mkdir
 * format: mkdir/dir/<parentID>/name/<name>/id/<myID>
 */
/// 重做创建目录的操作
/// @note 如果该目录的ID大于fileID的种子, 则需要更新该种子.
static bool replay_mkdir(deque<string> &c)
{
	fid_t parent, me;
	string myname;
	int status = 0;
	bool ok = pop_parent(parent, c);
	ok = pop_name(myname, "name", c, ok);
	ok = pop_fid(me, "id", c, ok);
	if (ok)
	{
		status = metatree.mkdir(parent, myname, &me);
		if (status == 0)
			updateSeed(fileID, me);
	}
	KFS_LOG_VA_DEBUG("Replay mkdir: name=%s, id=%lld", myname.c_str(), me);
	return (ok && status == 0);
}

/*!
 * \brief replay remove
 * format: remove/dir/<parentID>/name/<name>
 */
static bool replay_remove(deque<string> &c)
{
	fid_t parent;
	string myname;
	int status = 0;
	bool ok = pop_parent(parent, c);
	ok = pop_name(myname, "name", c, ok);

	if (ok)
		status = metatree.remove(parent, myname);

	return (ok && status == 0);
}

/*!
 * \brief replay rmdir
 * format: rmdir/dir/<parentID>/name/<name>
 */
/// 重做删除目录操作: 直接调用metatree.rmdir()
static bool replay_rmdir(deque<string> &c)
{
	fid_t parent;
	string myname;
	int status = 0;
	bool ok = pop_parent(parent, c);
	ok = pop_name(myname, "name", c, ok);
	if (ok)
		status = metatree.rmdir(parent, myname);
	return (ok && status == 0);
}

/*!
 * \brief replay rename
 * format: rename/dir/<parentID>/old/<oldname>/new/<newpath>
 * NOTE: <oldname> is the name of file/dir in parent.  This
 * will never contain any slashes.
 * <newpath> is the full path of file/dir. This may contain slashes.
 * Since it is the last component, everything after new is <newpath>.
 * So, unlike <oldname> which just requires taking one element out,
 * we need to take everything after "new" for the <newpath>.
 *
 */
/// 重做重命名操作, 直接调用metatree.rename()即可.
static bool replay_rename(deque<string> &c)
{
	fid_t parent;
	string oldname, newpath;
	int status = 0;
	bool ok = pop_parent(parent, c);
	ok = pop_name(oldname, "old", c, ok);
	ok = pop_path(newpath, "new", c, ok);
	if (ok)
		status = metatree.rename(parent, oldname, newpath, true);
	return (ok && status == 0);
}

/*!
 * \brief replay allocate
 * format: allocate/file/<fileID>/offset/<offset>/chunkId/<chunkID>/
 * chunkVersion/<chunkVersion>
 */
/// 重做分配操作, 需要处理: 1. 如果allocate是在该chunk已经完成后, 又发出的操作请求; 2. 将
/// chunk添加到metatree中; 3. 创建chunk和chunkserver的关联; 4. 更新chunkID产生器的种子.
static bool replay_allocate(deque<string> &c)
{
	fid_t fid;
	chunkId_t cid, logChunkId;
	chunkOff_t offset;
	seq_t chunkVersion, logChunkVersion;
	int status = 0;
	MetaFattr *fa;

	c.pop_front();
	bool ok = pop_fid(fid, "file", c, true);
	ok = pop_fid(offset, "offset", c, ok);
	ok = pop_fid(logChunkId, "chunkId", c, ok);
	ok = pop_fid(logChunkVersion, "chunkVersion", c, ok);

	// during normal operation, if a file that has a valid
	// lease is removed, we move the file to the dumpster and log it.
	// a subsequent allocation on that file will succeed.
	// the remove/allocation is recorded in the logs in that order.
	// during replay, we do the remove first and then we try to
	// replay allocation; for the allocation, we won't find
	// the file attributes.  we move on...when the chunkservers
	// that has the associated chunks for the file contacts us, we won't
	// find the fid and so those chunks will get nuked as stale.
	// 获取文件的属性
	fa = metatree.getFattr(fid);
	if (fa == NULL)
		return ok;

	if (ok)
	{
		cid = logChunkId;
		status = metatree.allocateChunkId(fid, offset, &cid, &chunkVersion,
				NULL);
		if (status == -EEXIST)
		{
			// allocates are particularly nasty: we can have
			// allocate requests that retrieve the info for an
			// existing chunk; since there is no tree mutation,
			// there is no way to turn off logging for the request
			// (the mutation field of a request is const).  so, if
			// we end up in a situation where what we get from the
			// log matches what is in the tree, ignore it and move
			// on
			// 当从日志文件中获取的日志记录和tree中已经有的一样的话, 直接退出, 不做任何操作
			if (chunkVersion == logChunkVersion)
				return ok;
			// 如果这两个版本号不同, 则继续执行操作
			status = 0;
		}

		if (status == 0)
		{
			assert(cid == logChunkId);
			chunkVersion = logChunkVersion;
			// 将一个chunk关联到一个文件和他的offset中
			status = metatree.assignChunkId(fid, offset, cid, chunkVersion);
			if (status == 0)
			{

				// 添加该chunk到chunkserver的映射关系
				gLayoutManager.AddChunkToServerMapping(cid, fid, NULL);
				if (cid > chunkID.getseed())
				{
					// chunkID are handled by a two-stage
					// allocation: the seed is updated in
					// the first part of the allocation and
					// the chunk is attached to the file
					// after the chunkservers have ack'ed
					// the allocation.  We can have a run
					// where: (1) the seed is updated, (2)
					// a checkpoint is taken, (3) allocation
					// is done and written to log file.  If
					// we crash, then the cid in log < seed in ckpt.
					// 更新chunkID产生器的种子
					updateSeed(chunkID, cid);
				}
			}
		}
	}
	return (ok && status == 0);
}

/// 从做截取文件: 需要提取出fid和offset
/// format: truncate/file/<fileID>/offset/<offset>
static bool replay_truncate(deque<string> &c)
{
	fid_t fid;
	chunkOff_t offset;
	int status = 0;

	c.pop_front();
	bool ok = pop_fid(fid, "file", c, true);
	ok = pop_fid(offset, "offset", c, ok);
	if (ok)
	{
		chunkOff_t allocOffset;

		// an allocation should not occur during replay
		status = metatree.truncate(fid, offset, &allocOffset);
	}
	return (ok && status == 0);
}

/*!
 * \brief restore time
 * format: time/<time>
 */
// 恢复系统时间
static bool restore_time(deque<string> &c)
{
	c.pop_front();
	std::cout << "Log time: " << c.front() << std::endl;
	return true;
}

/// 创建操作名称和操作的对应关系
static void init_map(DiskEntry &e)
{
	e.add_parser("version", replay_version);
	e.add_parser("create", replay_create);
	e.add_parser("mkdir", replay_mkdir);
	e.add_parser("remove", replay_remove);
	e.add_parser("rmdir", replay_rmdir);
	e.add_parser("rename", replay_rename);
	e.add_parser("allocate", replay_allocate);
	e.add_parser("truncate", replay_truncate);
	e.add_parser("chunkVersionInc", restore_chunkVersionInc);
	e.add_parser("time", restore_time);
}

/*!
 * \brief replay contents of log file
 * \return	zero if replay successful, negative otherwise
 */
/// 用file指定的日志文件进行系统恢复: 除了重做日志文件中的每一条记录外, 还会更新Logger中的
/// 最近日志操作的序列号
int Replay::playlog()
{
	// 如果没有打开日志文件, 则将number置0
	if (!file.is_open())
	{
		//!< no log...so, reset the # to 0.
		number = 0;
		return 0;
	}

	// 规定一行的最长长度为400个字符
	const int MAXLINE = 400;
	char line[MAXLINE];
	int lineno = 0;

	DiskEntry entrymap;
	// 初始化各个操作名称对应的函数
	init_map(entrymap);

	bool is_ok = true;

	// 获取最后一个checkpoint中所包含的序号
	seq_t opcount = oplog.checkpointed();
	while (is_ok && !file.eof())
	{
		++lineno;
		file.getline(line, MAXLINE);

		// 查看该记录中有没有指定的操作类型
		is_ok = entrymap.parse(line);

		// 如果不属于任何一种操作, 则该行记录作废
		if (!is_ok)
			std::cerr << "Error at line " << lineno << ": " << line << '\n';
	}

	// 获取最终的序号
	opcount += lineno;
	// 设置最大序号
	oplog.set_seqno(opcount);

	file.close();
	return is_ok ? 0 : -EIO;
}

/*!
 * \brief replay contents of all log files since CP
 * \return	zero if replay successful, negative otherwise
 */
/// 重做从上一个checkpoint开始的所有日志文件
int Replay::playAllLogs()
{
	int status = 0;

	if (logno() < 0)
	{
		//!< no log...so, reset the # to 0.
		number = 0;
		return 0;
	}

	// 从number开始, 逐个重做每一个日志文件
	for (int i = logno();; i++)
	{
		string logfn = oplog.logfile(i);

		if (!file_exists(logfn))
			break;
		// 1. 打开日志;
		openlog(logfn);
		// 2. 执行恢复.
		status = playlog();
		if (status < 0)
			break;
	}
	return status;

}
