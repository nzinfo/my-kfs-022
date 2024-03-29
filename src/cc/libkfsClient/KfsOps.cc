//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: KfsOps.cc 132 2008-08-21 06:18:26Z sriramsrao $
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

#include "KfsOps.h"
#include <cassert>

extern "C"
{
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
}
#include "libkfsIO/Checksum.h"
#include "Utils.h"

using std::istringstream;
using std::ostringstream;
using std::string;

#include <iostream>
using std::cout;
using std::endl;

static const char *KFS_VERSION_STR = "KFS/1.0";

using namespace KFS;

///
/// All Request() methods build a request RPC based on the KFS
/// protocol and output the request into a ostringstream.
/// @param[out] os which contains the request RPC.
///
void CreateOp::Request(ostringstream &os)
{
	int e = exclusive ? 1 : 0;

	os << "CREATE " << "\r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Parent File-handle: " << parentFid << "\r\n";
	os << "Filename: " << filename << "\r\n";
	os << "Num-replicas: " << numReplicas << "\r\n";
	os << "Exclusive: " << e << "\r\n\r\n";
}

void MkdirOp::Request(ostringstream &os)
{
	os << "MKDIR " << "\r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Parent File-handle: " << parentFid << "\r\n";
	os << "Directory: " << dirname << "\r\n\r\n";
}

void RmdirOp::Request(ostringstream &os)
{
	os << "RMDIR " << "\r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Parent File-handle: " << parentFid << "\r\n";
	os << "Directory: " << dirname << "\r\n\r\n";
}

void RenameOp::Request(ostringstream &os)
{
	int o = overwrite ? 1 : 0;

	os << "RENAME " << "\r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Parent File-handle: " << parentFid << "\r\n";
	os << "Old-name: " << oldname << "\r\n";
	os << "New-path: " << newpath << "\r\n";
	os << "Overwrite: " << o << "\r\n\r\n";
}

void ReaddirOp::Request(ostringstream &os)
{
	os << "READDIR " << "\r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Directory File-handle: " << fid << "\r\n\r\n";
}

void ReaddirPlusOp::Request(ostringstream &os)
{
	os << "READDIRPLUS " << "\r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Directory File-handle: " << fid << "\r\n\r\n";
}

void RemoveOp::Request(ostringstream &os)
{
	os << "REMOVE " << "\r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Parent File-handle: " << parentFid << "\r\n";
	os << "Filename: " << filename << "\r\n\r\n";
}

///
/// @brief 发送查询命令（RPC），包括：命令，序号，版本号，父句柄，文件名
///
void LookupOp::Request(ostringstream &os)
{
	os << "LOOKUP \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Parent File-handle: " << parentFid << "\r\n";
	os << "Filename: " << filename << "\r\n\r\n";
}

void LookupPathOp::Request(ostringstream &os)
{
	os << "LOOKUP_PATH \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Root File-handle: " << rootFid << "\r\n";
	os << "Pathname: " << filename << "\r\n\r\n";
}

void GetAllocOp::Request(ostringstream &os)
{
	assert(fileOffset >= 0);

	os << "GETALLOC \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "File-handle: " << fid << "\r\n";
	os << "Chunk-offset: " << fileOffset << "\r\n\r\n";
}

void GetLayoutOp::Request(ostringstream &os)
{
	os << "GETLAYOUT \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "File-handle: " << fid << "\r\n\r\n";
}

void GetChunkMetadataOp::Request(ostringstream &os)
{
	os << "GET_CHUNK_METADATA \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Chunk-handle: " << chunkId << "\r\n\r\n";
}

void AllocateOp::Request(ostringstream &os)
{
	static const int MAXHOSTNAMELEN = 256;
	char hostname[MAXHOSTNAMELEN];

	gethostname(hostname, MAXHOSTNAMELEN);

	os << "ALLOCATE \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Client-host: " << hostname << "\r\n";
	os << "File-handle: " << fid << "\r\n";
	os << "Chunk-offset: " << fileOffset << "\r\n\r\n";
}

void TruncateOp::Request(ostringstream &os)
{
	os << "TRUNCATE \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "File-handle: " << fid << "\r\n";
	os << "Offset: " << fileOffset << "\r\n\r\n";
}

void OpenOp::Request(ostringstream &os)
{
	const char *modeStr;

	os << "OPEN \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Chunk-handle: " << chunkId << "\r\n";
	if (openFlags == O_RDONLY)
		modeStr = "READ";
	else
	{
		assert(openFlags == O_WRONLY || openFlags == O_RDWR);
		modeStr = "WRITE";
	}

	os << "Intent: " << modeStr << "\r\n\r\n";
}

void CloseOp::Request(ostringstream &os)
{
	os << "CLOSE \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Chunk-handle: " << chunkId << "\r\n\r\n";
}

void ReadOp::Request(ostringstream &os)
{
	os << "READ \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Chunk-handle: " << chunkId << "\r\n";
	os << "Chunk-version: " << chunkVersion << "\r\n";
	os << "Offset: " << offset << "\r\n";
	os << "Num-bytes: " << numBytes << "\r\n\r\n";
}

void WriteIdAllocOp::Request(ostringstream &os)
{
	os << "WRITE_ID_ALLOC \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Chunk-handle: " << chunkId << "\r\n";
	os << "Chunk-version: " << chunkVersion << "\r\n";
	os << "Offset: " << offset << "\r\n";
	os << "Num-bytes: " << numBytes << "\r\n";
	if (chunkServerLoc.size() > 1)
	{
		os << "Num-servers: " << chunkServerLoc.size() << "\r\n";
		os << "Servers:";
		for (vector<ServerLocation>::size_type i = 0; i < chunkServerLoc.size(); ++i)
		{
			os << chunkServerLoc[i].ToString().c_str() << ' ';
		}
		os << "\r\n";
	}
	os << "\r\n";
}

void WritePrepareOp::Request(ostringstream &os)
{
	os << "WRITE_PREPARE \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Chunk-handle: " << chunkId << "\r\n";
	os << "Chunk-version: " << chunkVersion << "\r\n";
	os << "Offset: " << offset << "\r\n";
	os << "Num-bytes: " << numBytes << "\r\n";
	os << "Checksum: " << checksum << "\r\n";
	os << "Num-servers: " << writeInfo.size() << "\r\n";
	os << "Servers:";
	for (vector<WriteInfo>::size_type i = 0; i < writeInfo.size(); ++i)
	{
		os << writeInfo[i].serverLoc.ToString().c_str();
		os << ' ' << writeInfo[i].writeId << ' ';
	}
	os << "\r\n\r\n";
}

void WriteSyncOp::Request(ostringstream &os)
{
	os << "WRITE_SYNC \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Chunk-handle: " << chunkId << "\r\n";
	os << "Chunk-version: " << chunkVersion << "\r\n";
	os << "Num-servers: " << writeInfo.size() << "\r\n";
	os << "Servers:";
	for (vector<WriteInfo>::size_type i = 0; i < writeInfo.size(); ++i)
	{
		os << writeInfo[i].serverLoc.ToString().c_str();
		os << ' ' << writeInfo[i].writeId << ' ';
	}
	os << "\r\n\r\n";
}

void SizeOp::Request(ostringstream &os)
{
	os << "SIZE \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Chunk-handle: " << chunkId << "\r\n";
	os << "Chunk-version: " << chunkVersion << "\r\n\r\n";
}

void LeaseAcquireOp::Request(ostringstream &os)
{
	os << "LEASE_ACQUIRE \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Chunk-handle: " << chunkId << "\r\n\r\n";
}

void LeaseRenewOp::Request(ostringstream &os)
{
	os << "LEASE_RENEW \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "Chunk-handle: " << chunkId << "\r\n";
	os << "Lease-id: " << leaseId << "\r\n";
	os << "Lease-type: READ_LEASE" << "\r\n\r\n";
}

void ChangeFileReplicationOp::Request(ostringstream &os)
{
	os << "CHANGE_FILE_REPLICATION \r\n";
	os << "Cseq: " << seq << "\r\n";
	os << "Version: " << KFS_VERSION_STR << "\r\n";
	os << "File-handle: " << fid << "\r\n";
	os << "Num-replicas: " << numReplicas << "\r\n\r\n";
}

///
/// @brief 解析响应的头，并且把状态和内容长度存入全局量中，其他的放在prop中
/// @param[in] resp 要解析的字符串
/// @param[out] prop 解析的结果
///
void KfsOp::ParseResponseHeaderCommon(string &resp, Properties &prop)
{
	istringstream ist(resp);
	kfsSeq_t resSeq;
	const char separator = ':';

	// 属性文件按行隔开，每一行包含type:value
	prop.loadProperties(ist, separator, false);
	resSeq = prop.getValue("Cseq", (kfsSeq_t) -1);	// 这步做什么？
	status = prop.getValue("Status", -1);
	contentLength = prop.getValue("Content-length", 0);
}

///
/// @brief 解析回复
/// @param[in] buf: buffer containing the response
/// @param[in] len: str-len of the buffer.
/// @note 目的是将操作中的状态和内容长度取出并且存放，其他信息直接丢弃
///
void KfsOp::ParseResponseHeader(char *buf, int len)
{
	string resp(buf, len);
	Properties prop;

	// XXX: Need to extract out the status: OK or what?
	ParseResponseHeaderCommon(resp, prop);
}

///
/// Specific response parsing handlers.
///
void CreateOp::ParseResponseHeader(char *buf, int len)
{
	string resp(buf, len);
	Properties prop;

	ParseResponseHeaderCommon(resp, prop);
	fileId = prop.getValue("File-handle", (kfsFileId_t) -1);
}

void ReaddirOp::ParseResponseHeader(char *buf, int len)
{
	string resp(buf, len);
	Properties prop;

	ParseResponseHeaderCommon(resp, prop);
	numEntries = prop.getValue("Num-Entries", 0);
}

void ReaddirPlusOp::ParseResponseHeader(char *buf, int len)
{
	string resp(buf, len);
	Properties prop;

	ParseResponseHeaderCommon(resp, prop);
	numEntries = prop.getValue("Num-Entries", 0);
}

// 解析信息header部分，返回状态，content信息大小和fileId
void MkdirOp::ParseResponseHeader(char *buf, int len)
{
	string resp(buf, len);
	Properties prop;

	ParseResponseHeaderCommon(resp, prop);
	fileId = prop.getValue("File-handle", (kfsFileId_t) -1);
}

void LookupOp::ParseResponseHeader(char *buf, int len)
{
	string resp(buf, len);
	Properties prop;
	string s;

	// 将字符串转换为Properity格式
	ParseResponseHeaderCommon(resp, prop);
	fattr.fileId = prop.getValue("File-handle", (kfsFileId_t) -1);
	s = prop.getValue("Type", "");
	fattr.isDirectory = (s == "dir");
	fattr.chunkCount = prop.getValue("Chunk-count", 0);
	fattr.fileSize = prop.getValue("File-size", (off_t) -1);
	fattr.numReplicas = prop.getValue("Replication", 1);

	// 修改时间
	s = prop.getValue("M-Time", "");
	GetTimeval(s, fattr.mtime);

	// 属性修改时间
	s = prop.getValue("C-Time", "");
	GetTimeval(s, fattr.ctime);

	// 创建时间
	s = prop.getValue("CR-Time", "");
	GetTimeval(s, fattr.crtime);
}

void LookupPathOp::ParseResponseHeader(char *buf, int len)
{
	string resp(buf, len);
	Properties prop;
	string s;
	istringstream ist;

	ParseResponseHeaderCommon(resp, prop);

	fattr.fileId = prop.getValue("File-handle", (kfsFileId_t) -1);
	s = prop.getValue("Type", "");
	fattr.isDirectory = (s == "dir");
	fattr.fileSize = prop.getValue("File-size", (off_t) -1);
	fattr.chunkCount = prop.getValue("Chunk-count", 0);
	fattr.numReplicas = prop.getValue("Replication", 1);

	s = prop.getValue("M-Time", "");
	GetTimeval(s, fattr.mtime);

	s = prop.getValue("C-Time", "");
	GetTimeval(s, fattr.ctime);

	s = prop.getValue("CR-Time", "");
	GetTimeval(s, fattr.crtime);
}

void AllocateOp::ParseResponseHeader(char *buf, int len)
{
	string resp(buf, len);
	Properties prop;

	ParseResponseHeaderCommon(resp, prop);
	chunkId = prop.getValue("Chunk-handle", (kfsFileId_t) -1);
	chunkVersion = prop.getValue("Chunk-version", (int64_t) -1);

	string master = prop.getValue("Master", "");
	if (master != "")
	{
		istringstream ist(master);

		ist >> masterServer.hostname;
		ist >> masterServer.port;
		// put the master the first in the list
		chunkServers.push_back(masterServer);
	}

	int numReplicas = prop.getValue("Num-replicas", 0);
	string replicas = prop.getValue("Replicas", "");

	if (replicas != "")
	{
		istringstream ser(replicas);
		ServerLocation loc;

		for (int i = 0; i < numReplicas; ++i)
		{
			ser >> loc.hostname;
			ser >> loc.port;
			if (loc != masterServer)
				chunkServers.push_back(loc);
		}
	}
}

void GetAllocOp::ParseResponseHeader(char *buf, int len)
{
	string resp(buf, len);
	Properties prop;

	ParseResponseHeaderCommon(resp, prop);
	chunkId = prop.getValue("Chunk-handle", (kfsFileId_t) -1);
	chunkVersion = prop.getValue("Chunk-version", (int64_t) -1);

	int numReplicas = prop.getValue("Num-replicas", 0);
	string replicas = prop.getValue("Replicas", "");
	if (replicas != "")
	{
		istringstream ser(replicas);
		ServerLocation loc;

		for (int i = 0; i < numReplicas; ++i)
		{
			ser >> loc.hostname;
			ser >> loc.port;
			chunkServers.push_back(loc);
		}
	}
}

///
/// @brief 解析消息头
/// @note 主要获取：状态信息，内容长度和chunk数目
///
void GetLayoutOp::ParseResponseHeader(char *buf, int len)
{
	string resp(buf, len);
	Properties prop;

	ParseResponseHeaderCommon(resp, prop);
	numChunks = prop.getValue("Num-chunks", 0);
}

///
/// @brief 解析命令中Content缓冲器中的数据
/// @note 数据格式如下：\n
/// @note fileOffset, chunkId, chunkVersion, numServers,\n
/// @note n lines with (hostname port)
///
int GetLayoutOp::ParseLayoutInfo()
{
	if (numChunks == 0 || contentBuf == NULL)
		return 0;

	istringstream ist(contentBuf);
	for (int i = 0; i < numChunks; ++i)
	{
		ChunkLayoutInfo l;
		ServerLocation s;
		int numServers;

		ist >> l.fileOffset;
		ist >> l.chunkId;
		ist >> l.chunkVersion;
		ist >> numServers;
		for (int j = 0; j < numServers; j++)
		{
			ist >> s.hostname;
			ist >> s.port;
			l.chunkServers.push_back(s);
		}
		chunks.push_back(l);
	}
	return 0;
}

void SizeOp::ParseResponseHeader(char *buf, int len)
{
	string resp(buf, len);
	Properties prop;

	ParseResponseHeaderCommon(resp, prop);
	size = prop.getValue("Size", (long long) 0);
}

void ReadOp::ParseResponseHeader(char *buf, int len)
{
	string resp(buf, len);
	Properties prop;

	ParseResponseHeaderCommon(resp, prop);
	checksum = prop.getValue("Checksum", 0);
}

void WriteIdAllocOp::ParseResponseHeader(char *buf, int len)
{
	string resp(buf, len);
	Properties prop;

	ParseResponseHeaderCommon(resp, prop);
	writeIdStr = prop.getValue("Write-id", "");
}

void LeaseAcquireOp::ParseResponseHeader(char *buf, int len)
{
	string resp(buf, len);
	Properties prop;

	ParseResponseHeaderCommon(resp, prop);
	leaseId = prop.getValue("Lease-id", (long long) -1);
}

void ChangeFileReplicationOp::ParseResponseHeader(char *buf, int len)
{
	string resp(buf, len);
	Properties prop;

	ParseResponseHeaderCommon(resp, prop);
	numReplicas = prop.getValue("Num-replicas", 1);
}

