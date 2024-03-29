/*!
 * $Id: meta.cc 79 2008-07-12 15:13:20Z sriramsrao $
 *
 * \file meta.cc
 * \brief Operations on the various metadata types.
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

#include <iostream>
#include <fstream>
#include <sstream>
#include "meta.h"
#include "kfstree.h"
#include "kfstypes.h"
#include "util.h"
#include "LayoutManager.h"

using namespace KFS;

/*
 * Seed the unique id generators for files/chunks to start at 2
 */
// Unique(id, seed);
UniqueID KFS::fileID(0, ROOTFID);
UniqueID KFS::chunkID(1, ROOTFID);

/*
 * Initialize the chunk version increment to start at 0.  It'll get bumped
 * when the system starts up.
 */
seq_t KFS::chunkVersionInc = 0;

/*!
 * \brief compare key against test value
 * \param[in] test	the test value
 * \return	negative, zero, or positive, according as the
 * 		this key value is <, =, or > than the test value.
 */
/// 比较两个Key的先后
int Key::compare(const Key &test) const
{
	int kdiff = kind - test.kind;
	int d = 0;

	// 如果种类不同，则返回种类差别
	if (kdiff != 0)
		d = kdiff;
	// 如果种类相同，但是kdata1不同，按规则输出+1或-1
	else if (kdata1 != test.kdata1)
		d = (kdata1 < test.kdata1) ? -1 : 1;
	// 如果种类相同，kdata1也相同，则比较kdata2，按规则输出
	else if (kdata2 != test.kdata2 && kdata2 != MATCH_ANY && test.kdata2
			!= MATCH_ANY)
		d = (kdata2 < test.kdata2) ? -1 : 1;

	return d;
}

const string MetaDentry::show() const
{
	return "dentry/name/" + name + "/id/" + toString(id()) + "/parent/"
			+ toString(dir);
}

bool MetaDentry::match(Meta *m)
{
	MetaDentry *d = refine<MetaDentry> (m);
	return (d != NULL && d->compareName(name) == 0);
}

static string showtime(struct timeval t)
{
	std::ostringstream n(std::ostringstream::out);
	n << t.tv_sec << "/" << t.tv_usec;
	return n.str();
}

const string MetaFattr::show() const
{
	static string fname[] =
	{ "empty", "file", "dir" };

	return "fattr/" + fname[type] + "/id/" + toString(id()) + "/chunkcount/"
			+ toString(chunkcount) + "/numReplicas/" + toString(numReplicas)
			+ "/mtime/" + showtime(mtime) + "/ctime/" + showtime(ctime)
			+ "/crtime/" + showtime(crtime);
}

/// 从gLayoutManager中删除该chunk
void MetaChunkInfo::DeleteChunk()
{
	// if the call to metatree.del() succeeds, the "this" pointer will be
	// deleted.  so, save the chunk id before doing the deletion.
	chunkId_t cid = chunkId;

	// Update the metatree to reflect chunk deletion.  Since we got
	// this MetaChunkInfo by retrieving the allocation information,
	// deletion from the metatree should not fail.
	if (metatree.del(this))
	{
		panic("deleteChunk", false);
	}
	gLayoutManager.DeleteChunk(cid);
}

/// 调用LayoutManager中的TruncateChunk()执行操作
void MetaChunkInfo::TruncateChunk(off_t s)
{
	gLayoutManager.TruncateChunk(chunkId, s);
}

const string MetaChunkInfo::show() const
{
	return "chunkinfo/fid/" + toString(id()) + "/chunkid/" + toString(chunkId)
			+ "/offset/" + toString(offset) + "/chunkVersion/" + toString(
			chunkVersion);
}
