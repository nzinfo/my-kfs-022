//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: Chunk.h 98 2008-07-23 17:25:21Z sriramsrao $
//
// Created 2006/03/22
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

#ifndef _CHUNKSERVER_CHUNK_H
#define _CHUNKSERVER_CHUNK_H

#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <boost/shared_ptr.hpp>

#include <vector>

#include "common/log.h"
#include "common/kfstypes.h"
#include "libkfsIO/FileHandle.h"
#include "libkfsIO/Checksum.h"
#include "Utils.h"

namespace KFS
{

///
/// @file Chunk.h
/// @brief Declarations related to a Chunk in KFS.
///


///
/// @brief ChunkInfo_t
/// For each chunk, the chunkserver maintains a meta-data file.  This
/// file defines the chunk attributes such as, the file it is
/// associated with, the chunk version #, and the checksums for each
/// block of the file.  For each chunk, this structure is read in at
/// startup time.
///

/// The max # of checksum blocks we have for a given chunk
/// 每一个chunk最多可以含有的校验块的数目，即：chunk大小/块大小
const uint32_t MAX_CHUNK_CHECKSUM_BLOCKS = CHUNKSIZE / CHECKSUM_BLOCKSIZE;

/// In the chunk header, we store up to 256 char of the file that
/// originally created the chunk.
const size_t MAX_FILENAME_LEN = 256;

/// TODO: CHUNK_META_MAGIC
const int CHUNK_META_MAGIC = 0xCAFECAFE;
const int CHUNK_META_VERSION = 0x1;

/// 保存在磁盘上的数据结构，包括：
struct DiskChunkInfo_t
{
	/// 构造的时候，meta magic和meta version都是默认值
	DiskChunkInfo_t() :
		metaMagic(CHUNK_META_MAGIC), metaVersion(CHUNK_META_VERSION)
	{
	}
	DiskChunkInfo_t(kfsFileId_t f, kfsChunkId_t c, off_t s, kfsSeq_t v) :
		metaMagic(CHUNK_META_MAGIC), metaVersion(CHUNK_META_VERSION),
				fileId(f), chunkId(c), chunkVersion(v), chunkSize(s), numReads(
						0)
	{
		memset(filename, 0, MAX_FILENAME_LEN);
	}

	/// 设置校验和为指定值
	void SetChecksums(const uint32_t *checksums)
	{
		memcpy(chunkBlockChecksum, checksums, MAX_CHUNK_CHECKSUM_BLOCKS
				* sizeof(uint32_t));
	}

	/// TODO: 还不知道是什么功能...
	int metaMagic;
	int metaVersion;

	/// 所属文件描述符
	kfsFileId_t fileId;
	/// chunk ID
	kfsChunkId_t chunkId;
	/// chunkVersion可以粗略估计对于该块写的次数
	kfsSeq_t chunkVersion;
	off_t chunkSize;
	/// chunk中各个块的校验和
	uint32_t chunkBlockChecksum[MAX_CHUNK_CHECKSUM_BLOCKS];
	// some statistics about the chunk:
	// -- version # has an estimate of the # of writes
	// -- track the # of reads
	// ...
	uint32_t numReads;
	/// TODO: 文件名：用户定义的文件名还是系统存储在磁盘上时的文件名？？
	char filename[MAX_FILENAME_LEN];
};

// This structure is in-core
/// 存储在内核当中的数据结构
struct ChunkInfo_t
{

	ChunkInfo_t() :
		fileId(0), chunkId(0), chunkVersion(0), chunkSize(0),
				chunkBlockChecksum(NULL)
	{
		// memset(chunkBlockChecksum, 0, sizeof(chunkBlockChecksum));
		// memset(filename, 0, MAX_FILENAME_LEN);
	}
	~ChunkInfo_t()
	{
		delete[] chunkBlockChecksum;
	}
	///
	ChunkInfo_t(const ChunkInfo_t &other) :
		fileId(other.fileId), chunkId(other.chunkId), chunkVersion(
				other.chunkVersion), chunkSize(other.chunkSize),
				chunkBlockChecksum(NULL)
	{

	}

	/// 拷贝构造，所有信息均原封不动，不是共享而是拷贝
	ChunkInfo_t& operator=(const ChunkInfo_t &other)
	{
		fileId = other.fileId;
		chunkId = other.chunkId;
		chunkVersion = other.chunkVersion;
		chunkSize = other.chunkSize;
		SetChecksums(other.chunkBlockChecksum);
		return *this;
	}

	void setFilename(const char *other)
	{
		// strncpy(filename, other, MAX_FILENAME_LEN);
	}
	void Init(kfsFileId_t f, kfsChunkId_t c, off_t v)
	{
		fileId = f;
		chunkId = c;
		chunkVersion = v;
		chunkBlockChecksum = new uint32_t[MAX_CHUNK_CHECKSUM_BLOCKS];
		memset(chunkBlockChecksum, 0, MAX_CHUNK_CHECKSUM_BLOCKS
				* sizeof(uint32_t));
	}

	/// 将chunk的校验和读入到内存中
	/// 如果checksum已经读入到内存中，则其起始地址必不为0
	bool AreChecksumsLoaded()
	{
		return chunkBlockChecksum != NULL;
	}

	/// 清除内存中的chunksum
	void UnloadChecksums()
	{
		delete[] chunkBlockChecksum;
		chunkBlockChecksum = NULL;
		KFS_LOG_VA_DEBUG("Unloading in the chunk checksum for chunk %d", chunkId);
	}

	/// 需要先清除原先的checksum
	void SetChecksums(const uint32_t *checksums)
	{
		delete[] chunkBlockChecksum;
		if (checksums == NULL)
		{
			chunkBlockChecksum = NULL;
			return;
		}

		chunkBlockChecksum = new uint32_t[MAX_CHUNK_CHECKSUM_BLOCKS];
		memcpy(chunkBlockChecksum, checksums, MAX_CHUNK_CHECKSUM_BLOCKS
				* sizeof(uint32_t));
	}

	void LoadChecksums(int fd)
	{
		Deserialize(fd, true);
	}

	/// 判断校验和是否已经读入内存
	void VerifyChecksumsLoaded()
	{
		assert(chunkBlockChecksum != NULL);
		if (chunkBlockChecksum == NULL)
			die("Checksums are not loaded!");
	}

	// save the chunk meta-data to the buffer;
	/// 将chunk信息转换成DiskChunkInfo_t，再以字符串的形式存放到缓存中
	void Serialize(IOBuffer *dataBuf)
	{
		DiskChunkInfo_t dci(fileId, chunkId, chunkSize, chunkVersion);

		assert(chunkBlockChecksum != NULL);
		dci.SetChecksums(chunkBlockChecksum);

		dataBuf->CopyIn((char *) &dci, sizeof(DiskChunkInfo_t));
	}

	/// 将指定文件中的chenksum读入到内存中，放到本类的chunkBlockChecksum中
	int Deserialize(int fd, bool validate)
	{
		DiskChunkInfo_t dci;
		int res;

		res = pread(fd, &dci, sizeof(DiskChunkInfo_t), 0);
		if (res != sizeof(DiskChunkInfo_t))
			return -EINVAL;
		return Deserialize(dci, validate);
	}

	/// 将DiskChunkInfo_t形式的信息转换成ChunkInfo_t形式的信息；如果alidate为真，则需要
	/// 校验DiskChunkInfo_t中的meta magic和meta version是否为初始值.
	int Deserialize(const DiskChunkInfo_t &dci, bool validate)
	{
		if (validate)
		{
			if (dci.metaMagic != CHUNK_META_MAGIC)
			{
				KFS_LOG_VA_INFO("Magic # mismatch (got: %x, expect: %x)",
						dci.metaMagic, CHUNK_META_MAGIC);
				return -EINVAL;
			}
			if (dci.metaVersion != CHUNK_META_VERSION)
			{
				KFS_LOG_VA_INFO("Version # mismatch (got: %x, expect: %x)",
						dci.metaVersion, CHUNK_META_VERSION);
				return -EINVAL;
			}
		}
		fileId = dci.fileId;
		chunkId = dci.chunkId;
		chunkSize = dci.chunkSize;
		chunkVersion = dci.chunkVersion;

		delete[] chunkBlockChecksum;
		chunkBlockChecksum = new uint32_t[MAX_CHUNK_CHECKSUM_BLOCKS];
		memcpy(chunkBlockChecksum, dci.chunkBlockChecksum,
				MAX_CHUNK_CHECKSUM_BLOCKS * sizeof(uint32_t));
		KFS_LOG_VA_DEBUG("Loading in the chunk checksum for chunk %d", chunkId);

		return 0;
	}

	/// kfs的文件描述符
	kfsFileId_t fileId;
	kfsChunkId_t chunkId;
	kfsSeq_t chunkVersion;

	// chunk的大小，不是chunk对应文件的大小，这两个大小相差一个chunk header
	off_t chunkSize;

	/// TODO: what does this para mean?
	// uint32_t chunkBlockChecksum[MAX_CHUNK_CHECKSUM_BLOCKS];
	// this is unpinned; whenever we open the chunk, this has to be
	// paged in...damn..would've been nice if this was at the end
	uint32_t *chunkBlockChecksum;
	// some statistics about the chunk:
	// -- version # has an estimate of the # of writes
	// -- track the # of reads
	// ...
	/// 该块读的次数，写的次数可以通过version粗略估计
	uint32_t numReads;
	// char filename[MAX_FILENAME_LEN];
};
}

#endif // _CHUNKSERVER_CHUNK_H
