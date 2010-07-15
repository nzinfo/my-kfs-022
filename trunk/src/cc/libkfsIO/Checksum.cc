//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: Checksum.cc 71 2008-07-07 15:49:14Z sriramsrao $
//
// Created 2006/09/12
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
// An adaptation of the 32-bit Adler checksum algorithm
//
//----------------------------------------------------------------------------

#include "Checksum.h"

#include <algorithm>
#include <vector>
#include <zlib.h>

using std::min;
using std::vector;
using std::list;

using namespace KFS;

/*
 * 按块进行校验，每一个校验块的大小是64KB
 */

/// 计算出当前偏移是第几个校验块
uint32_t KFS::OffsetToChecksumBlockNum(off_t offset)
{
	return offset / CHECKSUM_BLOCKSIZE;
}

/// offset所在的校验和的起始地址
uint32_t KFS::OffsetToChecksumBlockStart(off_t offset)
{
	return (offset / CHECKSUM_BLOCKSIZE) * CHECKSUM_BLOCKSIZE;
}

/// offset所在的校验块的结束地址（即下一个校验块的起始地址）
uint32_t KFS::OffsetToChecksumBlockEnd(off_t offset)
{
	return ((offset / CHECKSUM_BLOCKSIZE) + 1) * CHECKSUM_BLOCKSIZE;
}

/// 计算buf中数据的校验和，数据的长度为len个字节
uint32_t KFS::ComputeBlockChecksum(const char *buf, size_t len)
{
	uint32_t res = adler32(0L, Z_NULL, 0);

	res = adler32(res, (const Bytef *) buf, len);
	return res;
}

/// 从IOBuffer中的每一个数据缓存块中截取len长度的数据进行校验
/// （可用数据长度不足len的，取可用数据的长度）
uint32_t KFS::ComputeBlockChecksum(IOBuffer *data, size_t len)
{
	// 初始化，长度为0的数据校验和
	uint32_t res = adler32(0L, Z_NULL, 0);

	for (list<IOBufferDataPtr>::iterator iter = data->mBuf.begin(); len > 0
			&& (iter != data->mBuf.end()); ++iter)
	{
		IOBufferDataPtr blk = *iter;
		size_t tlen = min((size_t) blk->BytesConsumable(), len);

		if (tlen == 0)
			continue;

		res = adler32(res, (const Bytef *) blk->Consumer(), tlen);
		len -= tlen;
	}
	return res;
}

/// 将IOBuffer中的所有数据拆分成等大的数据块（CHECKSUM_BLOCKSIZE个字节），逐个进行校验，
/// 并返回校验和数组
vector<uint32_t> KFS::ComputeChecksums(IOBuffer *data, size_t len)
{
	vector<uint32_t> cksums;
	list<IOBufferDataPtr>::iterator iter = data->mBuf.begin();

	assert(len >= CHECKSUM_BLOCKSIZE);

	if (iter == data->mBuf.end())
		return cksums;

	IOBufferDataPtr blk = *iter;
	char *buf = blk->Consumer();

	/// Compute checksum block by block
	while ((len > 0) && (iter != data->mBuf.end()))
	{
		size_t currLen = 0;
		uint32_t res = adler32(0L, Z_NULL, 0);

		// 将数据块拆分成CHECKSUM_BLOCKSIZE长度的数据块进行校验和
		while (currLen < CHECKSUM_BLOCKSIZE)
		{
			unsigned navail = min((size_t)(blk->Producer() - buf), len);
			if (currLen + navail > CHECKSUM_BLOCKSIZE)
				navail = CHECKSUM_BLOCKSIZE - currLen;

			if (navail == 0)
			{
				iter++;
				if (iter == data->mBuf.end())
					break;
				blk = *iter;
				buf = blk->Consumer();
				continue;
			}

			currLen += navail;
			len -= navail;
			res = adler32(res, (const Bytef *) buf, navail);
			buf += navail;
		}
		cksums.push_back(res);
	}
	return cksums;
}
