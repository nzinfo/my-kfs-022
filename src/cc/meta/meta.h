/*!
 * $Id: meta.h 92 2008-07-21 21:20:48Z sriramsrao $
 *
 * \file meta.h
 * \brief Base class and derived classes for KFS metadata objects.
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
#if !defined(KFS_META_H)
#define KFS_META_H

#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include "common/config.h"
#include "base.h"

extern "C"
{
#include <sys/types.h>
#include <sys/time.h>
}

using std::string;
using std::ofstream;

namespace KFS
{

/*!
 * \brief fixed-length unique id generator
 *
 * Unique fixed-length id generator for each file and directory and chunk
 * in the system.
 */

class UniqueID
{
	seqid_t n; //!< id of this object: 本UniqueID的ID
	seqid_t seed; //!< seed for generator
public:
	/*!
	 * \brief generate a new id
	 */
	// 生成ID: seed + 1
	fid_t genid()
	{
		return ++seed;
	}

	// 获取当前种子, 不改变seed的值
	fid_t getseed()
	{
		return seed;
	}

	// 设置seed的值
	void setseed(seqid_t s)
	{
		seed = s;
	}
	UniqueID(seqid_t id, seqid_t s) :
		n(id), seed(s)
	{
	}
	UniqueID() :
		n(0), seed(0)
	{
	}
	seqid_t id() const
	{
		return n;
	} //!< return id
};

/*!
 * \brief base class for data objects (leaf nodes)
 */
class Meta: public MetaNode
{
	fid_t fid; //!< id of this item's owner
public:
	Meta(MetaType t, fid_t id) :
		MetaNode(t), fid(id)
	{
	}
	virtual ~Meta()
	{
	}
	fid_t id() const
	{
		return fid;
	} //!< return the owner id
	bool skip() const
	{
		return testflag(META_SKIP);
	}
	void markskip()
	{
		setflag(META_SKIP);
	}
	void clearskip()
	{
		clearflag(META_SKIP);
	}
	int checkpoint(ofstream &file) const
	{
		file << show() << '\n';
		return file.fail() ? -EIO : 0;
	}
	//!< Compare for equality
	virtual bool match(Meta *test)
	{
		return id() == test->id();
	}
};

/*!
 * \brief Directory entry, mapping a file name to a file id
 */
class MetaDentry: public Meta
{
	fid_t dir; //!< id of parent directory
	string name; //!< name of this entry
public:
	MetaDentry(fid_t parent, string fname, fid_t myID) :
		Meta(KFS_DENTRY, myID), dir(parent), name(fname)
	{
	}

	MetaDentry(const MetaDentry *other) :
		Meta(KFS_DENTRY, other->id()), dir(other->dir), name(other->name)
	{
	}

	const Key key() const
	{
		return Key(KFS_DENTRY, dir);
	}
	const string show() const;
	//!< accessor that returns the name of this Dentry
	const string getName() const
	{
		return name;
	}
	fid_t getDir() const
	{
		return dir;
	}

	// 比较指定的字符串和文件名是否(完全)相同
	const int compareName(const string test) const
	{
		return name.compare(test);
	}
	bool match(Meta *test);
	int checkpoint(ofstream &file) const;
};

/*!
 * \brief Function object to search for file name in directory
 */
/// 匹配MetaDentry: 如果两个MetaDentry中的name相同, 则说明两者相同
class DirMatch
{
	const string searchname;
public:
	DirMatch(const string s) :
		searchname(s)
	{
	}
	bool operator ()(const MetaDentry *d)
	{
		return (d->compareName(searchname) == 0);
	}
};

/*!
 * \brief File or directory attributes.
 *
 * This structure plays the role of an inode in KFS.  Currently just
 * an "artist's conception"; more attritbutes will be added as needed.
 */
/// 文件或文件夹属性: 类型, 拷贝数, 修改时间, 属性修改时间, 创建时间, chunk数目, 文件大小.
class MetaFattr: public Meta
{
public:
	// FileType: dir, file.
	FileType type; //!< file or directory
	int16_t numReplicas; //!< Desired number of replicas for a file
	struct timeval mtime; //!< modification time
	struct timeval ctime; //!< attribute change time
	struct timeval crtime; //!< creation time
	long long chunkcount; //!< number of constituent chunks
	//!< size of file: is only a hint; if we don't the size, the client will
	//!< compute the size whenever needed.  this is not saved to disk
	off_t filesize;

	MetaFattr(FileType t, fid_t id, int16_t n) :
		Meta(KFS_FATTR, id), type(t), numReplicas(n), chunkcount(0), filesize(
				-1)
	{
		int UNUSED_ATTR s = gettimeofday(&crtime, NULL);
		assert(s == 0);
		mtime = ctime = crtime;
		if (type == KFS_DIR)
			filesize = 0;
	}

	MetaFattr(FileType t, fid_t id, struct timeval mt, struct timeval ct,
			struct timeval crt, long long c, int16_t n) :
		Meta(KFS_FATTR, id), type(t), numReplicas(n), mtime(mt), ctime(ct),
				crtime(crt), chunkcount(c), filesize(-1)
	{
		if (type == KFS_DIR)
			filesize = 0;
	}

	MetaFattr() :
		Meta(KFS_FATTR, 0), type(KFS_NONE)
	{
	}

	const Key key() const
	{
		return Key(KFS_FATTR, id());
	}
	const string show() const;
	int checkpoint(ofstream &file) const;
	void setReplication(int16_t val)
	{
		numReplicas = val;
	}
};

/*!
 * \brief downcast from base to derived metadata types
 */
template<typename T> T *
refine(Meta *m)
{
	return static_cast<T *> (m);
}

/*!
 * \brief chunk information for a given file offset
 */
class MetaChunkInfo: public Meta
{
public:
	chunkOff_t offset; // !< offset of chunk within file, 相对于文件首的偏移位置
	chunkId_t chunkId; //!< unique chunk identifier
	seq_t chunkVersion; //!< version # for this chunk

	MetaChunkInfo(fid_t file, chunkOff_t off, chunkId_t id, seq_t v) :
		Meta(KFS_CHUNKINFO, file), offset(off), chunkId(id), chunkVersion(v)
	{
	}

	const Key key() const
	{
		return Key(KFS_CHUNKINFO, id(), offset);
	}

	void DeleteChunk();
	//!< size to which this chunk should be truncated to
	void TruncateChunk(off_t s);

	const string show() const;
	int checkpoint(ofstream &file) const;
};

extern UniqueID fileID; //!< Instance for generating unique fid
extern UniqueID chunkID; //!< Instance for generating unique chunkId

//!< This number is the value used for incrementing chunk version
//numbers.  This number is incremented whenever metaserver restarts
//after a crash as well as whenever an allocation fails because of
//a replica failure.
extern seq_t chunkVersionInc;

}
#endif	// !defined(KFS_META_H)
