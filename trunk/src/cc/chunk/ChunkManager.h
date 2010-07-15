//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: ChunkManager.h 76 2008-07-10 01:06:52Z sriramsrao $
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
// \file ChunkManager.h
// \brief Handles all chunk related ops.
//
//----------------------------------------------------------------------------

#ifndef _CHUNKMANAGER_H
#define _CHUNKMANAGER_H

#include <tr1/unordered_map>
#include <vector>
#include <string>

#include "libkfsIO/ITimeout.h"
#include "libkfsIO/DiskManager.h"
#include "libkfsIO/Globals.h"
#include "Chunk.h"
#include "KfsOps.h"
#include "Logger.h"
#include "common/cxxutil.h"

namespace KFS
{

/// We allow a chunk header upto 16K in size
/// chunk头最大允许值为16k
const size_t KFS_CHUNK_HEADER_SIZE = 16384;

/// Encapsulate a chunk file descriptor and information about the
/// chunk such as name and version #.
/// chunk在Linux文件系统中是以文件的形式存储的，文件名即为chunk ID
/// 这个类中包含chunk信息和对应的文件句柄
struct ChunkInfoHandle_t
{
	ChunkInfoHandle_t() :
		lastIOTime(0), isBeingReplicated(false), isMetadataReadOngoing(false),
				readChunkMetaOp(NULL)
	{
	}
	;

	struct ChunkInfo_t chunkInfo;
	/// Chunks are stored as files in he underlying filesystem; each
	/// chunk file is named by the chunkId.  Each chunk has a header;
	/// this header is hidden from clients; all the client I/O is
	/// offset by the header amount
	FileHandlePtr dataFH;
	time_t lastIOTime; // when was the last I/O done on this chunk

	/// 该chunk是否正在从其他服务器上拷贝
	bool isBeingReplicated;

	/// set if a read request for the chunk meta-data has been issued to disk.
	/// 如果我们已经发送了一个读请求，并且尚未完成，则不再提交新的请求
	bool isMetadataReadOngoing;
	/// keep track of the op that is doing the read
	ReadChunkMetaOp *readChunkMetaOp;

	/// 关闭文件
	void Release()
	{
		chunkInfo.UnloadChecksums();
		dataFH->Close();
		libkfsio::globals().ctrOpenDiskFds.Update(-1);
	}

	void Init(int fd)
	{
		dataFH.reset(new FileHandle_t(fd));
	}
};

/// Map from a chunk id to a chunk handle
/// chunk ID到chunk句柄的映射
typedef std::tr1::unordered_map<kfsChunkId_t, ChunkInfoHandle_t *> CMap;
/// chunk table迭代器
typedef std::tr1::unordered_map<kfsChunkId_t, ChunkInfoHandle_t *>::const_iterator
		CMI;

/// Periodically write out the chunk manager state to disk
class ChunkManagerTimeoutImpl;

/// The chunk manager writes out chunks as individual files on disk.
/// The location of the chunk directory is defined by chunkBaseDir.
/// The file names of chunks is a string representation of the chunk
/// id.  The chunk manager performs disk I/O asynchronously---that is,
/// it schedules disk requests to the Disk manager which uses aio() to
/// perform the operations.
///
class ChunkManager
{
public:
	ChunkManager();

	/// 析构时清空其中所有的chunk句柄（直接清除内存）
	~ChunkManager();

	/// Init function to configure the chunk manager object.
	/// 初始化时，并不读入chunk在磁盘上的信息
	void Init(const std::vector<std::string> &chunkDirs, int64_t totalSpace);

	/// Allocate a file to hold a chunk on disk.  The filename is the
	/// chunk id itself.
	/// @param[in] fileId  id of the file that has chunk chunkId
	/// @param[in] chunkId id of the chunk being allocated.
	/// @param[in] chunkVersion  the version assigned by the metaserver to this chunk
	/// @param[in] isBeingReplicated is the allocation for replicating a chunk?
	/// @retval status code
	/// 为一个chunk在Linux文件系统上分配一个文件
	/// 为指定文件的指定chunk在chunk table（打开的chunk列表）中分配一个空间
	int AllocChunk(kfsFileId_t fileId, kfsChunkId_t chunkId,
			int64_t chunkVersion, bool isBeingReplicated = false);

	/// Delete a previously allocated chunk file.
	/// @param[in] chunkId id of the chunk being deleted.
	/// @retval status code
	/// 从Linux文件系统中删除指定的chunk（同时必然删除chunk table中对应的项）
	int DeleteChunk(kfsChunkId_t chunkId);

	/// A previously created chunk is stale; move it to stale chunks
	/// dir; space can be reclaimed later
	///
	/// @param[in] chunkId id of the chunk being moved
	/// @retval status code
	/// 彻底删除一个chunk（从chunk table中清除，但没有清除Linux文件系统中的存储）
	int StaleChunk(kfsChunkId_t chunkId);

	/// Truncate a chunk to the specified size
	/// @param[in] chunkId id of the chunk being truncated.
	/// @param[in] chunkSize  size to which chunk should be truncated.
	/// @retval status code
	/// 将chunk ID指定的chunk大小改变为chunkSize指定的大小
	int TruncateChunk(kfsChunkId_t chunkId, off_t chunkSize);

	/// Change a chunk's version # to what the server says it should be.
	/// @param[in] fileId  id of the file that has chunk chunkId
	/// @param[in] chunkId id of the chunk being allocated.
	/// @param[in] chunkVersion  the version assigned by the metaserver to this chunk
	/// @retval status code
	/// 更新chunk的版本号为chunkVersion指定的版本号。更改之后，原版本在Linux文件系统中的
	/// 存储就消失了
	int ChangeChunkVers(kfsFileId_t fileId, kfsChunkId_t chunkId,
			int64_t chunkVersion);

	/// Open a chunk for I/O.
	/// @param[in] chunkId id of the chunk being opened.
	/// @param[in] openFlags  O_RDONLY, O_WRONLY
	/// @retval status code
	/// 打开指定chunk对应的Linux文件
	/// openFlags: O_RDONLY, O_WRONLY
	int OpenChunk(kfsChunkId_t chunkId, int openFlags);

	/// Close a previously opened chunk and release resources.
	/// @param[in] chunkId id of the chunk being closed.
	/// 关闭chunk
	/// @note 关闭指定chunk对应的文件，但并不清除mChunkTable中的定义
	void CloseChunk(kfsChunkId_t chunkId);

	/// Schedule a read on a chunk.
	/// @param[in] op  The read operation being scheduled.
	/// @retval 0 if op was successfully scheduled; -1 otherwise
	/// 从chunkserver磁盘读入指定chunk，指定字节的数据
	/// @note 若不足完整的校验块，需按完整的校验块读入数据，以便于校验
	int ReadChunk(ReadOp *op);

	/// Schedule a write on a chunk.
	/// @param[in] op  The write operation being scheduled.
	/// @retval 0 if op was successfully scheduled; -1 otherwise
	/// 写磁盘数据。如果要写入的数据不是从一个校验块的首地址开始的，并且rop（读操作）为空，则
	/// 不进行操作
	int WriteChunk(WriteOp *op);

	/// Write/read out/in the chunk meta-data and notify the cb when the op
	/// is done.
	/// @retval 0 if op was successfully scheduled; -errno otherwise
	/// 将chunk的全局信息写入到meta server中
	/// chunk全局信息在在文件的起始位置
	int WriteChunkMetadata(kfsChunkId_t chunkId, KfsOp *cb);

	// 从指定chunk对应的Linux文件中读入meta信息
	// chunk的meta信息存放在文件的首部，长度为KFS_CHUNK_HEADER_SIZE个字节
	int ReadChunkMetadata(kfsChunkId_t chunkId, KfsOp *cb);

	/// Notification that read is finished
	/// 完成chunk meta信息的读取：置isMetadataReadOngoing为false，readChunkMetaOp为空
	void ReadChunkMetadataDone(kfsChunkId_t chunkId);

	/// We read the chunk metadata out of disk; we update the chunk
	/// table with this info.
	/// @retval 0 if successful (i.e., valid chunkid); -EINVAL otherwise
	/// 设置指定chunk的meta信息：修改chunk中所有校验块的校验和
	int SetChunkMetadata(const DiskChunkInfo_t &dci);
	bool IsChunkMetadataLoaded(kfsChunkId_t chunkId)
	{
		ChunkInfoHandle_t *cih = NULL;

		if (GetChunkInfoHandle(chunkId, &cih) < 0)
			return false;
		return cih->chunkInfo.AreChecksumsLoaded();
	}

	/// A previously scheduled write op just finished.  Update chunk
	/// size and the amount of used space.
	/// @param[in] op  The write op that just finished
	///
	/// 完成读chunk操作：如果校验和全部匹配，则只需要截去首尾无用数据；否则需通知metaserver
	/// 重新复制该块
	void ReadChunkDone(ReadOp *op);

	/// 完成chunk复制：标记mIsChunkTableDirty为true，isBeingReplicated为false
	void ReplicationDone(kfsChunkId_t chunkId);
	/// Determine the size of a chunk.
	/// @param[in] chunkId  The chunk whose size is needed
	/// @param[out] chunkSize  The size of the chunk
	/// @retval status code
	/// 通过chunk table中的信息获取chunk大小
	int ChunkSize(kfsChunkId_t chunkId, off_t *chunkSize);

	/// Cancel a previously scheduled chunk operation.
	/// @param[in] cont   The callback object that scheduled the
	///  operation
	/// @param[in] chunkId  The chunk on which ops were scheduled
	void CancelChunkOp(KfsCallbackObj *cont, kfsChunkId_t chunkId);

	/// Register a timeout handler with the net manager for taking
	/// checkpoints.  Also, get the logger going
	/// 启动chunkManager，即向全局变量的netManager中注册定时处理程序
	void Start();

	/// Write out the chunk table data structure to disk
	/// 将内存中的chunk table存放到本地磁盘上
	void Checkpoint();

	/// Read the chunk table from disk following a restart.  See
	/// comments in the method for issues relating to validation (such
	/// as, checkpoint contains a chunk name, but the associated file
	/// is not there on disk, etc.).
	void Restart();

	/// On a restart following a dirty shutdown, do log replay.  This
	/// involves updating the Chunk table map to reflect operations
	/// that are in the log.

	/// When a checkpoint file is read, update the mChunkTable[] to
	/// include a mapping for cih->chunkInfo.chunkId.
	/// 向chunk table中添加chunk句柄
	void AddMapping(ChunkInfoHandle_t *cih);

	/// Replay a chunk allocation.
	///
	/// @param[in] fileId  id of the file that has chunk chunkId
	/// @param[in] chunkId  Update the mChunkTable[] to include this
	/// chunk id
	/// @param[in] chunkVersion  the version assigned by the
	/// metaserver to this chunk.
	/// 重新将指定的chunk加入到本地磁盘
	/// @note 与AllocChunk的区别在于：这个函数不进行实际的创建文件操作，只修改chunk table中有关
	void ReplayAllocChunk(kfsFileId_t fileId, kfsChunkId_t chunkId,
			int64_t chunkVersion);

	/// Replay a chunk version # change.
	///
	/// @param[in] fileId  id of the file that has chunk chunkId
	/// @param[in] chunkId  Update the mChunkTable[] with the changed
	/// version # for this chunkId
	/// @param[in] chunkVersion  the version assigned by the
	/// metaserver to this chunk.
	/// 重新修改chunk table中的chunk版本号
	/// @note 与ChangeChunkVers的不同在于：此函数并没有进行实质上的重命名操作
	void ReplayChangeChunkVers(kfsFileId_t fileId, kfsChunkId_t chunkId,
			int64_t chunkVersion);

	/// Replay a chunk deletion
	/// @param[in] chunkId  Update the mChunkTable[] to remove this
	/// chunk id
	/// 重演删除chunk操作，但并不进行实际的删除文件操作，用以保证占用大小统计的正确性
	/// 在文件删除后，chunk table中的目录仍然存在时
	void ReplayDeleteChunk(kfsChunkId_t chunkId);

	/// Replay a write done on a chunk.
	/// @param[in] chunkId  Update the size of chunk to reflect the
	/// completion of a write.
	/// @param[in] chunkSize The new size of the chunk
	/// 重演写完成操作，将指定chunk对应的句柄中的校验和更新为指定校验和（没有写磁盘操作），同时
	/// 更新已用磁盘统计数据
	/// @note 系统中并没有WriteDone接口
	void ReplayWriteDone(kfsChunkId_t chunkId, off_t chunkSize, off_t offset,
			std::vector<uint32_t> checksum);

	/// Replay a truncation done on a chunk.
	/// @param[in] chunkId  Update the size of chunk to reflect the
	/// completion of a truncation
	/// @param[in] chunkSize The new size of the chunk
	/// 重演更改文件大小的操作
	/// @note 本系统中并没有TruncateDone接口
	void ReplayTruncateDone(kfsChunkId_t chunkId, off_t chunkSize);

	/// Retrieve the chunks hosted on this chunk server.
	/// @param[out] result  A vector containing info of all chunks
	/// hosted on this server.
	/// 获取本地磁盘上的所有chunk句柄
	void GetHostedChunks(std::vector<ChunkInfo_t> &result);

	/// Return the total space that is exported by this server.  If
	/// chunks are stored in a single directory, we use statvfs to
	/// determine the total space avail; we report the min of statvfs
	/// value and the configured mTotalSpace.
	int64_t GetTotalSpace() const;
	int64_t GetUsedSpace() const
	{
		return mUsedSpace;
	}
	;
	long GetNumChunks() const
	{
		return mNumChunks;
	}
	;

	/// For a write, the client is defining a write operation.  The op
	/// is queued and the client pushes data for it subsequently.
	/// @param[in] wi  The op that defines the write
	/// @retval status code
	/// 根据指定的WriteIdAllocOp，生成一个WriteOp操作，存放到mPendingWrites中
	int AllocateWriteId(WriteIdAllocOp *wi);

	/// For a write, the client has pushed data to us.  This is queued
	/// for a commit later on.
	/// @param[in] wp  The op that needs to be queued
	/// @retval status code
	/// 将wp中缓冲器中的数据合并到mPendingWrites中WriteId相同的WriteOp中
	int EnqueueWrite(WritePrepareOp *wp);

	/// Check if a write is pending to a chunk.
	/// @param[in] chunkId  The chunkid for which we are checking for
	/// pending write(s).
	/// @retval True if a write is pending; false otherwise
	bool IsWritePending(kfsChunkId_t chunkId);

	/// Given a chunk id, return its version
	int64_t GetChunkVersion(kfsChunkId_t c);

	/// if the chunk exists and has a valid version #, then we need to page in the chunk meta-data.
	bool NeedToReadChunkMetadata(kfsChunkId_t c)
	{
		return GetChunkVersion(c) > 0;
	}

	/// Retrieve the write op given a write id.
	/// @param[in] writeId  The id corresponding to a previously
	/// enqueued write.
	/// @retval WriteOp if one exists; NULL otherwise
	/// 从mPendingWrites中获取指定writeId的WriteOp，并从mPendingWrites中删除这个WriteOp
	WriteOp *GetWriteOp(int64_t writeId);

	/// The model with writes: allocate a write id (this causes a
	/// write-op to be created); then, push data for writes (which
	/// retrieves the write-op and then sends writes down to disk).
	/// The "clone" method makes a copy of a previously created
	/// write-op.
	/// @param[in] writeId the write id that was previously assigned
	/// @retval WriteOp if one exists; NULL otherwise
	/// 从mPendingWrites克隆出一个WriteOp，该函数与GetWriteOp的区别在于不删除mPendingWrites
	/// 中的操作
	WriteOp *CloneWriteOp(int64_t writeId);

	/// Set the status for a given write id
	/// 设置mPendingWrites中指定操作的状态
	void SetWriteStatus(int64_t writeId, int status);

	/// Is the write id a valid one
	/// 查找指定的WriteOp是否存在于mPendingWrites中
	bool IsValidWriteId(int64_t writeId);

	/// 超时函数：备份chunk table，取消等待时间太长的操作，清除不活动的chunk文件
	void Timeout();

	/// Push the changes from the write out to disk
	/// 同步op相关的DiskConnection中的数据到磁盘
	int Sync(WriteOp *op);

	/// return 0 if the chunkId is good; -EBADF otherwise
	int GetChunkChecksums(kfsChunkId_t chunkId, uint32_t **checksums)
	{
		ChunkInfoHandle_t *cih = NULL;

		if (GetChunkInfoHandle(chunkId, &cih) < 0)
			return -EBADF;
		*checksums = cih->chunkInfo.chunkBlockChecksum;
		return 0;
	}

private:
	/// How long should a pending write be held in LRU
	static const int MAX_PENDING_WRITE_LRU_SECS = 300;
	/// take a checkpoint once every 2 mins
	static const int CKPT_TIME_INTERVAL = 120;

	/// space available for allocation
	int64_t mTotalSpace;
	/// how much is used up by chunks
	int64_t mUsedSpace;

	/// how many chunks are we hosting
	/// 系统中正在管理的chunk数目
	long mNumChunks;

	time_t mLastCheckpointTime;

	/// directories for storing the chunks
	/// 存储块的Linux文件系统的路径
	std::vector<std::string> mChunkDirs;

	/// See the comments in KfsOps.cc near WritePreapreOp related to write handling
	int64_t mWriteId;
	std::list<WriteOp *> mPendingWrites;

	/// on a timeout, the timeout interface will force a checkpoint
	/// and query the disk manager for data
	ChunkManagerTimeoutImpl *mChunkManagerTimeoutImpl;

	/// when taking checkpoints, write one out only if the chunk table
	/// is dirty.
	bool mIsChunkTableDirty;
	/// table that maps chunkIds to their associated state
	/// chunk ID到chunk句柄的映射，记录manager中每一个chunk句柄的地址
	/// 该变量中包含chunk server中所有chunk ID到chunk句柄的映射
	CMap mChunkTable;

	/// Given a chunk file name, extract out the
	/// fileid/chunkid/chunkversion from it and build a chunkinfo structure
	/// 给定完整的文件路径名称和相对于文件首的offset，获取chunk信息句柄
	/// @note 获得的chunk句柄中数据（chunk data）的大小 = filesz - KFS_CHUNK_HEADER_SIZE
	void MakeChunkInfoFromPathname(const std::string &pathname, off_t filesz,
			ChunkInfoHandle_t **result);

	/// Utility function that given a chunkId, returns the full path
	/// to the chunk filename.
	/// 从本地内存中获取chunk在Linux文件系统中的完整路径
	std::string MakeChunkPathname(kfsFileId_t fid, kfsChunkId_t chunkId,
			kfsSeq_t chunkVersion);

	/// Utility function that given a chunkId, returns the full path
	/// to the chunk filename in the "stalechunks" dir
	/// 获取staled chunk对应的Linux文件的绝对路径
	/// staled路径和普通路径的关系： staled = 原路径 + "/lost+found/".
	std::string MakeStaleChunkPathname(kfsFileId_t fid, kfsChunkId_t chunkId,
			kfsSeq_t chunkVersion);

	/// Utility function that sets up a disk connection for an
	/// I/O operation on a chunk.
	/// @param[in] chunkId  Id of the chunk on which we are doing I/O
	/// @param[in] op   The KfsOp that is being on the chunk
	/// @retval A disk connection pointer allocated via a call to new;
	/// it is the caller's responsibility to free the memory
	/// 建立磁盘连接，op指定DiskConnection的KfsCallbackObj...
	DiskConnection *SetupDiskConnection(kfsChunkId_t chunkId, KfsOp *op);

	/// Utility function that returns a pointer to mChunkTable[chunkId].
	/// @param[in] chunkId  the chunk id for which we want info
	/// @param[out] cih  the resulting pointer from mChunkTable[chunkId]
	/// @retval  0 on success; -EBADF if we can't find mChunkTable[chunkId]
	/// 查找对指定chunk ID对应的chunk句柄的内存地址（通过map查找）
	int GetChunkInfoHandle(kfsChunkId_t chunkId, ChunkInfoHandle_t **cih);

	/// Checksums are computed on 64K blocks.  To verify checksums on
	/// reads, reads are aligned at 64K boundaries and data is read in
	/// 64K blocks.  So, for reads that are un-aligned/read less data,
	/// adjust appropriately.
	// 调整读入的数据：因为数据的读入是以校验块为基本单位的，所以需要截去首尾多余的数据
	void AdjustDataRead(ReadOp *op);

	/// Pad the buffer with sufficient 0's so that checksumming works
	/// out.
	/// @param[in/out] buffer  The buffer to be padded with 0's
	/// 若buffer结尾部分不是一个完整的校验块，则进行零填充，补齐一个完整的校验块
	void ZeroPad(IOBuffer *buffer);

	/// Given a chunkId and offset, return the checksum of corresponding
	/// "checksum block"---i.e., the 64K block that contains offset.
	/// 获取offset所在校验块的校验和
	uint32_t GetChecksum(kfsChunkId_t chunkId, off_t offset);

	/// For any writes that have been held for more than 2 mins,
	/// scavenge them and reclaim memory.
	/// 清除mPendingWrites中开头的几个超过MAX_PENDING_WRITE_LRU_SECS时长的写操作
	void ScavengePendingWrites();

	/// If we have too many open fd's close out whatever we can.  When
	/// periodic is set, we do a scan and clean up.
	/// 清除不活动的chunk文件
	void CleanupInactiveFds(bool periodic = false);

	/// Notify the metaserver that chunk chunkId is corrupted; the
	/// metaserver will re-replicate this chunk and for now, won't
	/// send us traffic for this chunk.
	/// 通知metaserver，chunkID指定的chunk已损坏
	void NotifyMetaCorruptedChunk(kfsChunkId_t chunkId);

	/// Get all the chunk filenames into a single array.
	/// @retval on success, # of entries in the array;
	///         on failures, -1
	/// 从系统中取出所有的chunk的Linux文件路径，并且存放在一个数组namelist中
	/// @note 如果有一个目录读取失败，则退出函数，不返回任何值
	int GetChunkDirsEntries(struct dirent ***namelist);

	/// Get all the chunk pathnames into a single vector
	/// 获取chunk server中所有的chunk文件的完整（路径）名称，存放到pathname中
	/// @note 如果其中有目录读取失败，则跳过失败的目录继续读取下一个目录
	void GetChunkPathEntries(std::vector<std::string> &pathnames);

	/// Helper function to move a chunk to the stale dir
	/// 将chunk放到回收站里边：改变存储路径即可
	void MarkChunkStale(kfsFileId_t fid, kfsChunkId_t chunkId,
			kfsSeq_t chunkVersion);

	/// Code paths for restoring chunk-meta data.  This is older
	/// version where there is a checkpoint file that contains the
	/// chunk meta data.  On a restart, we restore the data from the
	/// checkpoint and then upgrade to V2.
	void RestoreV1();
	/// This version has the "<chunkId>.meta" file; one per chunk
	/// 恢复系统: 从文件系统读入所有的chunk文件信息，并解析出chunk句柄放到chunk table中
	void RestoreV2();
	/// Restore the chunk meta-data from the specified file name.
	void RestoreChunkMeta(const std::string &chunkMetaFn);

	/// Update the checksums in the chunk metadata based on the op.
	/// 更新checksum和已经使用的空间数
	/// TODO: 为什么在这里更新已经使用的空间数呢？
	void UpdateChecksums(ChunkInfoHandle_t *cih, WriteOp *op);
};

/// A Timeout interface object for taking checkpoints on the
/// ChunkManager object.
class ChunkManagerTimeoutImpl: public ITimeout
{
public:
	ChunkManagerTimeoutImpl(ChunkManager *mgr) :
		mTimeoutOp(0)
	{
		mChunkManager = mgr;
		// set a checkpoint once every min.
		// SetTimeoutInterval(60*1000);
	}
	;
	void Timeout();
private:
	/// Owning chunk manager
	ChunkManager *mChunkManager;
	TimeoutOp mTimeoutOp;
};

extern ChunkManager gChunkManager;

/// Given a partition that holds chunks, get the path to the directory
/// that is used to keep the stale chunks (from this partition)
/// 获取回收站目录
std::string GetStaleChunkPath(const std::string &partition);

}

#endif // _CHUNKMANAGER_H
