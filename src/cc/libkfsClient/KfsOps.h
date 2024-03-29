//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: KfsOps.h 162 2008-09-21 19:33:12Z sriramsrao $
//
// Created 2006/05/24
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

#ifndef _LIBKFSCLIENT_KFSOPS_H
#define _LIBKFSCLIENT_KFSOPS_H

#include <algorithm>
#include <string>
#include <sstream>
#include <vector>

#include "common/kfstypes.h"
#include "KfsAttr.h"

#include "common/properties.h"

namespace KFS
{

/// 操作类型
enum KfsOp_t
{
	CMD_UNKNOWN,
	// Meta-data server RPCs
	CMD_GETALLOC,
	CMD_GETLAYOUT,
	CMD_ALLOCATE,
	CMD_TRUNCATE,
	CMD_LOOKUP,
	CMD_MKDIR,
	CMD_RMDIR,
	CMD_READDIR,
	CMD_READDIRPLUS,
	CMD_CREATE,
	CMD_REMOVE,
	CMD_RENAME,
	CMD_LEASE_ACQUIRE,
	CMD_LEASE_RENEW,
	CMD_CHANGE_FILE_REPLICATION,
	// Chunkserver RPCs
	CMD_OPEN,
	CMD_CLOSE,
	CMD_READ,
	CMD_WRITE_ID_ALLOC,
	CMD_WRITE_PREPARE,
	CMD_WRITE_SYNC,
	CMD_SIZE,
	CMD_GET_CHUNK_METADATA,
	CMD_NCMDS
};

/// @brief 所有操作的基类
struct KfsOp
{
	KfsOp_t op;		/// 操作的类型
	kfsSeq_t seq;	/// 操作序号
	int32_t status;	///
	uint32_t checksum; /// a checksum over the data
	size_t contentLength;	/// 内容长度
	size_t contentBufLen;	/// 内容缓存长度
	char *contentBuf;		/// 缓存
	///
	KfsOp(KfsOp_t o, kfsSeq_t s) :
		op(o), seq(s), status(0), checksum(0), contentLength(0), contentBufLen(
				0), contentBuf(NULL)
	{
	}

	/// to allow dynamic-type-casting, make the destructor virtual
	virtual ~KfsOp()
	{
		if (contentBuf != NULL)
			delete[] contentBuf;
	}

	/// 将缓存指定为buf，长度指定为len
	void AttachContentBuf(const char *buf, size_t len)
	{
		AttachContentBuf((char *) buf, len);
	}

	/// 同上，指定缓存位置和长度
	void AttachContentBuf(char *buf, size_t len)
	{
		contentBuf = buf;
		contentBufLen = len;
	}
	/// 不释放内存，是因为这个类根本就没有开辟过内存空间，所有的内存全是引用的，不能释放
	void ReleaseContentBuf()
	{
		contentBuf = NULL;
		contentBufLen = 0;
	}
	/// Build a request RPC that can be sent to the server
	/// 建立server的远程系统调用
	virtual void Request(std::ostringstream &os) = 0;

	///
	/// @brief 解析响应的头，并且把状态和内容长度存入全局量中，其他的放在prop中
	/// @param[in] resp 要解析的字符串
	/// @param[out] prop 解析的结果
	///
	void ParseResponseHeaderCommon(std::string &resp, Properties &prop);

	///
	/// @brief 解析回复
	/// @param[in] buf: buffer containing the response
	/// @param[in] len: str-len of the buffer.
	/// @note 目的是将操作中的状态和内容长度取出并且存放，其他信息直接丢弃
	///
	virtual void ParseResponseHeader(char *buf, int len);

	/// Return information about op that can printed out for debugging.
	virtual std::string Show() const = 0;
};

/// @brief 用于创建文件，操作的结果返回文件ID
struct CreateOp: public KfsOp
{
	kfsFileId_t parentFid; /// input parent file-id
	const char *filename;///
	kfsFileId_t fileId; /// result
	int numReplicas; /// desired degree of replication
	bool exclusive; /// O_EXCL flag

	///
	CreateOp(kfsSeq_t s, kfsFileId_t p, const char *f, int n, bool e) :
		KfsOp(CMD_CREATE, s), parentFid(p), filename(f), numReplicas(n),
				exclusive(e)
	{

	}
	///
	void Request(std::ostringstream &os);
	///
	void ParseResponseHeader(char *buf, int len);
	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "create: " << filename << " (parentfid = " << parentFid << ")";
		return os.str();
	}
};

/// @brief 删除指定文件
struct RemoveOp: public KfsOp
{
	kfsFileId_t parentFid; /// input parent file-id
	const char *filename;///

	///
	RemoveOp(kfsSeq_t s, kfsFileId_t p, const char *f) :
		KfsOp(CMD_REMOVE, s), parentFid(p), filename(f)
	{

	}
	///
	void Request(std::ostringstream &os);
	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "remove: " << filename << " (parentfid = " << parentFid << ")";
		return os.str();
	}
};

/// @brief 创建目录
struct MkdirOp: public KfsOp
{
	kfsFileId_t parentFid; /// input parent file-id
	const char *dirname;///
	kfsFileId_t fileId; /// result
	///
	MkdirOp(kfsSeq_t s, kfsFileId_t p, const char *d) :
		KfsOp(CMD_MKDIR, s), parentFid(p), dirname(d)
	{

	}
	///
	void Request(std::ostringstream &os);
	///
	void ParseResponseHeader(char *buf, int len);
	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "mkdir: " << dirname << " (parentfid = " << parentFid << ")";
		return os.str();
	}
};

/// @brief 用于删除目录的操作命令
struct RmdirOp: public KfsOp
{
	kfsFileId_t parentFid; /// input parent file-id
	const char *dirname;///
	///
	RmdirOp(kfsSeq_t s, kfsFileId_t p, const char *d) :
		KfsOp(CMD_RMDIR, s), parentFid(p), dirname(d)
	{

	}
	///
	void Request(std::ostringstream &os);
	// default parsing of OK/Cseq/Status/Content-length will suffice.

	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "rmdir: " << dirname << " (parentfid = " << parentFid << ")";
		return os.str();
	}
};

/// @brief 重命名
struct RenameOp: public KfsOp
{
	kfsFileId_t parentFid; /// input parent file-id
	const char *oldname; /// old file name/dir
	const char *newpath; /// new path to be renamed to
	bool overwrite; /// set if the rename can overwrite newpath

	///
	RenameOp(kfsSeq_t s, kfsFileId_t p, const char *o, const char *n, bool c) :
		KfsOp(CMD_RENAME, s), parentFid(p), oldname(o), newpath(n),
				overwrite(c)
	{

	}

	///
	void Request(std::ostringstream &os);

	// default parsing of OK/Cseq/Status/Content-length will suffice.

	///
	std::string Show() const
	{
		std::ostringstream os;

		if (overwrite)
			os << "rename_overwrite: ";
		else
			os << "rename: ";
		os << " old=" << oldname << " (parentfid = " << parentFid << ")";
		os << " new = " << newpath;
		return os.str();
	}
};

/// @brief 类似READDIR的操作
struct ReaddirOp: public KfsOp
{
	kfsFileId_t fid; /// fid of the directory
	int numEntries; /// # of entries in the directory

	///
	ReaddirOp(kfsSeq_t s, kfsFileId_t f) :
		KfsOp(CMD_READDIR, s), fid(f), numEntries(0)
	{

	}

	///
	void Request(std::ostringstream &os);
	/// This will only extract out the default+num-entries.  The actual
	/// dir. entries are in the content-length portion of things
	void ParseResponseHeader(char *buf, int len);

	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "readdir: fid = " << fid;
		return os.str();
	}
};

/// @brief 类似READDIRPLUS的功能
struct ReaddirPlusOp: public KfsOp
{
	kfsFileId_t fid; /// fid of the directory
	int numEntries; /// # of entries in the directory

	///
	ReaddirPlusOp(kfsSeq_t s, kfsFileId_t f) :
		KfsOp(CMD_READDIRPLUS, s), fid(f), numEntries(0)
	{
	}

	///
	void Request(std::ostringstream &os);
	/// This will only extract out the default+num-entries.  The actual
	/// dir. entries are in the content-length portion of things
	void ParseResponseHeader(char *buf, int len);

	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "readdirplus: fid = " << fid;
		return os.str();
	}
};

// Lookup the attributes of a file in a directory
/// @brief 用于查询文件或目录的属性
struct LookupOp: public KfsOp
{
	kfsFileId_t parentFid; /// fid of the parent dir
	const char *filename; /// file in the dir
	KfsServerAttr fattr; /// result

	///
	LookupOp(kfsSeq_t s, kfsFileId_t p, const char *f) :
		KfsOp(CMD_LOOKUP, s), parentFid(p), filename(f)
	{

	}

	///
	void Request(std::ostringstream &os);

	///
	void ParseResponseHeader(char *buf, int len);

	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "lookup: " << filename << " (parentfid = " << parentFid << ")";
		return os.str();
	}
};

/// @brief Lookup the attributes of a file relative to a root dir.
struct LookupPathOp: public KfsOp
{
	kfsFileId_t rootFid; /// fid of the root dir
	const char *filename; /// path relative to root
	KfsServerAttr fattr; /// result
	///
	LookupPathOp(kfsSeq_t s, kfsFileId_t r, const char *f) :
		KfsOp(CMD_LOOKUP, s), rootFid(r), filename(f)
	{

	}
	///
	void Request(std::ostringstream &os);
	///
	void ParseResponseHeader(char *buf, int len);

	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "lookup_path: " << filename << " (rootFid = " << rootFid << ")";
		return os.str();
	}
};

/// @brief Get the allocation information for a chunk in a file.
// 定位某一数据块的物理位置等
struct GetAllocOp: public KfsOp
{
	kfsFileId_t fid;///
	off_t fileOffset;///
	kfsChunkId_t chunkId; /// result
	int64_t chunkVersion; /// result
	/// result: where the chunk is hosted name/port
	std::vector<ServerLocation> chunkServers;

	///
	GetAllocOp(kfsSeq_t s, kfsFileId_t f, off_t o) :
		KfsOp(CMD_GETALLOC, s), fid(f), fileOffset(o)
	{

	}

	///
	void Request(std::ostringstream &os);

	///
	void ParseResponseHeader(char *buf, int len);

	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "getalloc: fid=" << fid << " offset: " << fileOffset;
		return os.str();
	}
};

/// @brief 单个chunk的信息
struct ChunkLayoutInfo
{
	///
	ChunkLayoutInfo() :
		fileOffset(-1), chunkId(0)
	{
	}
	;
	off_t fileOffset;		/// 该chunk的起始地址在文件中的偏移
	kfsChunkId_t chunkId; 	/// result
	int64_t chunkVersion; 	/// result
	std::vector<ServerLocation> chunkServers; /// where the chunk lives
};

/// Get the layout information for all chunks in a file.
/// @brief 获取文件的全局属性：chunk的数目以及各个chunk的属性
struct GetLayoutOp: public KfsOp
{
	kfsFileId_t fid;///
	int numChunks;///
	std::vector<ChunkLayoutInfo> chunks;///
	///
	GetLayoutOp(kfsSeq_t s, kfsFileId_t f) :
		KfsOp(CMD_GETLAYOUT, s), fid(f)
	{
	}
	///
	void Request(std::ostringstream &os);

	///
	/// @brief 解析消息头
	/// @note 主要获取：状态信息，内容长度和chunk数目
	///
	void ParseResponseHeader(char *buf, int len);

	///
	/// @brief 解析命令中Content缓冲器中的数据
	/// @note 数据格式如下：\n
	/// @note fileOffset, chunkId, chunkVersion, numServers,\n
	/// @note n lines with (hostname port)
	///
	int ParseLayoutInfo();

	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "getlayout: fid=" << fid;
		return os.str();
	}
};

/// Get the chunk metadata (aka checksums) stored on the chunkservers
/// @brief 获取chunk的校验和，存放在content中
struct GetChunkMetadataOp: public KfsOp
{
	kfsChunkId_t chunkId;///
	///
	GetChunkMetadataOp(kfsSeq_t s, kfsChunkId_t c) :
		KfsOp(CMD_GET_CHUNK_METADATA, s), chunkId(c)
	{
	}
	///
	void Request(std::ostringstream &os);
	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "get chunk metadata: chunkId=" << chunkId;
		return os.str();
	}
};

/// @brief 为chunk分配存储
struct AllocateOp: public KfsOp
{
	kfsFileId_t fid;///
	off_t fileOffset;///
	kfsChunkId_t chunkId; /// result
	int64_t chunkVersion; /// result---version # for the chunk
	std::string clientHost; /// our hostname
	/// where is the chunk hosted name/port
	ServerLocation masterServer; /// master for running the write transaction
	std::vector<ServerLocation> chunkServers;///

	///
	AllocateOp(kfsSeq_t s, kfsFileId_t f) :
		KfsOp(CMD_ALLOCATE, s), fid(f), fileOffset(0)
	{

	}

	///
	void Request(std::ostringstream &os);

	///
	void ParseResponseHeader(char *buf, int len);

	///
	string Show() const
	{
		std::ostringstream os;

		os << "allocate: fid=" << fid << " offset: " << fileOffset;
		return os.str();
	}
};

/// @brief 进行截尾操作，截断从fileOffset开始的所有数据
struct TruncateOp: public KfsOp
{
	kfsFileId_t fid;///
	off_t fileOffset;///

	///
	TruncateOp(kfsSeq_t s, kfsFileId_t f, off_t o) :
		KfsOp(CMD_TRUNCATE, s), fid(f), fileOffset(o)
	{

	}

	///
	void Request(std::ostringstream &os);

	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "truncate: fid=" << fid << " offset: " << fileOffset;
		return os.str();
	}
};

/// @brief 打开文件
struct OpenOp: public KfsOp
{
	kfsChunkId_t chunkId;///
	int openFlags; /// either O_RDONLY, O_WRONLY or O_RDWR

	///
	OpenOp(kfsSeq_t s, kfsChunkId_t c) :
		KfsOp(CMD_OPEN, s), chunkId(c)
	{

	}

	///
	void Request(std::ostringstream &os);

	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "open: chunkid=" << chunkId;
		return os.str();
	}
};

/// @brief 关闭文件
struct CloseOp: public KfsOp
{
	kfsChunkId_t chunkId;///

	///
	CloseOp(kfsSeq_t s, kfsChunkId_t c) :
		KfsOp(CMD_CLOSE, s), chunkId(c)
	{

	}

	///
	void Request(std::ostringstream &os);

	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "close: chunkid=" << chunkId;
		return os.str();
	}
};

/// @brief used for retrieving a chunk's size
struct SizeOp: public KfsOp
{
	kfsChunkId_t chunkId;///
	int64_t chunkVersion;///
	off_t size; /// /* result */

	///
	SizeOp(kfsSeq_t s, kfsChunkId_t c, int64_t v) :
		KfsOp(CMD_SIZE, s), chunkId(c), chunkVersion(v)
	{

	}

	///
	void Request(std::ostringstream &os);

	///
	void ParseResponseHeader(char *buf, int len);

	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "size: chunkid=" << chunkId << " version=" << chunkVersion;
		return os.str();
	}
};

/// @brief 从服务器读入数据
struct ReadOp: public KfsOp
{
	kfsChunkId_t chunkId;///
	int64_t chunkVersion; /// /* input */
	off_t offset; /// /* input */
	size_t numBytes; /// /* input */

	///
	ReadOp(kfsSeq_t s, kfsChunkId_t c, int64_t v) :
		KfsOp(CMD_READ, s), chunkId(c), chunkVersion(v), offset(0), numBytes(0)
	{

	}

	///
	void Request(std::ostringstream &os);

	///
	void ParseResponseHeader(char *buf, int len);

	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "read: chunkid=" << chunkId << " version=" << chunkVersion;
		os << " offset=" << offset << " numBytes=" << numBytes;
		os << " checksum = " << checksum;
		return os.str();
	}
};

/// op that defines the write that is going to happen
/// @brief 向chunk服务器请求一个写数据的操作
struct WriteIdAllocOp: public KfsOp
{
	kfsChunkId_t chunkId;///
	int64_t chunkVersion;/// /* input */
	off_t offset;/// /* input */
	size_t numBytes;/// /* input */
	std::string writeIdStr;/// /* output */
	///
	std::vector<ServerLocation> chunkServerLoc;

	///
	WriteIdAllocOp(kfsSeq_t s, kfsChunkId_t c, int64_t v, off_t o, size_t n) :
		KfsOp(CMD_WRITE_ID_ALLOC, s), chunkId(c), chunkVersion(v), offset(o),
				numBytes(n)
	{

	}

	///
	void Request(std::ostringstream &os);

	///
	void ParseResponseHeader(char *buf, int len);

	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "write-id-alloc: chunkid=" << chunkId << " version="
				<< chunkVersion;
		return os.str();
	}
};

/// @brief 分配写序号
struct WriteInfo
{
	ServerLocation serverLoc;///
	int64_t writeId;///
	///
	WriteInfo() :
		writeId(-1)
	{
	}
	///
	WriteInfo(ServerLocation loc, int64_t w) :
		serverLoc(loc), writeId(w)
	{
	}
	///
	WriteInfo & operator =(const WriteInfo &other)
	{
		serverLoc = other.serverLoc;
		writeId = other.writeId;
		return *this;
	}
	///
	std::string Show() const
	{
		std::ostringstream os;

		os << " location= " << serverLoc.ToString() << " writeId=" << writeId;
		return os.str();
	}
};

/// @brief 显示写有关信息
class ShowWriteInfo
{
	std::ostringstream &os;///
public:
	///
	ShowWriteInfo(std::ostringstream &o) :
		os(o)
	{
	}
	///
	void operator()(WriteInfo w)
	{
		os << w.Show() << ' ';
	}
};

/// @brief 用于将数据写入到chunk服务器中
struct WritePrepareOp: public KfsOp
{
	kfsChunkId_t chunkId;///
	int64_t chunkVersion;/// /* input */
	/// chunk当中的offset，因为每次些数据，最多写一个完整的块
	off_t offset;/// /* input */
	size_t numBytes;/// /* input */
	std::vector<WriteInfo> writeInfo;/// /* input */
	///
	WritePrepareOp(kfsSeq_t s, kfsChunkId_t c, int64_t v,
			std::vector<WriteInfo> &w) :
		KfsOp(CMD_WRITE_PREPARE, s), chunkId(c), chunkVersion(v), offset(0),
				numBytes(0), writeInfo(w)
	{
	}

	///
	void Request(std::ostringstream &os);
	/// void ParseResponseHeader(char *buf, int len);
	std::string Show() const
	{
		std::ostringstream os;

		os << "write-prepare: chunkid=" << chunkId << " version="
				<< chunkVersion;
		os << " offset=" << offset << " numBytes=" << numBytes;
		for_each(writeInfo.begin(), writeInfo.end(), ShowWriteInfo(os));
		return os.str();
	}
};

/// @brief 进行同步
struct WriteSyncOp: public KfsOp
{
	kfsChunkId_t chunkId;///
	int64_t chunkVersion;///

	///
	std::vector<WriteInfo> writeInfo;

	///
	WriteSyncOp() :
		KfsOp(CMD_WRITE_SYNC, 0)
	{
	}

	///
	WriteSyncOp(kfsSeq_t s, kfsChunkId_t c, int64_t v,
			std::vector<WriteInfo> &w) :
		KfsOp(CMD_WRITE_SYNC, s), chunkId(c), chunkVersion(v), writeInfo(w)
	{
	}

	///
	void Init(kfsSeq_t s, kfsChunkId_t c, int64_t v, std::vector<WriteInfo> &w)
	{
		seq = s;
		chunkId = c;
		chunkVersion = v;
		writeInfo = w;
	}

	///
	void Request(std::ostringstream &os);

	///
	std::string Show() const
	{
		std::ostringstream os;

		os << "write-sync: chunkid=" << chunkId << " version=" << chunkVersion;
		std::for_each(writeInfo.begin(), writeInfo.end(), ShowWriteInfo(os));
		return os.str();
	}
};

/// @brief 用于获取指定chunk的lease的ID
struct LeaseAcquireOp: public KfsOp
{
	kfsChunkId_t chunkId; /// input
	int64_t leaseId; /// output

	///
	LeaseAcquireOp(kfsSeq_t s, kfsChunkId_t c) :
		KfsOp(CMD_LEASE_ACQUIRE, s), chunkId(c), leaseId(-1)
	{

	}

	///
	void Request(std::ostringstream &os);

	///
	void ParseResponseHeader(char *buf, int len);

	///
	std::string Show() const
	{
		std::ostringstream os;
		os << "lease-acquire: chunkid=" << chunkId;
		return os.str();
	}
};

/// @brief 用于更新指定的lease
struct LeaseRenewOp: public KfsOp
{
	kfsChunkId_t chunkId; /// input
	int64_t leaseId; /// input

	///
	LeaseRenewOp(kfsSeq_t s, kfsChunkId_t c, int64_t l) :
		KfsOp(CMD_LEASE_RENEW, s), chunkId(c), leaseId(l)
	{

	}

	///
	void Request(std::ostringstream &os);
	// default parsing of status is sufficient

	///
	std::string Show() const
	{
		std::ostringstream os;
		os << "lease-renew: chunkid=" << chunkId << " leaseId=" << leaseId;
		return os.str();
	}
};

/// @brief 设置文件的副本个数，numReplicas即是输入也是输出
struct ChangeFileReplicationOp: public KfsOp
{
	kfsFileId_t fid; /// input
	int16_t numReplicas; /// desired replication

	///
	ChangeFileReplicationOp(kfsSeq_t s, kfsFileId_t f, int16_t r) :
		KfsOp(CMD_CHANGE_FILE_REPLICATION, s), fid(f), numReplicas(r)
	{

	}

	///
	void Request(std::ostringstream &os);

	///
	void ParseResponseHeader(char *buf, int len);

	///
	std::string Show() const
	{
		std::ostringstream os;
		os << "change-file-replication: fid=" << fid << " # of replicas: "
				<< numReplicas;
		return os.str();
	}
};

}

#endif // _LIBKFSCLIENT_KFSOPS_H
