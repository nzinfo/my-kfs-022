//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: ChunkManager.cc 173 2008-09-25 22:57:04Z sriramsrao $
//
// Created 2006/03/28
// Author: Sriram Rao
//
// Copyright 2008 Quantcast Corp.
// Copyright 2006-2008 Kosmix Corp.
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
//
//----------------------------------------------------------------------------

extern "C"
{
#include <dirent.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/statvfs.h>
}

#include "common/log.h"
#include "common/kfstypes.h"

#include "ChunkServer.h"
#include "MetaServerSM.h"
#include "Utils.h"

#include "libkfsIO/Counter.h"
#include "libkfsIO/Checksum.h"
#include "libkfsIO/Globals.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <string>

#include <boost/lexical_cast.hpp>

using std::ofstream;
using std::ifstream;
using std::istringstream;
using std::ostringstream;
using std::ios_base;
using std::list;
using std::min;
using std::max;
using std::endl;
using std::find_if;
using std::string;
using std::vector;

using namespace KFS;
using namespace KFS::libkfsio;

ChunkManager KFS::gChunkManager;

// Cleanup fds on which no I/O has been done for the past N secs
/// 300秒时间内没有操作的fd将被清除
const int INACTIVE_FDS_CLEANUP_INTERVAL_SECS = 300;

// The # of fd's that we allow to be open before cleanup kicks in.
// This value will be set to : # of files that the process can open / 2
int OPEN_FDS_LOW_WATERMARK = 0;

ChunkManager::ChunkManager()
{
	mTotalSpace = mUsedSpace = 0;
	mNumChunks = 0;
	mChunkManagerTimeoutImpl = new ChunkManagerTimeoutImpl(this);
	// we want a timeout once in 10 secs
	// mChunkManagerTimeoutImpl->SetTimeoutInterval(10 * 1000);
	mIsChunkTableDirty = false;
}

/// 析构时清空其中所有的chunk句柄（直接清除内存）
ChunkManager::~ChunkManager()
{
	ChunkInfoHandle_t *cih;

	for (CMI iter = mChunkTable.begin(); iter != mChunkTable.end(); ++iter)
	{
		cih = iter->second;
		delete cih;
	}

	mChunkTable.clear();
	globals().netManager.UnRegisterTimeoutHandler(mChunkManagerTimeoutImpl);
	delete mChunkManagerTimeoutImpl;
}

/// 初始化时，并不读入chunk在磁盘上的信息
void ChunkManager::Init(const vector<string> &chunkDirs, int64_t totalSpace)
{
	mTotalSpace = totalSpace;
	mChunkDirs = chunkDirs;
}

/// 为指定文件的指定chunk在chunk table（打开的chunk列表）中分配一个空间
int ChunkManager::AllocChunk(kfsFileId_t fileId, kfsChunkId_t chunkId,
		kfsSeq_t chunkVersion, bool isBeingReplicated)
{
	string s;
	int fd;
	ChunkInfoHandle_t *cih;
	CMI tableEntry = mChunkTable.find(chunkId);

	mIsChunkTableDirty = true;

	s = MakeChunkPathname(fileId, chunkId, chunkVersion);

	if (tableEntry != mChunkTable.end())
	{
		// 如果该chunk目录已经存在在内存中，则只需要更新其version就可以了
		cih = tableEntry->second;
		ChangeChunkVers(cih->chunkInfo.fileId, cih->chunkInfo.chunkId,
				chunkVersion);
		return 0;
	}

	KFS_LOG_VA_DEBUG("Creating chunk: %s", s.c_str());

	//
	CleanupInactiveFds();

	if ((fd = creat(s.c_str(), S_IRUSR| S_IWUSR)) < 0)
	{
		perror("Create failed: ");
		return -KFS::ESERVERBUSY;
	}
	close(fd);

	mNumChunks++;

	// 申请一个Chunk句柄，并把它保存在chunk table中
	cih = new ChunkInfoHandle_t();
	cih->chunkInfo.Init(fileId, chunkId, chunkVersion);
	cih->isBeingReplicated = isBeingReplicated;
	mChunkTable[chunkId] = cih;
	return 0;
}

/// 从Linux文件系统中删除指定的chunk，即彻底删除chunk（同时必然删除chunk table中对应的项）
int ChunkManager::DeleteChunk(kfsChunkId_t chunkId)
{
	string s;
	ChunkInfoHandle_t *cih;
	CMI tableEntry = mChunkTable.find(chunkId);

	if (tableEntry == mChunkTable.end())
		return -EBADF;

	mIsChunkTableDirty = true;

	cih = tableEntry->second;

	// 获取该chunk对应的Linux文件
	s = MakeChunkPathname(cih->chunkInfo.fileId, cih->chunkInfo.chunkId,
			cih->chunkInfo.chunkVersion);
	// 删除该文件
	unlink(s.c_str());

	KFS_LOG_VA_DEBUG("Deleting chunk: %s", s.c_str());

	mNumChunks--;
	assert(mNumChunks >= 0);
	if (mNumChunks < 0)
		mNumChunks = 0;

	mUsedSpace -= cih->chunkInfo.chunkSize;
	// 删除chunk table中对应的目录
	mChunkTable.erase(chunkId);
	// 删除该chunk句柄
	delete cih;
	return 0;
}

/// 将chunk的全局信息写入到chunk server中
/// chunk全局信息在在文件的起始位置
int ChunkManager::WriteChunkMetadata(kfsChunkId_t chunkId, KfsOp *cb)
{
	CMI tableEntry = mChunkTable.find(chunkId);
	int res;

	if (tableEntry == mChunkTable.end())
		return -EBADF;

	ChunkInfoHandle_t *cih = tableEntry->second;
	WriteChunkMetaOp *wcm = new WriteChunkMetaOp(chunkId, cb);

	// 根据chunk ID创建一个磁盘连接
	DiskConnection *d = SetupDiskConnection(chunkId, wcm);
	if (d == NULL)
		return -KFS::ESERVERBUSY;

	wcm->diskConnection.reset(d);
	wcm->dataBuf = new IOBuffer();
	cih->chunkInfo.Serialize(wcm->dataBuf);

	// 向文件开始地址写入数据
	res = wcm->diskConnection->Write(0, wcm->dataBuf->BytesConsumable(),
			wcm->dataBuf);
	if (res < 0)
	{
		delete wcm;
	}
	return res >= 0 ? 0 : res;
}

// 从指定chunk对应的Linux文件中读入meta信息
// chunk的meta信息存放在文件的首部，长度为KFS_CHUNK_HEADER_SIZE个字节
int ChunkManager::ReadChunkMetadata(kfsChunkId_t chunkId, KfsOp *cb)
{
	CMI tableEntry = mChunkTable.find(chunkId);
	int res = 0;

	if (tableEntry == mChunkTable.end())
		return -EBADF;

	ChunkInfoHandle_t *cih = tableEntry->second;

	cih->lastIOTime = time(0);

	// 如果校验和已经被读入内存中了，则直接执行
	if (cih->chunkInfo.AreChecksumsLoaded())
		// 执行子类中的HandleEvent函数
		return cb->HandleEvent(EVENT_CMD_DONE, (void *) &res);

	// 如果有刚刚请求，并且还没有完成的读meta信息操作
	if (cih->isMetadataReadOngoing)
	{
		// if we have issued a read request for this chunk's metadata,
		// don't submit another one; otherwise, we will simply drive
		// up memory usage for useless IO's
		cih->readChunkMetaOp->AddWaiter(cb);
		return 0;
	}

	// 创建一个读chunk操作，并用这个操作建立一个DiskConnection
	ReadChunkMetaOp *rcm = new ReadChunkMetaOp(chunkId, cb);
	DiskConnection *d = SetupDiskConnection(chunkId, rcm);
	if (d == NULL)
		return -KFS::ESERVERBUSY;

	// 用rcm建立一个DiskConnection，并注册到rcm中
	rcm->diskConnection.reset(d);

	// 从0开始，读入chunk头大小的数据
	res = rcm->diskConnection->Read(0, KFS_CHUNK_HEADER_SIZE);
	if (res < 0)
	{
		delete rcm;
	}

	// 开始读之后，需要置isMetadataReadOngoing为真，以防止再次请求读meta信息
	cih->isMetadataReadOngoing = true;
	cih->readChunkMetaOp = rcm;

	return res >= 0 ? 0 : res;
}

/// 完成chunk meta信息的读取：置isMetadataReadOngoing为false，readChunkMetaOp为空
void ChunkManager::ReadChunkMetadataDone(kfsChunkId_t chunkId)
{
	CMI tableEntry = mChunkTable.find(chunkId);

	if (tableEntry == mChunkTable.end())
		return;

	ChunkInfoHandle_t *cih = tableEntry->second;

	cih->isMetadataReadOngoing = false;
	cih->readChunkMetaOp = NULL;
}

/// 设置指定chunk的meta信息：修改chunk中所有校验块的校验和
int ChunkManager::SetChunkMetadata(const DiskChunkInfo_t &dci)
{
	ChunkInfoHandle_t *cih;

	if (GetChunkInfoHandle(dci.chunkId, &cih) < 0)
		return -EBADF;

	cih->chunkInfo.SetChecksums(dci.chunkBlockChecksum);

	return 0;
}

/// 将chunk放到回收站里边：改变存储路径即可
void ChunkManager::MarkChunkStale(kfsFileId_t fid, kfsChunkId_t chunkId,
		kfsSeq_t chunkVersion)
{
	string s = MakeChunkPathname(fid, chunkId, chunkVersion);
	string staleChunkPathname = MakeStaleChunkPathname(fid, chunkId,
			chunkVersion);

	// 将chunk文件名改为stale文件名，相当于放入回收站了
	rename(s.c_str(), staleChunkPathname.c_str());
	KFS_LOG_VA_INFO("Moving chunk %ld to staleChunks dir", chunkId);
}

/// 彻底删除一个chunk（从chunk table中清除，但没有清除Linux文件系统中的存储）
int ChunkManager::StaleChunk(kfsChunkId_t chunkId)
{
	ChunkInfoHandle_t *cih;
	CMI tableEntry = mChunkTable.find(chunkId);

	if (tableEntry == mChunkTable.end())
		return -EBADF;

	mIsChunkTableDirty = true;

	cih = tableEntry->second;

	// 把指定块放到回收站中
	MarkChunkStale(cih->chunkInfo.fileId, cih->chunkInfo.chunkId,
			cih->chunkInfo.chunkVersion);

	mNumChunks--;
	assert(mNumChunks >= 0);
	if (mNumChunks < 0)
		mNumChunks = 0;

	mUsedSpace -= cih->chunkInfo.chunkSize;
	mChunkTable.erase(chunkId);
	delete cih;
	return 0;
}

/// 将chunk ID指定的chunk大小改变为chunkSize指定的大小
int ChunkManager::TruncateChunk(kfsChunkId_t chunkId, off_t chunkSize)
{
	string chunkPathname;
	ChunkInfoHandle_t *cih;
	int res;
	uint32_t lastChecksumBlock;
	// 查找指定的chunk ID所在的目录
	CMI tableEntry = mChunkTable.find(chunkId);

	// the truncated size should not exceed chunk size.
	if (chunkSize > (off_t) KFS::CHUNKSIZE)
		return -EINVAL;

	if (tableEntry == mChunkTable.end())
		return -EBADF;

	mIsChunkTableDirty = true;

	cih = tableEntry->second;
	chunkPathname = MakeChunkPathname(cih->chunkInfo.fileId,
			cih->chunkInfo.chunkId, cih->chunkInfo.chunkVersion);

	// 将chunkPathname指定的文件改变大小到chunkSize字节
	res = truncate(chunkPathname.c_str(), chunkSize);
	if (res < 0)
	{
		res = errno;
		return -res;
	}

	// 改变chunk server已经使用的空间的大小
	mUsedSpace -= cih->chunkInfo.chunkSize;
	mUsedSpace += chunkSize;
	cih->chunkInfo.chunkSize = chunkSize;

	lastChecksumBlock = OffsetToChecksumBlockNum(chunkSize);

	// XXX: Could do better; recompute the checksum for this last block
	// 应该重新计算最后一个校验块的校验和！！
	cih->chunkInfo.chunkBlockChecksum[lastChecksumBlock] = 0;

	return 0;
}

/// 更新chunk的版本号为chunkVersion指定的版本号。更改之后，原版本在Linux文件系统中的
/// 存储就消失了
int ChunkManager::ChangeChunkVers(kfsFileId_t fileId, kfsChunkId_t chunkId,
		int64_t chunkVersion)
{
	string chunkPathname;
	ChunkInfoHandle_t *cih;
	CMI tableEntry = mChunkTable.find(chunkId);
	string oldname, newname;

	if (tableEntry == mChunkTable.end())
	{
		return -1;
	}

	cih = tableEntry->second;
	// 获取该chunk原来版本在Linux文件系统中的路径名称
	oldname = MakeChunkPathname(cih->chunkInfo.fileId, cih->chunkInfo.chunkId,
			cih->chunkInfo.chunkVersion);

	mIsChunkTableDirty = true;

	KFS_LOG_VA_INFO("Chunk %s already exists; changing version # to %ld",
			oldname.c_str(), chunkVersion);

	cih->chunkInfo.chunkVersion = chunkVersion;

	// 获取chunk新版本在Linux文件系统当中的路径
	newname = MakeChunkPathname(cih->chunkInfo.fileId, cih->chunkInfo.chunkId,
			cih->chunkInfo.chunkVersion);

	// 将文件重命名为新文件名
	rename(oldname.c_str(), newname.c_str());

	mChunkTable[chunkId] = cih;
	return 0;
}

/// 完成chunk复制：标记mIsChunkTableDirty为true，isBeingReplicated为false
void ChunkManager::ReplicationDone(kfsChunkId_t chunkId)
{
	ChunkInfoHandle_t *cih;
	CMI tableEntry = mChunkTable.find(chunkId);

	if (tableEntry == mChunkTable.end())
	{
		return;
	}

	cih = tableEntry->second;

#ifdef DEBUG
	string chunkPathname = MakeChunkPathname(cih->chunkInfo.fileId, cih->chunkInfo.chunkId, cih->chunkInfo.chunkVersion);
	KFS_LOG_VA_DEBUG("Replication for chunk %s is complete...",
			chunkPathname.c_str());
#endif

	mIsChunkTableDirty = true;
	cih->isBeingReplicated = false;
	mChunkTable[chunkId] = cih;
}

/// 启动chunkManager，即向全局变量的netManager中注册定时处理程序
void ChunkManager::Start()
{
	globals().netManager.RegisterTimeoutHandler(mChunkManagerTimeoutImpl);
}

/// 获取chunk在Linux文件系统中的完整路径
string ChunkManager::MakeChunkPathname(kfsFileId_t fid, kfsChunkId_t chunkId,
		kfsSeq_t chunkVersion)
{
	assert(mChunkDirs.size()> 0);

	ostringstream os;
	/// TODO: 什么样的策略？
	uint32_t chunkSubdir = chunkId % mChunkDirs.size();

	os << mChunkDirs[chunkSubdir] << '/' << fid << '.' << chunkId << '.'
			<< chunkVersion;
	return os.str();
}

/// 获取staled chunk对应的Linux文件的绝对路径
/// staled路径和普通路径的关系： staled = 原路径 + "/lost+found/".
string ChunkManager::MakeStaleChunkPathname(kfsFileId_t fid,
		kfsChunkId_t chunkId, kfsSeq_t chunkVersion)
{
	ostringstream os;
	uint32_t chunkSubdir = chunkId % mChunkDirs.size();

	// 获取完整的回收站路径，即：原文件路径 + "/lost+found/"
	string staleChunkDir = GetStaleChunkPath(mChunkDirs[chunkSubdir]);

	os << staleChunkDir << '/' << fid << '.' << chunkId << '.' << chunkVersion;

	return os.str();
}

/// 给定完整的文件路径名称和相对于文件首的offset，获取chunk信息句柄
/// @note 获得的chunk句柄中数据（chunk data）的大小 = filesz - KFS_CHUNK_HEADER_SIZE
void ChunkManager::MakeChunkInfoFromPathname(const string &pathname,
		off_t filesz, ChunkInfoHandle_t **result)
{
	// 找到路径中最后一个'/'
	string::size_type slash = pathname.rfind('/');
	ChunkInfoHandle_t *cih;

	// 如果没找到，rfind函数会返回npos：路径中至少要含有一个'/'
	/*
	 * 路径中至少有一个'/'才合法，因为在Linux文件系统中存储的绝对路径为：
	 * fullpath = Dir + '/' + chunkID + '.' + chunkVersion
	 */
	if (slash == string::npos)
	{
		*result = NULL;
		return;
	}

	string chunkFn;
	vector<string> component;

	// 将文件名存放到chunkFn（chunk file name）中
	chunkFn.assign(pathname, slash + 1, string::npos);

	// TODO: 把chunkFn以'.'为分割线，分割成一个一个的字符串，存放在component中
	split(component, chunkFn, '.');
	// 分开之后，必然为三部分：filename.chunkID,chunkVersion
	assert(component.size() == 3);

	cih = new ChunkInfoHandle_t();
	cih->chunkInfo.fileId = atoll(component[0].c_str());
	cih->chunkInfo.chunkId = atoll(component[1].c_str());
	cih->chunkInfo.chunkVersion = atoll(component[2].c_str());

	// 面向用户的offset和面向Linux文件的offset不同，关系如下：
	if (filesz >= (off_t) KFS_CHUNK_HEADER_SIZE)
		cih->chunkInfo.chunkSize = filesz - KFS_CHUNK_HEADER_SIZE;
	*result = cih;
	/*
	 KFS_LOG_VA_DEBUG("From %s restored: %d, %d, %d", chunkFn.c_str(),
	 cih->chunkInfo.fileId, cih->chunkInfo.chunkId,
	 cih->chunkInfo.chunkVersion);
	 */
}

/// 打开指定chunk对应的Linux文件
/// openFlags: O_RDONLY, O_WRONLY
int ChunkManager::OpenChunk(kfsChunkId_t chunkId, int openFlags)
{
	string fn;	// file name
	int fd;
	ChunkInfoHandle_t *cih;
	CMI tableEntry = mChunkTable.find(chunkId);

	// 指定chunk ID必须在chunk table中，否则即为非法ID
	if (tableEntry == mChunkTable.end())
	{
		KFS_LOG_VA_DEBUG("No such chunk: %s", fn.c_str());
		return -EBADF;
	}
	cih = tableEntry->second;

	// 获取chunk对应Linux文件的完整路径
	fn = MakeChunkPathname(cih->chunkInfo.fileId, cih->chunkInfo.chunkId,
			cih->chunkInfo.chunkVersion);

	// 如果文件指针无效，则说明该chunk未打开，需要打开该chunk
	if ((!cih->dataFH) || (cih->dataFH->mFd < 0))
	{
		// 按指定方式打开文件（Linux文件系统中文件），返回打开文件的句柄
		fd = open(fn.c_str(), openFlags, S_IRUSR|S_IWUSR);
		if (fd < 0)
		{
			perror("open: ");
			return -EBADF;
		}
		globals().ctrOpenDiskFds.Update(1);

		// the checksums will be loaded async
		cih->Init(fd);
	}
	// 否则，则说明该chunk对应的Linux文件已经打开，取出其FD即可
	else
	{
		fd = cih->dataFH->mFd;
	}

	return 0;
}

/// 关闭chunk
/// @note 关闭指定chunk对应的文件，但并不清除mChunkTable中的定义
void ChunkManager::CloseChunk(kfsChunkId_t chunkId)
{
	ChunkInfoHandle_t *cih;
	CMI tableEntry = mChunkTable.find(chunkId);

	if (tableEntry == mChunkTable.end())
	{
		return;
	}

	cih = tableEntry->second;
	// If there are at most 2 references to the handle---a reference
	// from mChunkTable and a reference from cih->chunkHandle, then
	// we can safely close the fileid.
	// 注：use_count()是shared_ptr的方法
	/// 如果该chunk的句柄只有两个引用（分别是mChunkTable和cih->chunkHandle），则可以
	/// 安全关闭而不产生任何影响
	if (cih->dataFH.use_count() <= 2)
	{
		if ((!cih->dataFH) || (cih->dataFH->mFd < 0))
			return;

		// 清除内存中的校验和并关闭相应文件
		cih->Release();
	}
}

// 通过chunk table中的信息获取chunk大小
int ChunkManager::ChunkSize(kfsChunkId_t chunkId, off_t *chunkSize)
{
	ChunkInfoHandle_t *cih;

	// 获取指定chunk对应的chunk句柄
	if (GetChunkInfoHandle(chunkId, &cih) < 0)
		return -EBADF;

	*chunkSize = cih->chunkInfo.chunkSize;

	return 0;
}

/// 从chunkserver磁盘读入指定chunk，指定字节的数据
/// @note 若不足完整的校验块，需按完整的校验块读入数据，以便于校验
int ChunkManager::ReadChunk(ReadOp *op)
{
	ssize_t res;
	DiskConnection *d;
	ChunkInfoHandle_t *cih;
	off_t offset;
	size_t numBytesIO;

	// 获取该chunk的句柄
	if (GetChunkInfoHandle(op->chunkId, &cih) < 0)
		return -EBADF;

	// 根据该chunk的句柄和操作创建DiskConnection
	d = SetupDiskConnection(op->chunkId, op);
	if (d == NULL)
		return -KFS::ESERVERBUSY;

	// the checksums should be loaded...
	cih->chunkInfo.VerifyChecksumsLoaded();

	// 如果chunk的版本号不匹配，则做出错处理
	if (op->chunkVersion != cih->chunkInfo.chunkVersion)
	{
		KFS_LOG_VA_INFO("Version # mismatch(have=%u vs asked=%ld...failing a read",
				cih->chunkInfo.chunkVersion, op->chunkVersion);
		return -KFS::EBADVERS;
	}
	op->diskConnection.reset(d);

	// schedule a read based on the chunk size
	if (op->offset >= cih->chunkInfo.chunkSize)
	{// 如果读入到offset在文件末尾，则不进行读操作
		op->numBytesIO = 0;
	}
	else if ((off_t) (op->offset + op->numBytes) > cih->chunkInfo.chunkSize)
	{// 如果将要读入的数据范围超过了文件末尾
		op->numBytesIO = cih->chunkInfo.chunkSize - op->offset;
	}
	else
	{// 否则，读入的数据即为op中规定要读入的数据
		op->numBytesIO = op->numBytes;
	}

	if (op->numBytesIO == 0)
		return -EIO;

	// for checksumming to work right, reads should be in terms of
	// checksum-blocks.
	// 文件从校验块开始位置读取，为了校验方便
	offset = OffsetToChecksumBlockStart(op->offset);

	// TODO: 这个不一定吧？万一跨过两个校验块呢？
	numBytesIO = (op->numBytesIO / CHECKSUM_BLOCKSIZE) * CHECKSUM_BLOCKSIZE;
	if (op->numBytesIO % CHECKSUM_BLOCKSIZE)
		numBytesIO += CHECKSUM_BLOCKSIZE;

	// Make sure we don't try to read past EOF; the checksumming will
	// do the necessary zero-padding.
	if ((off_t) (offset + numBytesIO) > cih->chunkInfo.chunkSize)
		numBytesIO = cih->chunkInfo.chunkSize - offset;

	// 要把相对于文件开头的地址转换为相对于数据开头的地址
	if ((res = op->diskConnection->Read(offset + KFS_CHUNK_HEADER_SIZE,
			numBytesIO)) < 0)
		return -EIO;

	// read was successfully scheduled
	return 0;
}

// 写磁盘数据。如果要写入的数据不是从一个校验块的首地址开始的，并且rop（读操作）为空，则
// 不进行操作
// TODO: 补齐数据进行校验这一块儿比较麻烦，不能理解！！
int ChunkManager::WriteChunk(WriteOp *op)
{
	ChunkInfoHandle_t *cih;
	int res;

	// 1. 获取chunk句柄
	if (GetChunkInfoHandle(op->chunkId, &cih) < 0)
		return -EBADF;

	// 2. 判断校验和是否已经读入内存
	// the checksums should be loaded...
	cih->chunkInfo.VerifyChecksumsLoaded();

	// schedule a write based on the chunk size.  Make sure that a
	// write doesn't overflow the size of a chunk.
	// 3. 保证数据不会超出一个chunk的大小
	op->numBytesIO = min((size_t)(KFS::CHUNKSIZE - op->offset), op->numBytes);

	if (op->numBytesIO == 0)
		return -EINVAL;

#if defined(__APPLE__)
	size_t addedBytes = max((long long) 0,
			op->offset + op->numBytesIO - cih->chunkInfo.chunkSize);
#else
	// 需要增加的额外的数据量
	size_t addedBytes = max((size_t) 0, (size_t)(op->offset + op->numBytesIO
			- cih->chunkInfo.chunkSize));
#endif

	// 如果空间不足，进行错误处理
	if ((off_t) (mUsedSpace + addedBytes) >= mTotalSpace)
		return -ENOSPC;

	// 4. 补齐数据，进行校验和

	// 如果op指定的操作是从校验块的起始位置开始的，并且跨过了多个校验块
	if ((OffsetToChecksumBlockStart(op->offset) == op->offset)
			&& ((size_t) op->numBytesIO >= (size_t) CHECKSUM_BLOCKSIZE))
	{
		// 此时写入数据的大小必须为校验块大小的整数倍
		assert(op->numBytesIO % CHECKSUM_BLOCKSIZE == 0);
		if (op->numBytesIO % CHECKSUM_BLOCKSIZE != 0)
		{
			return -EINVAL;
		}
#if 0
		// checksum was computed when we got data from client..so, skip
		// Hopefully, common case: write covers an entire block and
		// so, we just compute checksum and get on with the write.
		op->checksums = ComputeChecksums(op->dataBuf, op->numBytesIO);
#endif
		// 如果这个操作不是因为复制chunk而发起的
		if (!op->isFromReReplication)
		{
			assert(op->checksums[0] == op->wpop->checksum);
		}
		else
		{// 如果是因为复制chunk而作的操作，则需要计算所有的校验和
			op->checksums = ComputeChecksums(op->dataBuf, op->numBytesIO);
		}
	}
	// 将要写入的数据的起始位置不是校验块的起始位置
	else
	{
		assert((size_t) op->numBytesIO < (size_t) CHECKSUM_BLOCKSIZE);

		//
		op->checksums.clear();
		// The checksum block we are after is beyond the current
		// end-of-chunk.  So, treat that as a 0-block and splice in.
		/// 如果要写入的数据已经超过了当前chunk的末尾，则把这些数据放到一个新的缓存中，前边
		/// 从校验块开始到实际有数据的位置用0填充。所有这些操作均是为了方便校验
		if (OffsetToChecksumBlockStart(op->offset) >= cih->chunkInfo.chunkSize)
		{
			// 将缓存中的数据补齐到校验块开始的位置
			IOBuffer *data = new IOBuffer();

			data->ZeroFill(CHECKSUM_BLOCKSIZE);
			data->Splice(op->dataBuf, op->offset % CHECKSUM_BLOCKSIZE,
					op->numBytesIO);
			delete op->dataBuf;
			op->dataBuf = data;
			goto do_checksum;
		}

		// Need to read the data block over which the checksum is
		// computed.
		// 这里有程序的一个出口，只要进入if中，必然从这里退出函数
		if (op->rop == NULL)
		{	// 从chunkserver中读入足够的数据，补齐要写入的数据后退出程序
			// issue a read
			// 建立一个读数据操作，读入第一个校验块
			ReadOp *rop = new ReadOp(op,
					OffsetToChecksumBlockStart(op->offset), CHECKSUM_BLOCKSIZE);
			KFS_LOG_VA_DEBUG("Write triggered a read for offset = %ld",
					op->offset);

			op->rop = rop;

			// 执行读数据操作
			rop->Execute();

			if (rop->status < 0)
			{
				int res = rop->status;

				op->rop = NULL;
				rop->wop = NULL;
				delete rop;
				return res;
			}

			return 0;
		}

		// If the read failed, cleanup and bail
		// TODO: 在本函数执行之前，必然有读数据操作?？什么操作呢?？
		if (op->rop->status < 0)
		{
			op->status = op->rop->status;
			op->rop->wop = NULL;
			delete op->rop;
			op->rop = NULL;
			return op->HandleDone(EVENT_DISK_ERROR, NULL);
		}

		// 其中的数据是完整的校验块？？
		// All is good.  So, get on with checksumming
		// 把other中的数据插入到*this中的offset位置，并且删除从offset开始的numBytes个字节
		// void IOBuffer::Splice(IOBuffer *other, int offset, int numBytes)
		op->rop->dataBuf->Splice(op->dataBuf, op->offset % CHECKSUM_BLOCKSIZE,
				op->numBytesIO);

		delete op->dataBuf;
		op->dataBuf = op->rop->dataBuf;
		op->rop->dataBuf = NULL;

		// If the buffer doesn't have a full CHECKSUM_BLOCKSIZE worth
		// of data, zero-pad the end.  We don't need to zero-pad the
		// front because the underlying filesystem will zero-fill when
		// we read a hole.
		/// TODO: 上边这句话什么意思阿？
		ZeroPad(op->dataBuf);

		// 数据量为一个完整的校验块？？
		do_checksum: assert(op->dataBuf->BytesConsumable() == (int) CHECKSUM_BLOCKSIZE);

		// 计算第一块校验块的校验和
		uint32_t cksum = ComputeBlockChecksum(op->dataBuf, CHECKSUM_BLOCKSIZE);
		op->checksums.push_back(cksum);

		// eat away the stuff at the beginning, so that we write out
		// exactly where we were asked from.
		// 删除buffer开头加入的无用数据
		off_t extra = op->offset - OffsetToChecksumBlockStart(op->offset);
		if (extra > 0)
			op->dataBuf->Consume(extra);
	}

	// 5. 创建相关磁盘连接
	DiskConnection *d = SetupDiskConnection(op->chunkId, op);
	if (d == NULL)
		return -KFS::ESERVERBUSY;

	op->diskConnection.reset(d);

	/*
	 KFS_LOG_VA_DEBUG("Checksum for chunk: %ld, offset = %ld, bytes = %ld, # of cksums = %u",
	 op->chunkId, op->offset, op->numBytesIO, op->checksums.size());
	 */

	// 6. 进行磁盘读写
	res = op->diskConnection->Write(op->offset + KFS_CHUNK_HEADER_SIZE,
			op->numBytesIO, op->dataBuf);
	if (res >= 0)
		UpdateChecksums(cih, op);
	return res;
}

/// 更新checksum和已经使用的空间数
/// TODO: 为什么在这里更新已经使用的空间数呢？
void ChunkManager::UpdateChecksums(ChunkInfoHandle_t *cih, WriteOp *op)
{
	mIsChunkTableDirty = true;

	off_t endOffset = op->offset + op->numBytesIO;

	// the checksums should be loaded...
	cih->chunkInfo.VerifyChecksumsLoaded();

	// 将op中的checksum集合写入chunkInfo中
	for (vector<uint32_t>::size_type i = 0; i < op->checksums.size(); i++)
	{
		off_t offset = op->offset + i * CHECKSUM_BLOCKSIZE;
		uint32_t checksumBlock = OffsetToChecksumBlockNum(offset);

		cih->chunkInfo.chunkBlockChecksum[checksumBlock] = op->checksums[i];
	}

	// 如果数据超过了一个end_of_chunk，则需要更新已经使用的空间
	if (cih->chunkInfo.chunkSize < endOffset)
	{

		mUsedSpace += endOffset - cih->chunkInfo.chunkSize;
		cih->chunkInfo.chunkSize = endOffset;
	}
	assert(0 <= mUsedSpace && mUsedSpace <= mTotalSpace);
}

/// 完成读chunk操作：如果校验和全部匹配，则只需要截去首尾无用数据；否则需通知metaserver
/// 重新复制该块
void ChunkManager::ReadChunkDone(ReadOp *op)
{
	ChunkInfoHandle_t *cih = NULL;

	if ((GetChunkInfoHandle(op->chunkId, &cih) < 0) || (op->chunkVersion
			!= cih->chunkInfo.chunkVersion))
	{
		// 如果获取chunk句柄失败 或者 版本号不匹配
		// 调整读入的数据，删去多余数据
		AdjustDataRead(op);

		// 如果获取chunk句柄成功，则是因为版本号错误
		if (cih)
		{
			KFS_LOG_VA_INFO("Version # mismatch(have=%u vs asked=%ld...",
					cih->chunkInfo.chunkVersion, op->chunkVersion);
		}
		op->status = -KFS::EBADVERS;
		return;
	}

	// 填充数据，补齐校验块
	ZeroPad(op->dataBuf);

	assert(op->dataBuf->BytesConsumable() >= (int) CHECKSUM_BLOCKSIZE);

	// either nothing to verify or it better match

	bool mismatch = false;

	// figure out the block we are starting from and grab all the checksums
	vector<uint32_t>::size_type i, checksumBlock = OffsetToChecksumBlockStart(
			op->offset);
	// 计算buffer中各个校验块的校验和
	vector<uint32_t> checksums = ComputeChecksums(op->dataBuf,
			op->dataBuf->BytesConsumable());

	// the checksums should be loaded...
	cih->chunkInfo.VerifyChecksumsLoaded();

	// 分别比较各个校验块的校验和
	for (i = 0; i < checksums.size() && checksumBlock
			< MAX_CHUNK_CHECKSUM_BLOCKS; checksumBlock++, i++)
	{
		if ((cih->chunkInfo.chunkBlockChecksum[checksumBlock] == 0)
				|| (checksums[i]
						== cih->chunkInfo.chunkBlockChecksum[checksumBlock]))
		{
			continue;
		}
		mismatch = true;
		break;
	}

	// 如果没有不匹配的校验和，直接截去首尾无用数据即可
	if (!mismatch)
	{
		// for checksums to verify, we did reads in multiples of
		// checksum block sizes.  so, get rid of the extra
		AdjustDataRead(op);
		return;
	}

	// die ("checksum mismatch");

	KFS_LOG_VA_ERROR("Checksum mismatch for chunk=%ld, offset=%ld, bytes = %ld: expect: %u, computed: %u ",
			op->chunkId, op->offset, op->numBytesIO,
			cih->chunkInfo.chunkBlockChecksum[checksumBlock],
			checksums[i]);

	op->status = -KFS::EBADCKSUM;

	// Notify the metaserver that the chunk we have is "bad"; the
	// metaserver will re-replicate this chunk.
	// 通知metaserver这个块有问题，以便于metaserver重新复制该块
	NotifyMetaCorruptedChunk(op->chunkId);

	// Take out the chunk from our side
	// 删除该chunk
	StaleChunk(op->chunkId);
}

/// 通知metaserver，chunkID指定的chunk已损坏
void ChunkManager::NotifyMetaCorruptedChunk(kfsChunkId_t chunkId)
{
	ChunkInfoHandle_t *cih;

	if (GetChunkInfoHandle(chunkId, &cih) < 0)
	{
		KFS_LOG_VA_ERROR("Unable to notify metaserver of corrupt chunk: %ld",
				chunkId);
		return;
	}

	KFS_LOG_VA_INFO("Notifying metaserver of corrupt chunk (%ld) in file %ld",
			cih->chunkInfo.fileId, chunkId);

	// This op will get deleted when we get an ack from the metaserver
	CorruptChunkOp *ccop =
			new CorruptChunkOp(0, cih->chunkInfo.fileId, chunkId);
	gMetaServerSM.EnqueueOp(ccop);
}

/// 若buffer结尾部分不是一个完整的校验块，则进行零填充，补齐一个完整的校验块
void ChunkManager::ZeroPad(IOBuffer *buffer)
{
	int bytesFilled = buffer->BytesConsumable();
	if ((bytesFilled % CHECKSUM_BLOCKSIZE) == 0)
		return;

	int numToZero = CHECKSUM_BLOCKSIZE - (bytesFilled % CHECKSUM_BLOCKSIZE);
	if (numToZero > 0)
	{
		// pad with 0's
		buffer->ZeroFill(numToZero);
	}
}

// 调整读入的数据：因为数据的读入是以校验块为基本单位的，所以需要截去首尾多余的数据
void ChunkManager::AdjustDataRead(ReadOp *op)
{
	// 从校验块开始到实际数据地址的差别
	size_t extraRead = op->offset - OffsetToChecksumBlockStart(op->offset);

	// 删除开头补充进去的数据
	if (extraRead > 0)
		op->dataBuf->Consume(extraRead);
	// 删除末尾补充进去的数据
	if (op->dataBuf->BytesConsumable() > op->numBytesIO)
		op->dataBuf->Trim(op->numBytesIO);
}

/// 获取offset所在校验块的校验和
uint32_t ChunkManager::GetChecksum(kfsChunkId_t chunkId, off_t offset)
{
	uint32_t checksumBlock = OffsetToChecksumBlockNum(offset);
	ChunkInfoHandle_t *cih;

	if (GetChunkInfoHandle(chunkId, &cih) < 0)
		return 0;

	// the checksums should be loaded...
	cih->chunkInfo.VerifyChecksumsLoaded();

	assert(checksumBlock <= MAX_CHUNK_CHECKSUM_BLOCKS);

	return cih->chunkInfo.chunkBlockChecksum[checksumBlock];
}

/// 建立磁盘连接，op指定DiskConnection的KfsCallbackObj...
DiskConnection *
ChunkManager::SetupDiskConnection(kfsChunkId_t chunkId, KfsOp *op)
{
	ChunkInfoHandle_t *cih;
	DiskConnection *diskConnection;
	CMI tableEntry = mChunkTable.find(chunkId);

	// 如果指定的chunk ID不在chunk table中，则不为之打开DiskConnection
	// 是不是所有的Chunk ID都会存放在chunk table中? 是的
	if (tableEntry == mChunkTable.end())
	{
		return NULL;
	}

	cih = tableEntry->second;
	// 如果该chunk句柄指定的文件指针或文件描述符无效，则需要打开该chunk
	if ((!cih->dataFH) || (cih->dataFH->mFd < 0))
	{
		CleanupInactiveFds();
		// 以可读可写的方式打开文件
		if (OpenChunk(chunkId, O_RDWR) < 0)
			return NULL;
	}

	cih->lastIOTime = time(0);
	// TODO: op指定KfsCallbackObj?
	diskConnection = new DiskConnection(cih->dataFH, op);

	return diskConnection;
}

void ChunkManager::CancelChunkOp(KfsCallbackObj *cont, kfsChunkId_t chunkId)
{
	// Cancel the chunk operations scheduled by KfsCallbackObj on chunkId.
	// XXX: Fill it...
}

//
// dump out the contents of the chunkTable to disk
//
// 将内存中的chunk table存放到本地磁盘上
void ChunkManager::Checkpoint()
{
	CheckpointOp *cop;
	// on the macs, i can't declare CMI iter;
	CMI iter = mChunkTable.begin();

	mLastCheckpointTime = time(NULL);

	// 如果Chunk Table没有修改过，则可以直接返回
	if (!mIsChunkTableDirty)
		return;

	// KFS_LOG_VA_DEBUG("Checkpointing state");
	cop = new CheckpointOp(1);

#if 0
	// there are no more checkpoints on the chunkserver...this will all go
	// we are using this to rotate logs...

	ChunkInfoHandle_t *cih;

	for (iter = mChunkTable.begin(); iter != mChunkTable.end(); ++iter)
	{
		cih = iter->second;
		// If a chunk is being replicated, then it is not yet a part
		// of the namespace.  When replication is done, it becomes a
		// part of the namespace.  This model keeps recovery simple:
		// if we die in the midst of replicating a chunk, on restart,
		// we will the chunk as an orphan and throw it away.
		if (cih->isBeingReplicated)
		continue;
		cop->data << cih->chunkInfo.fileId << ' ';
		cop->data << cih->chunkInfo.chunkId << ' ';
		cop->data << cih->chunkInfo.chunkSize << ' ';
		cop->data << cih->chunkInfo.chunkVersion << ' ';

		cop->data << MAX_CHUNK_CHECKSUM_BLOCKS << ' ';
		for (uint32_t i = 0; i < MAX_CHUNK_CHECKSUM_BLOCKS; ++i)
		{
			cop->data << cih->chunkInfo.chunkBlockChecksum[i] << ' ';
		}
		cop->data << endl;
	}
#endif

	// 通知进行服务器同步
	gLogger.Submit(cop);

	// Now, everything is clean...
	mIsChunkTableDirty = false;
}

/// 从系统中取出所有的chunk的Linux文件路径，并且存放在一个数组namelist中
/// @note 如果有一个目录读取失败，则退出函数，不返回任何值
int ChunkManager::GetChunkDirsEntries(struct dirent ***namelist)
{
	// 全都是二维指针
	struct dirent **entries;
	vector<struct dirent **> dirEntries;
	vector<int> dirEntriesCount;
	int res, numChunkFiles = 0;
	uint32_t i;

	// *namelist是一个二维指针
	*namelist = NULL;

	// 读出系统中所有的chunk文件，把文件名称（非完整路径）存放在dirEntries中，并进行一系列统计
	for (i = 0; i < mChunkDirs.size(); i++)
	{
		// int  scandir(const char *dir, struct dirent **namelist,
		// 	nt (*select)  (const  struct  dirent *),
		//	nt (*compar)  (const struct dirent **, const struct dirent**));
		// 选出mChunk目录下的所有项(0)，按字母序(laphasort)排序后存放到entries中
		res = scandir(mChunkDirs[i].c_str(), &entries, 0, alphasort);
		if (res < 0)
		{
			// 如果读取失败，则清除所有已经读进来的目录，并退出程序
			KFS_LOG_VA_INFO("Unable to open %s", mChunkDirs[i].c_str());
			for (i = 0; i < dirEntries.size(); i++)
			{
				entries = dirEntries[i];
				for (int j = 0; j < dirEntriesCount[i]; j++)
					free(entries[j]);
				free(entries);
			}
			dirEntries.clear();
			return -1;
		}

		// 添加读入目录中的文件
		dirEntries.push_back(entries);
		// 记录每一个目录中文件的个数
		dirEntriesCount.push_back(res);
		// 统计该chunk中总共拥有的chunk文件数目
		numChunkFiles += res;
	}

	// Get all the directory entries into one giganto array
	*namelist = (struct dirent **) malloc(sizeof(struct dirent **)
			* numChunkFiles);

	numChunkFiles = 0;
	for (i = 0; i < dirEntries.size(); i++)
	{
		int count = dirEntriesCount[i];
		entries = dirEntries[i];

		memcpy((*namelist) + numChunkFiles, entries, count
				* sizeof(struct dirent **));
		numChunkFiles += count;
	}
	return numChunkFiles;
}

/// 获取chunk server中所有的chunk文件的完整（路径）名称，存放到pathname中
/// @note 如果其中有目录读取失败，则跳过失败的目录继续读取下一个目录
void ChunkManager::GetChunkPathEntries(vector<string> &pathnames)
{
	uint32_t i;
	struct dirent **entries = NULL;
	int res;

	for (i = 0; i < mChunkDirs.size(); i++)
	{
		res = scandir(mChunkDirs[i].c_str(), &entries, 0, alphasort);

		// 忽略读取失败的目录
		if (res < 0)
		{
			KFS_LOG_VA_INFO("Unable to open %s", mChunkDirs[i].c_str());
			continue;
		}
		for (int j = 0; j < res; j++)
		{
			string s = mChunkDirs[i] + "/" + entries[j]->d_name;
			pathnames.push_back(s);
			free(entries[j]);
		}
		free(entries);
	}
}

/// TODO: 通过重新启动的方式对系统进行恢复
void ChunkManager::Restart()
{
	int version;

	version = gLogger.GetVersionFromCkpt();
	if (version == gLogger.GetLoggerVersionNum())
	{
		RestoreV2();
	}
	else
	{
		std::cout
				<< "Unsupported version...copy out the data and copy it back in...."
				<< std::endl;
		exit(-1);
	}

	// Write out a new checkpoint file with just version and set it at 2
	gLogger.Checkpoint(NULL);
}

/// 恢复系统: 从文件系统读入所有的chunk文件信息，并解析出chunk句柄放到chunk table中
void ChunkManager::RestoreV2()
{
	// sort all the chunk names alphabetically in each of the
	// directories
	vector<string> chunkPathnames;
	struct stat buf;
	int res;
	uint32_t i, numChunkFiles;

	// 获取系统中所有chunk文件的完整（路径）名称，放到chunkPathname中
	GetChunkPathEntries(chunkPathnames);

	numChunkFiles = chunkPathnames.size();

	// each chunk file is of the form: <fileid>.<chunkid>.<chunkversion>
	// parse the filename to extract out the chunk info
	// 根据完整的文件名，解析出所有的chunk句柄，存放在chunk table中
	for (i = 0; i < numChunkFiles; ++i)
	{
		string s = chunkPathnames[i];
		ChunkInfoHandle_t *cih;
		res = stat(s.c_str(), &buf);
		if ((res < 0) || (!S_ISREG(buf.st_mode)))
			continue;
		MakeChunkInfoFromPathname(s, buf.st_size, &cih);
		// 将相关的chunk句柄添加到chunk table中
		if (cih != NULL)
			AddMapping(cih);
	}
}

#if 0
//
// Restart from a checkpoint. Validate that the files in the
// checkpoint exist in the chunks directory.
//
void
ChunkManager::RestoreV1()
{
	ChunkInfoHandle_t *cih;
	string chunkIdStr, chunkPathname;
	ChunkInfo_t entry;
	int i, res, numChunkFiles;
	bool found;
	struct stat buf;
	struct dirent **namelist;
	CMI iter = mChunkTable.begin();
	vector<kfsChunkId_t> orphans;
	vector<kfsChunkId_t>::size_type j;

	// sort all the chunk names alphabetically in each of the
	// directories
	numChunkFiles = GetChunkDirsEntries(&namelist);
	if (numChunkFiles < 0)
	return;

	gLogger.Restore();

	// Now, validate: for each entry in the chunk table, verify that
	// the backing file exists. also, if there any "zombies" lying
	// around---that is, the file exists, but there is no associated
	// entry in the chunk table, nuke the backing file.

	for (iter = mChunkTable.begin(); iter != mChunkTable.end(); ++iter)
	{
		entry = iter->second->chunkInfo;

		chunkIdStr = boost::lexical_cast<std::string>(entry.chunkId);

		found = false;
		for (i = 0; i < numChunkFiles; ++i)
		{
			if (namelist[i] &&
					(chunkIdStr == namelist[i]->d_name))
			{
				free(namelist[i]);
				namelist[i] = NULL;
				found = true;
				break;
			}
		}
		if (!found)
		{
			KFS_LOG_VA_INFO("Orphaned chunk as the file doesn't exist: %s",
					chunkIdStr.c_str());
			orphans.push_back(entry.chunkId);
			continue;
		}

		chunkPathname = MakeChunkPathname(entry.chunkId);
		res = stat(chunkPathname.c_str(), &buf);
		if (res < 0)
		continue;

		// stat buf's st_size is of type off_t.  Typecast to avoid compiler warnings.
		if (buf.st_size != (off_t) entry.chunkSize)
		{
			KFS_LOG_VA_INFO("Truncating file: %s to size: %zd",
					chunkPathname.c_str(), entry.chunkSize);
			if (truncate(chunkPathname.c_str(), entry.chunkSize) < 0)
			{
				perror("Truncate");
			}
		}
	}

	if (orphans.size()> 0)
	{
		// Take a checkpoint after we are done replay
		mIsChunkTableDirty = true;
	}

	// Get rid of the orphans---valid entries but no backing file
	for (j = 0; j < orphans.size(); ++j)
	{

		KFS_LOG_VA_DEBUG("Found orphan entry: %ld", orphans[j]);

		iter = mChunkTable.find(orphans[j]);
		if (iter != mChunkTable.end())
		{
			cih = iter->second;
			mUsedSpace -= cih->chunkInfo.chunkSize;
			mChunkTable.erase(orphans[j]);
			delete cih;

			mNumChunks--;
			assert(mNumChunks >= 0);
			if (mNumChunks < 0)
			mNumChunks = 0;
		}
	}

	// Get rid of zombies---backing file exists, but no entry in logs/ckpt
	for (i = 0; i < numChunkFiles; ++i)
	{
		if (namelist[i] == NULL)
		// entry was found (above)
		continue;
		if ((strcmp(namelist[i]->d_name, ".") == 0) ||
				(strcmp(namelist[i]->d_name, "..") == 0))
		{
			free(namelist[i]);
			namelist[i] = NULL;
			continue;
		}

		// zombie
		chunkPathname = MakeChunkPathname(namelist[i]->d_name);

		// there could be directories here...such as lost+found etc...
		res = stat(chunkPathname.c_str(), &buf);
		if ((res == 0) && (S_ISREG(buf.st_mode)))
		{
			// let us figure out why we are seeing zombies...
			// KFS_LOG_VA_FATAL("Found zombie entry...");

			unlink(chunkPathname.c_str());
			KFS_LOG_VA_DEBUG("Found zombie entry: %s", chunkPathname.c_str());
		}

		free(namelist[i]);
		namelist[i] = NULL;
	}

	free(namelist);
	if (mIsChunkTableDirty)
	{
		Checkpoint();
	}

#ifdef DEBUG
	assert((mUsedSpace >= 0) && (mUsedSpace <= mTotalSpace));
	// if there are no chunks, used space better be 0
	if (mChunkTable.size() == 0)
	{
		assert(mUsedSpace == 0);
		assert(mNumChunks == 0);
	}
#endif
}
#endif

/// 向chunk table中添加chunk句柄
void ChunkManager::AddMapping(ChunkInfoHandle_t *cih)
{
	mNumChunks++;
	mChunkTable[cih->chunkInfo.chunkId] = cih;
	mUsedSpace += cih->chunkInfo.chunkSize;
}

/// 重新将指定的chunk加入到本地磁盘
/// @note 与AllocChunk的区别在于：这个函数不进行实际的创建文件操作，只修改chunk table中有关项
void ChunkManager::ReplayAllocChunk(kfsFileId_t fileId, kfsChunkId_t chunkId,
		int64_t chunkVersion)
{
	ChunkInfoHandle_t *cih;

	mIsChunkTableDirty = true;

	if (GetChunkInfoHandle(chunkId, &cih) == 0)
	{
		// 如果chunk目录存在于本地chunk table中，则只需要更新其版本号
		cih->chunkInfo.chunkVersion = chunkVersion;
		mChunkTable[chunkId] = cih;
		return;
	}

	// 本地所有的chunk数目加1
	mNumChunks++;
	// after replay is done, when we verify entries in the table, we
	// stat the file and fix up the sizes then.  so, no need to do
	// anything here.
	// 不需要更新大小，这个操作将在之后验证chunk table目录时实现
	cih = new ChunkInfoHandle_t();
	cih->chunkInfo.fileId = fileId;
	cih->chunkInfo.chunkId = chunkId;
	cih->chunkInfo.chunkVersion = chunkVersion;
	mChunkTable[chunkId] = cih;
}

/// 重新修改chunk table中的chunk版本号
/// @note 与ChangeChunkVers的不同在于：此函数并没有进行实质上的重命名操作
void ChunkManager::ReplayChangeChunkVers(kfsFileId_t fileId,
		kfsChunkId_t chunkId, int64_t chunkVersion)
{
	ChunkInfoHandle_t *cih;

	if (GetChunkInfoHandle(chunkId, &cih) != 0)
		return;

	KFS_LOG_VA_DEBUG("Chunk %ld already exists; changing version # to %ld",
			chunkId, chunkVersion);

	// Update the version #
	cih->chunkInfo.chunkVersion = chunkVersion;
	mChunkTable[chunkId] = cih;
	mIsChunkTableDirty = true;
}

/// 重演删除chunk操作，但并不进行实际的删除文件操作，用以保证占用大小统计的正确性
/// 在文件删除后，chunk table中的目录仍然存在时
void ChunkManager::ReplayDeleteChunk(kfsChunkId_t chunkId)
{
	ChunkInfoHandle_t *cih;
	CMI tableEntry = mChunkTable.find(chunkId);

	mIsChunkTableDirty = true;

	/// 若chunk table中仍然存在该目录，则执行操作；否则，直接退出即可
	if (tableEntry != mChunkTable.end())
	{
		cih = tableEntry->second;
		mUsedSpace -= cih->chunkInfo.chunkSize;
		mChunkTable.erase(chunkId);
		delete cih;

		mNumChunks--;
		assert(mNumChunks >= 0);
		if (mNumChunks < 0)
			mNumChunks = 0;
	}
}

/// 重演写完成操作，将指定chunk对应的句柄中的校验和更新为指定校验和（没有写磁盘操作），同时
/// 更新已用磁盘统计数据
/// @note 系统中并没有WriteDone接口
void ChunkManager::ReplayWriteDone(kfsChunkId_t chunkId, off_t chunkSize,
		off_t offset, vector<uint32_t> checksums)
{
	ChunkInfoHandle_t *cih;
	int res;

	res = GetChunkInfoHandle(chunkId, &cih);
	if (res < 0)
		return;

	mIsChunkTableDirty = true;
	mUsedSpace -= cih->chunkInfo.chunkSize;
	cih->chunkInfo.chunkSize = chunkSize;
	mUsedSpace += cih->chunkInfo.chunkSize;

	// 更新校验和
	for (vector<uint32_t>::size_type i = 0; i < checksums.size(); i++)
	{
		off_t currOffset = offset + i * CHECKSUM_BLOCKSIZE;
		size_t checksumBlock = OffsetToChecksumBlockNum(currOffset);

		cih->chunkInfo.chunkBlockChecksum[checksumBlock] = checksums[i];
	}
}

/// 重演更改文件大小的操作
/// @note 本系统中并没有TruncateDone接口
void ChunkManager::ReplayTruncateDone(kfsChunkId_t chunkId, off_t chunkSize)
{
	ChunkInfoHandle_t *cih;
	int res;
	off_t lastChecksumBlock;

	res = GetChunkInfoHandle(chunkId, &cih);
	if (res < 0)
		return;

	// 更新已经使用的磁盘的统计数据
	mIsChunkTableDirty = true;
	mUsedSpace -= cih->chunkInfo.chunkSize;
	cih->chunkInfo.chunkSize = chunkSize;
	mUsedSpace += cih->chunkInfo.chunkSize;

	lastChecksumBlock = OffsetToChecksumBlockNum(chunkSize);

	// 更新最后一个校验块的校验和
	cih->chunkInfo.chunkBlockChecksum[lastChecksumBlock] = 0;
}

/// 获取本地磁盘上的所有chunk句柄
void ChunkManager::GetHostedChunks(vector<ChunkInfo_t> &result)
{
	ChunkInfoHandle_t *cih;

	// walk thru the table and pick up the chunk-ids
	for (CMI iter = mChunkTable.begin(); iter != mChunkTable.end(); ++iter)
	{
		cih = iter->second;
		result.push_back(cih->chunkInfo);
	}
}

/// 查找对指定chunk ID对应的chunk句柄的内存地址（通过map查找）
int ChunkManager::GetChunkInfoHandle(kfsChunkId_t chunkId,
		ChunkInfoHandle_t **cih)
{
	CMI iter = mChunkTable.find(chunkId);

	if (iter == mChunkTable.end())
	{
		*cih = NULL;
		return -EBADF;
	}

	*cih = iter->second;
	return 0;
}

/// 根据指定的WriteIdAllocOp，生成一个WriteOp操作，存放到mPendingWrites中
int ChunkManager::AllocateWriteId(WriteIdAllocOp *wi)
{
	WriteOp *op;
	ChunkInfoHandle_t *cih;

	if (GetChunkInfoHandle(wi->chunkId, &cih) < 0)
		return -EBADF;

	if (wi->chunkVersion != cih->chunkInfo.chunkVersion)
	{
		KFS_LOG_VA_INFO("Version # mismatch(have=%d vs asked=%lu...failing a write",
				cih->chunkInfo.chunkVersion, wi->chunkVersion);
		return -EINVAL;
	}

	mWriteId++;
	op = new WriteOp(wi->seq, wi->chunkId, wi->chunkVersion, wi->offset,
			wi->numBytes, NULL, mWriteId);
	op->enqueueTime = time(NULL);
	wi->writeId = mWriteId;
	op->isWriteIdHolder = true;
	mPendingWrites.push_back(op);
	return 0;
}

/// 将wp中缓冲器中的数据合并到mPendingWrites中WriteId相同的WriteOp中
int ChunkManager::EnqueueWrite(WritePrepareOp *wp)
{
	WriteOp *op;
	ChunkInfoHandle_t *cih;

	if (GetChunkInfoHandle(wp->chunkId, &cih) < 0)
		return -EBADF;

	if (wp->chunkVersion != cih->chunkInfo.chunkVersion)
	{
		KFS_LOG_VA_INFO("Version # mismatch(have=%d vs asked=%lu...failing a write",
				cih->chunkInfo.chunkVersion, wp->chunkVersion);
		return -EINVAL;
	}

	// 从mPendingWrites中获取指定的WriteOp操作
	op = GetWriteOp(wp->writeId);

	// 如果取出的op中没有缓存数据，则将wp中缓存中的数据指定到op中；否则，将wp的缓存数据扩展到
	// op的缓存器中
	if (op->dataBuf == NULL)
		op->dataBuf = wp->dataBuf;
	else
		op->dataBuf->Append(wp->dataBuf);
	wp->dataBuf = NULL;

	// 再将op加入到mPendingWrites中
	mPendingWrites.push_back(op);
	return 0;
}

/// 用于判断两个WriteOp中操作的chunkId是否相同
class ChunkIdMatcher
{
	kfsChunkId_t myid;
public:
	ChunkIdMatcher(kfsChunkId_t s) :
		myid(s)
	{
	}
	bool operator()(const WriteOp *r)
	{
		return (r->chunkId == myid);
	}
};

/// 查看mPendingWrites中是否有对于指定chunk的写操作
bool ChunkManager::IsWritePending(kfsChunkId_t chunkId)
{
	list<WriteOp *>::iterator i;

	i = find_if(mPendingWrites.begin(), mPendingWrites.end(), ChunkIdMatcher(
			chunkId));
	if (i == mPendingWrites.end())
		return false;
	return true;
}

/// 从chunk句柄中获取指定chunk的版本号
int64_t ChunkManager::GetChunkVersion(kfsChunkId_t c)
{
	ChunkInfoHandle_t *cih;

	if (GetChunkInfoHandle(c, &cih) < 0)
		return -1;

	return cih->chunkInfo.chunkVersion;
}

// Helper functor that matches write id's by sequence #'s
/// 用来判断两个WriteOp的writeId是否相同
class WriteIdMatcher
{
	int64_t myid;
public:
	WriteIdMatcher(int64_t s) :
		myid(s)
	{
	}
	bool operator()(const WriteOp *r)
	{
		return (r->writeId == myid);
	}
};

/// 从mPendingWrites中获取指定writeId的WriteOp，并从mPendingWrites中删除这个WriteOp
WriteOp *
ChunkManager::GetWriteOp(int64_t writeId)
{
	list<WriteOp *>::iterator i;
	WriteOp *op;

	i = find_if(mPendingWrites.begin(), mPendingWrites.end(), WriteIdMatcher(
			writeId));
	if (i == mPendingWrites.end())
		return NULL;
	op = *i;

	// 从mPendingWrites中删除已经获取的WriteOp
	mPendingWrites.erase(i);
	return op;
}

/// 从mPendingWrites克隆出一个WriteOp，该函数与GetWriteOp的区别在于不删除mPendingWrites
/// 中的操作
WriteOp *
ChunkManager::CloneWriteOp(int64_t writeId)
{
	list<WriteOp *>::iterator i;
	WriteOp *op, *other;

	// 查找到指定writeId的WriteOp
	i = find_if(mPendingWrites.begin(), mPendingWrites.end(), WriteIdMatcher(
			writeId));
	if (i == mPendingWrites.end())
		return NULL;
	other = *i;
	if (other->status < 0)
		// if the write is "bad" already, don't add more data to it
		return NULL;

	// Since we are cloning, "touch" the time
	other->enqueueTime = time(NULL);
	// offset/size/buffer are to be filled in
	op = new WriteOp(other->seq, other->chunkId, other->chunkVersion, 0, 0,
			NULL, other->writeId);
	return op;
}

/// 设置mPendingWrites中指定操作的状态
void ChunkManager::SetWriteStatus(int64_t writeId, int status)
{
	list<WriteOp *>::iterator i;
	WriteOp *op;

	i = find_if(mPendingWrites.begin(), mPendingWrites.end(), WriteIdMatcher(
			writeId));
	if (i == mPendingWrites.end())
		return;
	op = *i;
	op->status = status;

	KFS_LOG_VA_INFO("Setting the status of writeid: %d to %d", writeId, status);
}

/// 查找指定的WriteOp是否存在于mPendingWrites中
bool ChunkManager::IsValidWriteId(int64_t writeId)
{
	list<WriteOp *>::iterator i;

	i = find_if(mPendingWrites.begin(), mPendingWrites.end(), WriteIdMatcher(
			writeId));
	// valid if we don't hit the end of the list
	return (i != mPendingWrites.end());
}

/// 超时函数：备份chunk table，取消等待时间太长的操作，清除不活动的chunk文件
void ChunkManager::Timeout()
{
	time_t now = time(NULL);

#ifdef DEBUG
	verifyExecutingOnEventProcessor();
#endif

	if (now - mLastCheckpointTime > CKPT_TIME_INTERVAL)
	{
		Checkpoint();
		// if any writes have been around for "too" long, remove them
		// and reclaim memory
		ScavengePendingWrites();
		// cleanup inactive fd's and thereby free up fd's
		CleanupInactiveFds(true);
	}
}

/// 清除mPendingWrites中开头的几个超过MAX_PENDING_WRITE_LRU_SECS时长的写操作
void ChunkManager::ScavengePendingWrites()
{
	list<WriteOp *>::iterator i;
	WriteOp *op;
	time_t now = time(NULL);
	ChunkInfoHandle_t *cih;

	i = mPendingWrites.begin();

	/// 从mPendingWrites开头的地方开始清除超时的WriteOp，直到出现第一个未超时的操作
	while (i != mPendingWrites.end())
	{
		op = *i;
		// The list is sorted by enqueue time
		if (now - op->enqueueTime < MAX_PENDING_WRITE_LRU_SECS)
		{
			break;
		}
		// if it exceeds 5 mins, retire the op
		KFS_LOG_VA_DEBUG("Retiring write with id=%ld as it has been too long",
				op->writeId);
		mPendingWrites.pop_front();

		// 同时，如果超时的操作中打开的chunk文件超时，则关闭该chunk文件
		if ((GetChunkInfoHandle(op->chunkId, &cih) == 0) && (now
				- cih->lastIOTime >= INACTIVE_FDS_CLEANUP_INTERVAL_SECS))
		{
			// close the chunk only if it is inactive
			CloseChunk(op->chunkId);
		}

		delete op;
		i = mPendingWrites.begin();
	}
}

/// 同步op相关的DiskConnection中的数据到磁盘
int ChunkManager::Sync(WriteOp *op)
{
	if (!op->diskConnection)
	{
		return -1;
	}
	return op->diskConnection->Sync(op->waitForSyncDone);
}

/// 用于清除不活动的chunk文件
class InactiveFdCleaner
{
	time_t now;
public:
	InactiveFdCleaner(time_t n) :
		now(n)
	{
	}
	void operator()(
			const std::tr1::unordered_map<kfsChunkId_t, ChunkInfoHandle_t *>::value_type v)
	{
		ChunkInfoHandle_t *cih = v.second;

		// 主要检验一下项目的可用性: chunk文件指针，文件指针指向的文件描述符和时候到达清理
		// 的周期
		if ((!cih->dataFH) || (cih->dataFH->mFd < 0) || (now - cih->lastIOTime
				< INACTIVE_FDS_CLEANUP_INTERVAL_SECS)
				|| (cih->isBeingReplicated))
			return;

		// we have a valid file-id and it has been over 5 mins since we last did I/O on it.
		KFS_LOG_VA_DEBUG("cleanup: closing fileid = %d, for chunk = %ld",
				cih->dataFH->mFd,
				cih->chunkInfo.chunkId);
		// 关闭文件
		cih->Release();
	}
};

/// 清除不活动的chunk文件
void ChunkManager::CleanupInactiveFds(bool periodic)
{
	static time_t lastCleanupTime = time(0);
	time_t now = time(0);

	if (OPEN_FDS_LOW_WATERMARK == 0)
	{
		struct rlimit rlim;
		int res;

		// 获取进程的资源限制
		res = getrlimit(RLIMIT_NOFILE, &rlim);
		if (res == 0)
		{
			OPEN_FDS_LOW_WATERMARK = rlim.rlim_cur / 2;
			// bump the soft limit to the hard limit
			// 将软限制提升到硬限制
			rlim.rlim_cur = rlim.rlim_max;
			if (setrlimit(RLIMIT_NOFILE, &rlim) == 0)
			{
				KFS_LOG_VA_DEBUG("Setting # of open files to: %ld",
						rlim.rlim_cur);
				OPEN_FDS_LOW_WATERMARK = rlim.rlim_cur / 2;
			}
		}
	}

	// not enough time has elapsed
	// 如果这个函数是周期运行的，并且还没有到达下一次清理时间，则不做任何操作
	if (periodic
			&& (now - lastCleanupTime < INACTIVE_FDS_CLEANUP_INTERVAL_SECS))
		return;

	// 总共打开的文件数 = 本地磁盘打开的文件数 + 网络文件数
	int totalOpenFds = globals().ctrOpenDiskFds.GetValue()
			+ globals().ctrOpenNetFds.GetValue();

	// if we haven't cleaned up in 5 mins or if we too many fd's that
	// are open, clean up.
	// 如果不是非周期性的清理，并且打开的文件数目不到软限制（soft limit）的一般，则不清理
	if ((!periodic) && (totalOpenFds < OPEN_FDS_LOW_WATERMARK))
	{
		return;
	}

	// either we are periodic cleaning or we have too many FDs open
	lastCleanupTime = time(0);

	// 如果chunk table中有符合“()”条件的，则进行处理: 即关闭一定时间内未活动的文件
	for_each(mChunkTable.begin(), mChunkTable.end(), InactiveFdCleaner(now));
}

/// 获取回收站目录
string KFS::GetStaleChunkPath(const string &partition)
{
	return partition + "/lost+found/";
}


int64_t ChunkManager::GetTotalSpace() const
{

	int64_t availableSpace;
	if (mChunkDirs.size() > 1)
	{
		return mTotalSpace;
	}

	// report the space based on availability
#if defined(__APPLE__) || defined(__sun__) || (!defined(__i386__))
	struct statvfs result;

	if (statvfs(mChunkDirs[0].c_str(), &result) < 0)
	{
		KFS_LOG_VA_DEBUG("statvfs failed...returning %ld", mTotalSpace);
		return mTotalSpace;
	}
#else
	// we are on i386 on linux
	struct statvfs64 result;

	if (statvfs64(mChunkDirs[0].c_str(), &result) < 0)
	{
		KFS_LOG_VA_DEBUG("statvfs failed...returning %ld", mTotalSpace);
		return mTotalSpace;
	}

#endif

	if (result.f_frsize == 0)
		return mTotalSpace;

	// result.* is how much is available on disk; mUsedSpace is how
	// much we used up with chunks; so, the total storage available on
	// the drive is the sum of the two.  if we don't add mUsedSpace,
	// then all the chunks we write will get use the space on disk and
	// won't get acounted for in terms of drive space.
	availableSpace = result.f_bavail * result.f_frsize + mUsedSpace;
	// we got all the info...so report true value
	return min(availableSpace, mTotalSpace);
}

void ChunkManagerTimeoutImpl::Timeout()
{
	SubmitOp(&mTimeoutOp);
}
