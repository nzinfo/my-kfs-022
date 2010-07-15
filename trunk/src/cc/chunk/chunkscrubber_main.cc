//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: chunkscrubber_main.cc 140 2008-08-26 04:12:10Z sriramsrao $
//
// Created 2008/06/11
//
// Author: Sriram Rao
//
// Copyright 2008 Quantcast Corp.
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
// \brief A tool that scrubs the chunks in a directory and validates checksums.
//
//----------------------------------------------------------------------------

#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <boost/scoped_array.hpp>
#include <iostream>

#include "libkfsIO/Checksum.h"
#include "libkfsIO/Globals.h"
#include "common/log.h"
#include "libkfsIO/FileHandle.h"
#include "Chunk.h"
#include "ChunkManager.h"

using std::cout;
using std::endl;
using std::string;
using boost::scoped_array;

using namespace KFS;

static void scrubFile(string &fn, bool verbose);

/// 验证本地chunkserver中的所有chunk数据是否正确，如果校验有错误，输出错误信息
int main(int argc, char **argv)
{
	char optchar;
	bool help = false;

	// chunk目录
	const char *chunkDir = NULL;
	int res, count;
	// 目录
	struct dirent **entries;
	struct stat statBuf;
	// 冗余
	bool verbose = false;

	/// 设置Logger，设置优先级为INFO
	KFS::MsgLogger::Init(NULL);
	KFS::MsgLogger::SetLevel(log4cpp::Priority::INFO);

	// 根据命令行参数设置全局变量
	/*
	 * getopt()自动解析命令行中类似-v -l -s -h的命令，"hvd:"指定参数，具有参数
	 * 的选项后面跟有一个 : 字符
	 */
	while ((optchar = getopt(argc, argv, "hvd:")) != -1)
	{
		switch (optchar)
		{
		case 'd':
			// 参数的具体内容设置在全局变量optarg中
			chunkDir = optarg;
			break;
		case 'v':
			verbose = true;
			break;
		case 'h':
		default:
			help = true;
			break;
		}
	}

	// 如果命令中请求帮助或者未指定chunkDir，则显示帮助信息
	if ((help) || (chunkDir == NULL))
	{
		cout << "Usage: " << argv[0] << " -d <chunkdir> {-v}" << endl;
		exit(-1);
	}

	// 获取chunkDir中所有的内容，存放到entries中，按字母序排序，返回其中的项数
	res = scandir(chunkDir, &entries, 0, alphasort);
	if (res < 0)
	{
		cout << "Unable to open: " << chunkDir << endl;
		exit(-1);
	}

	count = res;

	cout << "Start checking chunk files.." << endl;

	for (int i = 0; i < count; i++)
	{
		string fn = chunkDir;
		fn = fn + "/" + entries[i]->d_name;

		std::cout << "Checking: " << fn << std::endl;
		// 获取文件属性
		res = stat(fn.c_str(), &statBuf);
		free(entries[i]);

		/*
		 * S_ISLNK(st_mode):是否是一个连接.
		 * S_ISREG是否是一个常规文件.
		 * S_ISDIR是否是一个目录S_ISCHR是否是一个字符设备.
		 * S_ISBLK是否是一个块设备S_ISFIFO是否是一个FIFO文件.
		 * S_ISSOCK是否是一个SOCKET文件.
		 */
		// 如果获取属性失败或者指定文件不是常规文件
		if ((res < 0) || (!S_ISREG(statBuf.st_mode)))
			continue;
		// 对每一个文件进行校验

		cout << "Start checking..." << endl << endl;
		scrubFile(fn, verbose);
	}
	free(entries);
	exit(0);
}

/// 校验fn中的数据是否正确（用校验和进行校验）
static void scrubFile(string &fn, bool verbose)
{
	ChunkInfo_t chunkInfo;
	int fd, res;
	FileHandlePtr f;
	scoped_array<char> data;

	// 打开文件
	fd = open(fn.c_str(), O_RDONLY);
	if (fd < 0)
	{
		cout << "Unable to open: " << fn << endl;
		return;
	}
	f.reset(new FileHandle_t(fd));

	// 从Linux文件系统中读入数据校验和
	cout << "Read in serialized chunk data." << endl << endl;
	res = chunkInfo.Deserialize(f->mFd, true);
	if (res < 0)
	{
		cout << "Deserialize of chunkinfo failed for: " << fn << endl;
		return;
	}
	if (verbose)
	{
		cout << "fid: " << chunkInfo.fileId << endl;
		cout << "chunkId: " << chunkInfo.chunkId << endl;
		cout << "size: " << chunkInfo.chunkSize << endl;
		cout << "version: " << chunkInfo.chunkVersion << endl;
	}
	data.reset(new char[KFS::CHUNKSIZE]);

	// 从文件f中的KFS_CHUNK_HEADER_SIZE开始读入KFS::CHUNKSIZE字节的数据到data中
	res = pread(f->mFd, data.get(), KFS::CHUNKSIZE, KFS_CHUNK_HEADER_SIZE);
	if (res < 0)
	{
		cout << "Unable to read data for : " << fn << endl;
		return;
	}

	// 将data中未填充数据的部分用0填充（因为文件中的数据不够一个KFS::CHUNKSIZE大小）
	if ((size_t) res != KFS::CHUNKSIZE)
	{
		long size = chunkInfo.chunkSize;
		if (res != chunkInfo.chunkSize)
		{
			cout << "Size mismatch: chunk header says: " << size
					<< " but file size is: " << res << endl;
		}

		memset(data.get() + size, 0, KFS::CHUNKSIZE - size);
	}

	// go thru block by block and verify checksum
	// 进行校验
	for (int i = 0; i < res; i += CHECKSUM_BLOCKSIZE)
	{
		char *startPt = data.get() + i;
		uint32_t cksum = ComputeBlockChecksum(startPt, CHECKSUM_BLOCKSIZE);

		// output file content.
		cout << "------------ Chunk Content ------------"<< endl;
		for(unsigned j = 0;j<CHECKSUM_BLOCKSIZE;j++)
		{
			printf("%c", *(startPt + j));
		}
		cout << endl << "------------ Chunk Content ------------"<< endl << endl;

		// uint32_t cksum = ComputeBlockChecksum(startPt, res);
		uint32_t blkno = OffsetToChecksumBlockNum(i);
		printf("Blkno: %0x\n", blkno);
		printf("cksum: %0x\n", cksum);

		if (cksum != chunkInfo.chunkBlockChecksum[blkno])
		{
			cout << "fn = " << fn << " : Checksum mismatch for block ("
					<< blkno << "):" << " Computed: " << cksum
					<< " from chunk: " << chunkInfo.chunkBlockChecksum[blkno]
					<< endl;
		}
		else
		{
			std::cout << "Checksum OK for block: " << i << std::endl;
		}
	}
}
