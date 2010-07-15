//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: Checksum.h 71 2008-07-07 15:49:14Z sriramsrao $
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
// Code for computing 32-bit Adler checksums
//----------------------------------------------------------------------------

#ifndef CHUNKSERVER_CHECKSUM_H
#define CHUNKSERVER_CHECKSUM_H

#include <vector>
#include "libkfsIO/IOBuffer.h"

namespace KFS
{
/// Checksums are computed on 64KB block boundaries.  We use the
/// "rolling" 32-bit Adler checksum algorithm
/// 按块进行校验，校验块的大小是64KB
const uint32_t CHECKSUM_BLOCKSIZE = 65536;

const uint32_t MOD_ADLER = 65521;

/// 计算出当前偏移属于第几个校验块
extern uint32_t OffsetToChecksumBlockNum(off_t offset);

/// offset所在的校验块的起始地址
extern uint32_t OffsetToChecksumBlockStart(off_t offset);

/// offset所在的校验块的结束地址（即下一个校验块的起始地址）
extern uint32_t OffsetToChecksumBlockEnd(off_t offset);

/// 从IOBuffer中的每一个数据缓存块中截取len长度的数据进行校验
/// （可用数据长度不足len的，取可用数据的长度）
extern uint32_t ComputeBlockChecksum(IOBuffer *data, size_t len);

/// 计算buf中数据的校验和，数据的长度为len个字节
extern uint32_t ComputeBlockChecksum(const char *data, size_t len);

/// 将IOBuffer中的所有数据拆分成等大的数据块（CHECKSUM_BLOCKSIZE个字节），逐个进行校验，
/// 并返回校验和数组
extern std::vector<uint32_t> ComputeChecksums(IOBuffer *data, size_t len);

}

#endif // CHUNKSERVER_CHECKSUM_H
