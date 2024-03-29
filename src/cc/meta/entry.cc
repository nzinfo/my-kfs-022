/*
 * $Id: entry.cc 71 2008-07-07 15:49:14Z sriramsrao $
 *
 * \file entry.cc
 * \brief parse checkpoint and log entries
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

#include "entry.h"
#include "util.h"

using namespace KFS;

/// 查找l中有没有字符串是table中的一个元素, 即是恢复日志操作的一种操作
bool DiskEntry::parse(char *line)
{
	const string l(line);
	deque<string> component;

	if (l.empty())
		return true;

	// 将l分成一个一个的小字符串
	split(component, l, SEPARATOR);
	parsetab::iterator c = table.find(component[0]);
	return (c != table.end() && c->second(component));
}

/*!
 * \brief remove a file name from the front of the deque
 * \param[out]	name	the returned name
 * \param[in]	tag	the keyword that precedes the name
 * \param[in]	c	the deque of components from the entry
 * \param[in]	ok	if false, do nothing and return false
 * \return		true if parse was successful
 *
 * The ok parameter short-circuits parsing if an error occurs.
 * This lets us do a series of steps without checking until the
 * end.
 */
/// 将c中的第一个元素弹出(如果tag == c.front()), 并且将c中的下一个元素存放到name中, 然后弹出
/// @note 此时的tag可能为: "name", "old".
bool KFS::pop_name(string &name, const string tag, deque<string> &c, bool ok)
{
	if (!ok || c.size() < 2 || c.front() != tag)
		return false;

	// c.front()为tag, 弹出c.front(),
	c.pop_front();
	// 在将c中的下一个字符串存放到name中
	name = c.front();
	// 弹出name
	c.pop_front();
	if (!name.empty())
		return true;

	/*
	 * Special hack: the initial entry for "/" shows up
	 * as two empty components ("///"); I should probably
	 * come up with a more elegant way to do this.
	 */
	// 如果c为空, 则说明
	if (c.empty() || !c.front().empty())
		return false;

	// name为空, c不空并且c.front()为空: "///"
	c.pop_front();
	name = "/";
	return true;
}

/*!
 * \brief remove a path name from the front of the deque
 * \param[out]	path	the returned path
 * \param[in]	tag	the keyword that precedes the path
 * \param[in]	c	the deque of components from the entry
 * \param[in]	ok	if false, do nothing and return false
 * \return		true if parse was successful
 *
 * The ok parameter short-circuits parsing if an error occurs.
 * This lets us do a series of steps without checking until the
 * end.
 */
/// 将路径中的第一个元素弹出, 然后把整个路径通过path返回.
/// @note 此时的tag可能为: "new".
bool KFS::pop_path(string &path, const string tag, deque<string> &c, bool ok)
{
	if (!ok || c.size() < 2 || c.front() != tag)
		return false;

	c.pop_front();
	/* Collect everything else in path with components separated by '/' */
	path = "";
	while (1)
	{
		path += c.front();
		c.pop_front();
		if (c.empty())
			break;
		path += "/";
	}
	return true;
}

/*!
 * \brief remove a file ID from the component deque
 */
/// 弹出tag指定的c的第一个元素, 并以fid的形式返回第二个元素的值
/// @note 此时的tag可能为: "version", "dir", "id", "file", "offset", "chunkId",
/// "chunkVersion".
bool KFS::pop_fid(fid_t &fid, const string tag, deque<string> &c, bool ok)
{
	if (!ok || c.size() < 2 || c.front() != tag)
		return false;

	c.pop_front();
	fid = toNumber(c.front());
	c.pop_front();
	return (fid != -1);
}

/*!
 * \brief remove a size_t value from the component deque
 */
bool KFS::pop_size(size_t &sz, const string tag, deque<string> &c, bool ok)
{
	if (!ok || c.size() < 2 || c.front() != tag)
		return false;

	c.pop_front();
	sz = toNumber(c.front());
	c.pop_front();
	return (sz != -1u);
}

/*!
 * \brief remove a short value from the component deque
 */
/// @note 此时的tag可能为: "numReplicas".
bool KFS::pop_short(int16_t &num, const string tag, deque<string> &c, bool ok)
{
	if (!ok || c.size() < 2 || c.front() != tag)
		return false;

	c.pop_front();
	num = (int16_t) toNumber(c.front());
	c.pop_front();
	return (num != (int16_t) -1);
}

/*!
 * \brief remove a off_t value from the component deque
 */
bool KFS::pop_offset(off_t &o, const string tag, deque<string> &c, bool ok)
{
	if (!ok || c.size() < 2 || c.front() != tag)
		return false;

	c.pop_front();
	o = toNumber(c.front());
	c.pop_front();
	return (o != -1);
}

/*!
 * \brief remove a file type from the component deque
 */
bool KFS::pop_type(FileType &t, const string tag, deque<string> &c, bool ok)
{
	if (!ok || c.size() < 2 || c.front() != tag)
		return false;

	c.pop_front();
	string type = c.front();
	c.pop_front();
	if (type == "file")
	{
		t = KFS_FILE;
	}
	else if (type == "dir")
	{
		t = KFS_DIR;
	}
	else
		t = KFS_NONE;

	return (t != KFS_NONE);
}

/*!
 * \brief remove a time value from the component deque
 */
bool KFS::pop_time(struct timeval &tv, const string tag, deque<string> &c,
		bool ok)
{
	if (!ok || c.size() < 3 || c.front() != tag)
		return false;

	c.pop_front();
	tv.tv_sec = toNumber(c.front());
	c.pop_front();
	tv.tv_usec = toNumber(c.front());
	c.pop_front();
	return (tv.tv_sec != -1 && tv.tv_usec != -1);
}
