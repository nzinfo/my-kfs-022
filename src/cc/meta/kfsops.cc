/*!
 * $Id: kfsops.cc 92 2008-07-21 21:20:48Z sriramsrao $
 *
 * \file kfsops.cc
 * \brief KFS file system operations.
 * \author Blake Lewis and Sriram Rao
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
#include "kfstypes.h"
#include "kfstree.h"
#include "util.h"
#include "LayoutManager.h"

#include <algorithm>
#include <functional>
#include <boost/lexical_cast.hpp>
#include "common/log.h"
#include "common/config.h"

using std::mem_fun;
using std::for_each;
using std::find_if;
using std::lower_bound;

using std::cout;
using std::endl;

using namespace KFS;

const string DUMPSTERDIR("dumpster");

/*!
 * \brief Make a dumpster directory into which we can rename busy
 * files.  When the file is non-busy, it is nuked from the dumpster.
 */
/// 创建回收站目录
void KFS::makeDumpsterDir()
{
	fid_t dummy = 0;
	metatree.mkdir(ROOTFID, DUMPSTERDIR, &dummy);
}

/*!
 * \brief Cleanup the dumpster directory on startup.  Also, if
 * the dumpster doesn't exist, make one.
 */
void KFS::emptyDumpsterDir()
{
	makeDumpsterDir();
	metatree.cleanupDumpster();
}

/*!
 * \brief check file name for legality
 *
 * Legal means nonempty and not containing any slashes.
 *
 * \param[in]	name to check
 * \return	true if name is legal
 */
/// 如果文件名不为空, 且其中不包含'/', 则为合法的文件名
static bool legalname(const string name)
{
	return (!name.empty() && name.find('/', 0) == string::npos);
}

/*!
 * \brief see whether path is absolute
 */
/// 判断该路径是否为绝对路径: 即以'/'开头
static bool absolute(const string path)
{
	return (path[0] == '/');
}

/*!
 * \brief common code for create and mkdir
 * \param[in] dir	fid of parent directory
 * \param[in] fname	name of object to be created
 * \param[in] type	file or directory
 * \param[in] myID	fid of new object
 * \param[in] numReplicas desired degree of replication for file
 *
 * Create a directory entry and file attributes for the new object.
 * But don't create attributes for "." and ".." links, since these
 * share the directory's attributes.
 */
/// 在tree中添加新建的文件或文件夹的属性(MetaFattr): 给定父节点id, 文件或文件夹名称,
/// 文件类型, 文件ID和Replicas数目.
int Tree::link(fid_t dir, const string fname, FileType type, fid_t myID,
		int16_t numReplicas)
{
	assert(legalname(fname));
	MetaDentry *dentry = new MetaDentry(dir, fname, myID);
	insert(dentry);
	if (fname != "." && fname != "..")
	{
		MetaFattr *fattr = new MetaFattr(type, dentry->id(), numReplicas);
		insert(fattr);
	}
	return 0;
}

/*!
 * \brief create a new file
 * \param[in] dir	file id of the parent directory
 * \param[in] fname	file name
 * \param[out] newFid	id of new file.
 * \param[in] numReplicas desired degree of replication for file
 * \param[in] exclusive  model the O_EXCL flag of open()
 *
 * \return		status code (zero on success)
 */
/// 创建一个新文件, 并在tree中创建对应项: 如果exclusive置位, 则不允许覆盖已经存在的文件
/// @note 不能用于创建文件夹
int Tree::create(fid_t dir, const string &fname, fid_t *newFid,
		int16_t numReplicas, bool exclusive)
{
	// 检测文件名是否合法
	if (!legalname(fname))
	{
		KFS_LOG_VA_WARN("Bad file name %s", fname.c_str());
		return -EINVAL;
	}

	// 检测拷贝(replicas number)数是否合法
	if (numReplicas <= 0)
	{
		KFS_LOG_VA_DEBUG("Bad # of replicas (%d) for %s",
				numReplicas, fname.c_str());
		return -EINVAL;
	}

	// 获取该文件的文件属性
	MetaFattr *fa = lookup(dir, fname);
	if (fa != NULL)
	{
		// 如果不是文件, 则错误返回
		if (fa->type != KFS_FILE)
			return -EISDIR;

		// Model O_EXECL behavior in create: if the file exists
		// and exclusive is specified, fail the create.
		// 如果文件已经存在, 并且exclusive置位, 则创建失败
		if (exclusive)
			return -EEXIST;

		// 否则, 删除原有文件, 创建新文件.
		int status = remove(dir, fname);
		if (status == -EBUSY)
		{
			KFS_LOG_VA_INFO("Remove failed as file (%d:%s) is busy", dir, fname.c_str());
			return status;
		}
		assert(status == 0);
	}

	// 为创建文件生成一个ID
	if (*newFid == 0)
		*newFid = fileID.genid();

	// 将新建的文件或文件夹信息加入到tree中
	return link(dir, fname, KFS_FILE, *newFid, numReplicas);
}

/*!
 * \brief common code for remove and rmdir
 * \param[in] dir	fid of parent directory
 * \param[in] fname	name of item to be removed
 * \param[in] fa	attributes for removed item
 * \pamam[in] save_fa	don't delete attributes if true
 *
 * save_fa prevents multiple deletions when removing
 * the "." and ".." links to a directory.
 */
/// 删除指定文件或文件夹: 给出父亲目录, 文件或文件夹名, 已经删除的文件或文件夹属性, 是否保留属性
/// @note 保留属性可以防止对同一个文件进行两次删除
/// @note 文件和文件夹的属性是脱离MetaDentry而存在的.
void Tree::unlink(fid_t dir, const string fname, MetaFattr *fa, bool save_fa)
{
	MetaDentry dentry(dir, fname, fa->id());

	// 删除指定的节点
	int UNUSED_ATTR status = del(&dentry);
	assert(status == 0);
	// 如果不要求保留文件或文件夹属性, 则删除该属性
	// @note 文件和文件夹的属性是脱离MetaDentry而存在的.
	if (!save_fa)
	{
		status = del(fa);
		assert(status == 0);
	}
}

/*!
 * \brief remove a file
 * \param[in] dir	file id of the parent directory
 * \param[in] fname	file name
 * \return		status code (zero on success)
 */
/// 删除指定父亲目录和文件名的KFS文件(不能删除文件夹): 先放入回收站, 然后调用unlink从tree中
/// 删除
int Tree::remove(fid_t dir, const string &fname)
{
	// 获取该文件的属性
	MetaFattr *fa = lookup(dir, fname);
	if (fa == NULL)
		return -ENOENT;

	// 该操作只能删除文件, 不能删除文件夹
	if (fa->type != KFS_FILE)
		return -EISDIR;
	if (fa->chunkcount > 0)
	{
		vector<MetaChunkInfo *> chunkInfo;

		// 获取文件所有chunk的信息, 存放到vector中
		getalloc(fa->id(), chunkInfo);
		assert(fa->chunkcount == (long long)chunkInfo.size());

		// 如果租约可用
		if (gLayoutManager.IsValidLeaseIssued(chunkInfo))
		{
			// 把文件或文件夹放入回收站待删除, 同时需删除相关的一些东西, 如文件或文件夹属性等
			int status = moveToDumpster(dir, fname);
			KFS_LOG_VA_DEBUG("Moving %s to dumpster", fname.c_str());
			return status;
		}
		// fire-away...
		// 删除所有属于该文件的chunk. 注意: 要执行metatree的操作
		for_each(chunkInfo.begin(), chunkInfo.end(), mem_fun(
				&MetaChunkInfo::DeleteChunk));
	}

	// 从tree中删除文件
	unlink(dir, fname, fa, false);
	return 0;
}

/*!
 * \brief create a new directory
 * \param[in] dir	file id of the parent directory
 * \param[in] dname	name of new directory
 * \param[out] newFid	id of new directory
 * \return		status code (zero on success)
 */
/// 创建一个新的目录: 通过newFid返回新建文件夹的ID
int Tree::mkdir(fid_t dir, const string &dname, fid_t *newFid)
{
	// 如果文件夹名称为空并且dir不是ROOTFID
	if (!legalname(dname) && dir != ROOTFID && dname != "/")
		return -EINVAL;

	// 如果文件夹存在
	if (lookup(dir, dname) != NULL)
		return -EEXIST;

	fid_t myID = *newFid;

	// 如果dname是'/'的话, 则dir必定为ROOTFID(不然在第一行就会退出程序了),
	// 否则应该生成一个独一无二的fid
	if (myID == 0)
		myID = (dname == "/") ? dir : fileID.genid();

	// 创建新文件夹的MetaDentry()
	MetaDentry *dentry = new MetaDentry(dir, dname, myID);
	// 创建新文件夹的MetaFattr()
	MetaFattr *fattr = new MetaFattr(KFS_DIR, dentry->id(), 1);
	insert(dentry);
	insert(fattr);

	// 为文件夹创建"."和"..".
	int status = link(myID, ".", KFS_DIR, myID, 1);
	if (status != 0)
		panic("link(.)", false);
	status = link(myID, "..", KFS_DIR, dir, 1);
	if (status != 0)
		panic("link(..)", false);

	// 通过newFid返回新建文件夹的ID
	*newFid = myID;
	return 0;
}

/*!
 * \brief check whether a directory is empty
 * \param[in] dir	file ID of the directory
 */
// 查看一个目录是否为空: 如果为空, 则其中只有"."和".."两个项目
bool Tree::emptydir(fid_t dir)
{
	vector<MetaDentry *> v;

	readdir(dir, v);

	return (v.size() == 2);
}

/*!
 * \brief remove a directory
 * \param[in] dir	file id of the parent directory
 * \param[in] dname	name of directory
 * \return		status code (zero on success)
 */
// 删除一个文件夹(给定其父亲目录和该目录名)
/*
 * 不能删除的条件:
 * 1. 回收站"/dumpster";
 * 2. 目录不存在;
 * 3. "."或"..";
 * 4. 目录非空.
 */
int Tree::rmdir(fid_t dir, const string &dname)
{
	MetaFattr *fa = lookup(dir, dname);

	// 如果其父亲目录是根目录, 并且该目录为回收站, 则不可删除
	if ((dir == ROOTFID) && (dname == DUMPSTERDIR))
	{
		KFS_LOG_VA_INFO(" Preventing removing dumpster (%s)",
				dname.c_str());
		return -EPERM;
	}

	// 指定目录不存在
	if (fa == NULL)
		return -ENOENT;

	// 不是目录
	if (fa->type != KFS_DIR)
		return -ENOTDIR;

	// 是"."或者"..", 也不可删除
	if (dname == "." || dname == "..")
		return -EPERM;

	fid_t myID = fa->id();

	// 如果该目录非空, 则禁止删除, 直接返回
	if (!emptydir(myID))
		return -ENOTEMPTY;

	// 如果符合删除条件: 给定fid为目录, 并且为空目录

	// 删除"."和".."之后删除自身.
	unlink(myID, ".", fa, true);
	unlink(myID, "..", fa, true);
	unlink(dir, dname, fa, false);
	return 0;
}

/*!
 * \brief return attributes for the specified object
 * \param[in] fid	the object's file id
 * \return		pointer to the attributes
 */
/// 获取指定的fid所对应的属性
MetaFattr *Tree::getFattr(fid_t fid)
{
	const Key fkey(KFS_FATTR, fid);
	Node *l = findLeaf(fkey);

	// 将tree中由key指定的元素以MetaFattr格式返回.
	return (l == NULL) ? NULL : l->extractMeta<MetaFattr> (fkey);
}

/// 给定父亲目录和文件夹名, 在tree中查找该文件夹的位置
/// @return 找到的文件对应的MetaDentry的指针
MetaDentry *Tree::getDentry(fid_t dir, const string &fname)
{
	vector<MetaDentry *> v;

	// 把dir下的的所有内容(即从属于dir的文件或文件夹)从tree中提取出来. 如果返回值不为0, 则
	// 说明dir不存在.
	if (readdir(dir, v) != 0)
		return NULL;

	vector<MetaDentry *>::iterator d;

	// 在从dir下的所有子文件中查找指定文件名的文件
	d = find_if(v.begin(), v.end(), DirMatch(fname));
	return (d == v.end()) ? NULL : *d;
}

/*!
 * \brief look up a file name and return its attributes
 * \param[in] dir	file id of the parent directory
 * \param[in] fname	file name that we are looking up
 * \return		file attributes or NULL if not found
 */
/// 获取指定文件的属性: 1. 查找文件的目录项; 2. 从目录项中提取fid, 进而获取文件属性.
MetaFattr *Tree::lookup(fid_t dir, const string &fname)
{
	// 获取指定文件所在目录和文件名, 查找其在tree中的MetaDentry
	MetaDentry *d = getDentry(dir, fname);
	if (d == NULL)
		return NULL;

	// 获取文件的属性
	MetaFattr *fa = getFattr(d->id());
	assert(fa != NULL);
	return fa;
}

/*!
 * \brief repeatedly apply Tree::lookup to an entire path
 * \param[in] rootdir	file id of starting directory
 * \param[in] path	the path to look up
 * \return		attributes of the last component (or NULL)
 */
/// @param[in] rootdir 判断的开始目录
/// 判断path指定的路径的每一层路径都相等, 并返回path目录的属性
MetaFattr *Tree::lookupPath(fid_t rootdir, const string &path)
{
	string component;
	// 判断path是不是绝对路径
	bool isabs = absolute(path);

	// 如果rootdir指定的fid为0, 或者newname指定的路径为绝对路径, 则从ROOTFID开始判断,
	// 否则, 从rootdir指定的路径开始判断
	fid_t dir = (rootdir == 0 || isabs) ? ROOTFID : rootdir;

	// 如果是绝对路径, 就从第1个位置开始提取, 否则从第0个位置开始
	string::size_type cstart = isabs ? 1 : 0;

	// 从开始位置开始找到第一个'/'
	string::size_type slash = path.find('/', cstart);

	// 如果path.size() == cstart, 则说明是根路径
	if (path.size() == cstart)
		return lookup(dir, "/");

	while (slash != string::npos)
	{
		// 逐个提取路径, 查看是否合法
		component.assign(path, cstart, slash - cstart);
		MetaFattr *fa = lookup(dir, component);
		if (fa == NULL)
			return NULL;
		dir = fa->id();
		cstart = slash + 1;
		slash = path.find('/', cstart);
	}

	component.assign(path, cstart, path.size() - cstart);
	return lookup(dir, component);
}

/*!
 * \brief read the contents of a directory
 * \param[in] dir	file id of directory
 * \param[out] v	vector of directory entries
 * \return		status code
 */
/// 在树中查找父亲目录为dir的所有KFS_DENTRY, 将他们的MetaDentry存放在v中
/// @retal -ENOENT 如果指定的目录不存在
int Tree::readdir(fid_t dir, vector<MetaDentry *> &v)
{
	const Key dkey(KFS_DENTRY, dir);

	// 在tree中查找第一个符合(KFS_DENTRY, dir)的元素
	Node *l = findLeaf(dkey);

	// 如果没有找到, 则返回错误信息
	if (l == NULL)
		return -ENOENT;

	// 将所有的符合dkey的节点都添加到v中
	extractAll(l, dkey, v);
	assert(v.size() >= 2);
	return 0;
}

/*!
 * \brief return a file's chunk information (if any)
 * \param[in] file	file id for the file
 * \param[out] v	vector of MetaChunkInfo results
 * \return		status code
 */
/// 获取指定file ID对应的文件所包含的所有chunk, 通过v返回
int Tree::getalloc(fid_t file, vector<MetaChunkInfo *> &v)
{
	const Key ckey(KFS_CHUNKINFO, file, Key::MATCH_ANY);
	Node *l = findLeaf(ckey);

	// 获取所有的chunk信息
	if (l != NULL)
		extractAll(l, ckey, v);
	return 0;
}

/*!
 * \brief return the specific chunk information from a file
 * \param[in] file	file id for the file
 * \param[in] offset	offset in the file
 * \param[out] c	MetaChunkInfo
 * \return		status code
 */
// 获取指定文件, 指定offset所在的chunk的MetaChunkInfo
int Tree::getalloc(fid_t file, chunkOff_t offset, MetaChunkInfo **c)
{
	// Allocation information is stored for offset's in the file that
	// correspond to chunk boundaries.
	// 获取offset所在chunk的起始位置
	chunkOff_t boundary = chunkStartOffset(offset);

	// 创建指定chunk的Key, 用于在tree中查找节点
	const Key ckey(KFS_CHUNKINFO, file, boundary);
	Node *l = findLeaf(ckey);
	if (l == NULL)
		return -ENOENT;
	// 将tree中key对应的叶子节点转换为Meta返回
	*c = l->extractMeta<MetaChunkInfo> (ckey);
	return 0;
}

// 比较两个MetaChunkInfo的ID是否相同
class ChunkIdMatch
{
	chunkId_t myid;
public:
	ChunkIdMatch(seq_t c) :
		myid(c)
	{
	}
	bool operator()(MetaChunkInfo *m)
	{
		return m->chunkId == myid;
	}
};

/*!
 * \brief Retrieve the chunk-version for a file/chunkId
 * \param[in] file	file id for the file
 * \param[in] chunkId	chunkId of interest
 * \param[out] chunkVersion  the version # of chunkId if such
 * a chunkId exists; 0 otherwise
 * \return 	status code
 */
/// 获取在指定文件中指定chunkID的chunk的版本号
int Tree::getChunkVersion(fid_t file, chunkId_t chunkId, seq_t *chunkVersion)
{
	vector<MetaChunkInfo *> v;
	vector<MetaChunkInfo *>::iterator i;
	MetaChunkInfo *m;

	*chunkVersion = 0;
	// 获取该文件的所有chunk信息, 存放到v中
	getalloc(file, v);
	i = find_if(v.begin(), v.end(), ChunkIdMatch(chunkId));
	if (i == v.end())
		return -ENOENT;
	m = *i;
	*chunkVersion = m->chunkVersion;
	return 0;
}

/*!
 * \brief allocate a chunk id for a file.
 * \param[in] file	file id for the file
 * \param[in] offset	offset in the file
 * \param[out] chunkId	chunkId that is (pre) allocated.  Allocation
 * is a two-step process: we grab a chunkId and then try to place the
 * chunk on a chunkserver; only when placement succeeds can the
 * chunkId be assigned to the file.  This function does the part of
 * grabbing the chunkId.
 * \param[out] chunkVersion  The version # assigned to the chunk
 * \return		status code
 */
/// 为指定的chunk分配chunk ID: chunk通过文件和offset唯一标识, 通过chunkId, chunkVersion
/// 和numReplicas返回输出结果.
/// @note 如果该chunk已经存在于tree中, 则直接返回chunk的信息即可, 否则再调用genid()分配ID
int Tree::allocateChunkId(fid_t file, chunkOff_t offset, chunkId_t *chunkId,
		seq_t *chunkVersion, int16_t *numReplicas)
{
	MetaFattr *fa = getFattr(file);
	if (fa == NULL)
		return -ENOENT;

	// 由于整个文件的numReplicas属性值都相等, 所以提取出文件中的numReplicas即可.
	if (numReplicas != NULL)
	{
		assert(fa->numReplicas != 0);
		*numReplicas = fa->numReplicas;
	}

	// Allocation information is stored for offset's in the file that
	// correspond to chunk boundaries.  This simplifies finding
	// allocation information as we need to look for chunk
	// starting locations only.
	assert(offset % CHUNKSIZE == 0);

	// 获取offset所在chunk的首地址
	chunkOff_t boundary = chunkStartOffset(offset);
	const Key ckey(KFS_CHUNKINFO, file, boundary);

	// 获取指定文件中指定起始位置的chunk信息
	Node *l = findLeaf(ckey);

	// check if an id has already been assigned to this offset
	// 如果这个chunk在tree中已经存在分配的空间, 则直接提取出该chunk的信息即可.
	if (l != NULL)
	{
		MetaChunkInfo *c = l->extractMeta<MetaChunkInfo> (ckey);
		*chunkId = c->chunkId;
		*chunkVersion = c->chunkVersion;
		return -EEXIST;
	}

	// during replay chunkId will be non-zero.  In such cases,
	// don't do new allocation.
	// 对于重做操作, chunkID可能不是0, 这时就不再为该chunk分配ID了
	if (*chunkId == 0)
	{
		*chunkId = chunkID.genid();
		*chunkVersion = chunkVersionInc;
	}
	return 0;
}

/*!
 * \brief update the metatree to link an allocated chunk id with
 * its associated file.
 * \param[in] file	file id for the file
 * \param[in] offset	offset in the file
 * \param[in] chunkId	chunkId that is (pre) allocated.  Allocation
 * is a two-step process: we grab a chunkId and then try to place the
 * chunk on a chunkserver; only when placement succeeds can the
 * chunkId be assigned to the file.  This function does the part of
 * assinging the chunkId to the file.
 * \param[in] chunkVersion chunkVersion that is (pre) assigned.
 * \return		status code
 */
/// 当我们完成写操作时, 将写入的文件信息和chunk ID等信息关联起来, 存放到tree中
int Tree::assignChunkId(fid_t file, chunkOff_t offset, chunkId_t chunkId,
		seq_t chunkVersion)
{
	chunkOff_t boundary = chunkStartOffset(offset);
	MetaFattr *fa = getFattr(file);
	if (fa == NULL)
		return -ENOENT;

	// check if an id has already been assigned to this chunk
	const Key ckey(KFS_CHUNKINFO, file, boundary);
	Node *l = findLeaf(ckey);

	// 如果这个chunk在tree中已经存在, 则更新其chunk version(如果不同)
	if (l != NULL)
	{
		MetaChunkInfo *c = l->extractMeta<MetaChunkInfo> (ckey);
		chunkId = c->chunkId;
		if (c->chunkVersion == chunkVersion)
			return -EEXIST;
		c->chunkVersion = chunkVersion;
		return 0;
	}

	// 在tree中没有该chunk的记录, 则创建该chunk的记录, 并插入到tree中
	MetaChunkInfo *m = new MetaChunkInfo(file, offset, chunkId, chunkVersion);
	if (insert(m))
	{
		// insert failed
		delete m;
		panic("assignChunk", false);
	}

	// insert succeeded; so, bump the chunkcount.
	fa->chunkcount++;
	// we will know the size of the file only when the write to this chunk
	// is finished.  so, until then....
	// 因为只有文件完成写操作之后才能更新这个值, 所以这里先设置为-1
	fa->filesize = -1;

	gettimeofday(&fa->mtime, NULL);
	return 0;
}

/// 比较两个MetaChunkInfo: 只比较他们在文件中的offset
static bool ChunkInfo_compare(MetaChunkInfo *first, MetaChunkInfo *second)
{
	return first->offset < second->offset;
}

/// 修改文件大小: 可以增大文件, 也可以减小文件大小
int Tree::truncate(fid_t file, chunkOff_t offset, chunkOff_t *allocOffset)
{
	// 提取文件属性
	MetaFattr *fa = getFattr(file);

	if (fa == NULL)
		return -ENOENT;
	if (fa->type != KFS_FILE)
		return -EISDIR;

	vector<MetaChunkInfo *> chunkInfo;
	vector<MetaChunkInfo *>::iterator m;

	// 获取文件fa相关连的所有chunk的MetaChunkInfo.
	getalloc(fa->id(), chunkInfo);
	assert(fa->chunkcount == (long long)chunkInfo.size());

	fa->filesize = -1;

	// compute the starting offset for what will be the
	// "last" chunk for the file
	// 获取文件尾所在的chunk的起始地址
	chunkOff_t lastChunkStartOffset = chunkStartOffset(offset);

	// 构造出fa最后一个chunk的MetaChunkInfo.
	MetaChunkInfo last(fa->id(), lastChunkStartOffset, 0, 0);

	// 用前面定义的比较函数, 二分查找最后一个chunk在chunkInfo中的位置
	m = lower_bound(chunkInfo.begin(), chunkInfo.end(), &last,
			ChunkInfo_compare);

	//
	// If there is no chunk corresponding to the offset to which
	// the file should be truncated, allocate one at that point.
	// This can happen due to the following cases:
	// 1. The offset to truncate to exceeds the last chunk of
	// the file.
	// 2. There is a hole in the file and the offset to truncate
	// to corresponds to the hole.
	//
	// 两种情况会导致该chunk不存在于chunkInfo中: 1. 越界; 2. 文件中存在空洞
	if ((m == chunkInfo.end()) || ((*m)->offset != lastChunkStartOffset))
	{
		// Allocate a chunk at this offset
		*allocOffset = lastChunkStartOffset;
		return 1;
	}

	// 如果最后一个chunk的起始位置<=offset(除访问出错外, 这个应该成立), 则截取最后一个chunk
	// 到指定大小.
	if ((*m)->offset <= offset)
	{
		// truncate the last chunk so that the file
		// has the desired size.
		// 通过MetaChunkInfo中的函数调用LayoutManager中的TruncateChunk()
		(*m)->TruncateChunk(offset - (*m)->offset);
		++m;
	}

	// 删除last之后的所有chunk
	while (m != chunkInfo.end())
	{
		(*m)->DeleteChunk();
		++m;
		fa->chunkcount--;
	}

	// 更新文件修改时间
	gettimeofday(&fa->mtime, NULL);
	return 0;
}

/*!
 * \brief check whether one directory is a descendant of another
 * \param[in] src	file ID of possible ancestor
 * \param[in] dst	file ID of possible descendant
 *
 * Check dst and each of its ancestors to see whether src is
 * among them; used to avoid making a directory into its own
 * child via rename.
 */
/// 逐层查看src是否是dst的祖先目录
bool Tree::is_descendant(fid_t src, fid_t dst)
{
	// 逐层查看;
	while (src != dst && dst != ROOTFID)
	{
		MetaFattr *dotdot = lookup(dst, "..");
		dst = dotdot->id();
	}

	return (src == dst);
}

/*!
 * \brief rename a file or directory
 * \param[in]	parent	file id of parent directory
 * \param[in]	oldname	the file's current name
 * \param[in]	newname	the new name for the file
 * \param[in]	overwrite when set, overwrite the dest if it exists
 * \return		status code
 */
/// 对文件或文件夹进行重命名操作
/*
 * 以下情况不能完成重命名, 需进行错误处理:
 * 1. 新名称不合法;
 * 2. 文件已存在, 且不允许覆盖;
 * 3. 文件已经存在, 但是同原文件的类型不同;
 * 4. 如果要重命名的是一个目录, 但是目录不空;
 * 5. 如果源文件是新文件的祖先目录.
 */
int Tree::rename(fid_t parent, const string &oldname, string &newname,
		bool overwrite)
{
	int status;

	// 获取该文件在tree中的位置
	MetaDentry *src = getDentry(parent, oldname);
	if (src == NULL)
		return -ENOENT;

	fid_t ddir;
	string dname;
	string::size_type rslash = newname.rfind('/');

	// 如果newname中没有'/', 则该newname就是文件名
	if (rslash == string::npos)
	{
		ddir = parent;
		dname = newname;
	}
	// 否则, newname是: 路径名 + 文件名
	else
	{
		// 查看从0到最后一个'/'这一段路径中包含的所有目录都合法. 如果都合法, 返回新路径的属性
		MetaFattr *ddfattr = lookupPath(parent, newname.substr(0, rslash));
		if (ddfattr == NULL)
			return -ENOENT;
		else if (ddfattr->type != KFS_DIR)
			return -ENOTDIR;
		else
			ddir = ddfattr->id();

		// 文件名称为最后的'/'后的名称
		dname = newname.substr(rslash + 1);
	}

	if (!legalname(dname))
		return -EINVAL;

	// 如果名称和当前名称相同, 则不做任何操作
	if (ddir == parent && dname == oldname)
		return 0;

	MetaFattr *sfattr = lookup(parent, oldname);
	MetaFattr *dfattr = lookup(ddir, dname);
	bool dexists = (dfattr != NULL);
	FileType t = sfattr->type;	// 原文件类型

	// 如果文件已存在, 并且不允许覆盖, 则错误退出
	if ((!overwrite) && dexists)
		return -EEXIST;

	// 如果存在并且允许覆盖, 但是新文件与源文件的类型不同时, 不能进行覆盖
	// 不能有同名的文件和文件夹??
	if (dexists && t != dfattr->type)
		return (t == KFS_DIR) ? -ENOTDIR : -EISDIR;

	// 如果已经存在, 并且允许覆盖, 但是目录非空, 则错误返回
	if (dexists && t == KFS_DIR && !emptydir(dfattr->id()))
		return -ENOTEMPTY;

	// 如果源文件是一个目录, 并且它是新文件路径的祖先目录, 则错误返回: 这样会导致新目录的创建
	if (t == KFS_DIR && is_descendant(sfattr->id(), ddir))
		return -EINVAL;

	// 如果存在并且允许覆盖, 则删除原文件或文件夹
	if (dexists)
	{
		status = (t == KFS_DIR) ? rmdir(ddir, dname) : remove(ddir, dname);
		if (status != 0)
			return status;
	}

	// src: 原目录的MetaDentry.
	fid_t srcFid = src->id();

	// 如果这个fid是一个目录, 则删除其中的".."
	if (t == KFS_DIR)
	{
		// get rid of the linkage of the "old" ..
		unlink(srcFid, "..", sfattr, true);
	}

	// 删除原文件或文件夹的属性节点
	status = del(src);
	assert(status == 0);

	// 将新文件的属性插入到tree中
	MetaDentry *newSrc = new MetaDentry(ddir, dname, srcFid);
	status = insert(newSrc);
	assert(status == 0);

	// 如果原文件是目录, 则新文件也是目录, 为其创建".."
	if (t == KFS_DIR)
	{
		// create a new linkage for ..
		status = link(srcFid, "..", KFS_DIR, ddir, 1);
		assert(status == 0);
	}
	return 0;
}

/*!
 * \brief Change the degree of replication for a file.
 * \param[in] dir	file id of the file
 * \param[in] numReplicas	desired degree of replication
 * \return		status code (-errno on failure)
 */
/// 改变文件的冗余拷贝数: 获取文件中包含的所有chunk, 然后调用LayoutManager中的
/// ChangeChunkReplication()函数分别改变每一个chunk的冗余拷贝数
int Tree::changeFileReplication(fid_t fid, int16_t numReplicas)
{
	MetaFattr *fa = getFattr(fid);
	vector<MetaChunkInfo*> chunkInfo;

	if (fa == NULL)
		return -ENOENT;

	fa->setReplication(numReplicas);

	getalloc(fid, chunkInfo);

	// 调用LayoutManager中的函数
	for (vector<ChunkLayoutInfo>::size_type i = 0; i < chunkInfo.size(); ++i)
	{
		gLayoutManager.ChangeChunkReplication(chunkInfo[i]->chunkId);
	}
	return 0;
}

/*!
 * \brief  A file that has to be removed is currently busy.  So, rename the
 * file to the dumpster and we'll clean it up later.
 * \param[in] dir	file id of the parent directory
 * \param[in] fname	file name
 * \return		status code (zero on success)
 */
/// 将文件或文件夹放入回收站, 等待删除, 即重命名为:"/dumpster/ + 原文件或文件夹完整名"
int Tree::moveToDumpster(fid_t dir, const string &fname)
{
	static uint64_t counter = 1;
	string tempname = "/" + DUMPSTERDIR + "/";

	// 获取回收站(dumpster)目录的属性
	MetaFattr *fa = lookup(ROOTFID, DUMPSTERDIR);

	if (fa == NULL)
	{
		// Someone nuked the dumpster
		// 如果回收站不存在, 则创建回收站(原因可能是无意删除)
		makeDumpsterDir();

		// 获取新创建的回收站目录的属性
		fa = lookup(ROOTFID, DUMPSTERDIR);
		if (fa == NULL)
		{
			assert(!"No dumpster");
			KFS_LOG_VA_INFO("Unable to create dumpster dir to remove %s",
					fname.c_str());
			return -1;
		}
	}

	// can't move something in the dumpster back to dumpster
	// 如果要删除的目录是回收站, 则拒绝操作, 返回不存在
	if (fa->id() == dir)
		return -EEXIST;

	// generate a unique name
	// 获取一个独一无二的文件名, 用filename + count构成, 船舰完成后, count++
	tempname += fname + boost::lexical_cast<string>(counter);

	counter++;

	// 将原文件名命名为其在回收站中的名字
	return rename(dir, fname, tempname, true);
}

/// 删除回收站目录: 只删除在tree中的信息, 不进行文件删除操作
class RemoveDumpsterEntry
{
	fid_t dir;
public:
	RemoveDumpsterEntry(fid_t d) :
		dir(d)
	{
	}
	void operator()(MetaDentry *e)
	{
		metatree.remove(dir, e->getName());
	}
};

/*!
 * \brief Periodically, cleanup the dumpster and reclaim space.  If
 * the lease issued on a file has expired, then the file can be nuked.
 */
/// 清除回收站目录下的文件
void Tree::cleanupDumpster()
{
	MetaFattr *fa = lookup(ROOTFID, DUMPSTERDIR);

	if (fa == NULL)
	{
		// Someone nuked the dumpster
		makeDumpsterDir();
	}

	fid_t dir = fa->id();

	vector<MetaDentry *> v;
	readdir(dir, v);

	// 删除每一个文件在tree中的目录
	for_each(v.begin(), v.end(), RemoveDumpsterEntry(dir));
}
