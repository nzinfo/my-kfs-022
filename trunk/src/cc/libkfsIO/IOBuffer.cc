//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: IOBuffer.cc 92 2008-07-21 21:20:48Z sriramsrao $
//
// Created 2006/03/15
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

#include <cerrno>
#include <unistd.h>

#include <iostream>
#include <algorithm>

#include "IOBuffer.h"
#include "Globals.h"

using std::min;
using std::list;

using namespace KFS;
using namespace KFS::libkfsio;

// To conserve memory, by default, we allocate IOBufferData in 4K
// blocks.  However, applications are free to change this default unit
// to what they like.
uint32_t IOBUFSIZE = 4096;

// Call this function if you want to change the default allocation unit.
void libkfsio::SetIOBufferSize(uint32_t bufsz)
{
	IOBUFSIZE = bufsz;
}

IOBufferData::IOBufferData(uint32_t bufsz)
{
	// cout << "Allocating: " << this << endl;
	Init(bufsz);
}

IOBufferData::IOBufferData()
{
	// cout << "Allocating: " << this << endl;
	Init(IOBUFSIZE);
}

// setup a new IOBufferData for read access by block sharing.
IOBufferData::IOBufferData(IOBufferDataPtr &other, char *s, char *e) :
	mData(other->mData), mStart(s), mEnd(e), mProducer(e), mConsumer(s)
{
}

void IOBufferData::Init(uint32_t bufsz)
{
	// reset是指shared_array这个数组变量的初始化
	mData.reset(new char[bufsz]);
	// get这个函数的作用是返回数组中变量的首地址，这个便是char *
	mStart = mData.get();
	mEnd = mStart + bufsz;
	mProducer = mConsumer = mStart;
}

IOBufferData::~IOBufferData()
{
	// cout << "Deleting: " << this << endl;

	mData.reset();
	mProducer = mConsumer = NULL;
}

// 填充0，从producer指针开始。不移动任何指针
int IOBufferData::ZeroFill(int nbytes)
{
	int fillAvail = mEnd - mProducer;

	if (fillAvail < nbytes)
		nbytes = fillAvail;

	memset(mProducer, '\0', nbytes);
	return Fill(nbytes);
}

// 向后移动生产者指针
int IOBufferData::Fill(int nbytes)
{
	int fillAvail = mEnd - mProducer;

	if (nbytes > fillAvail)
	{
		mProducer = mEnd;
		return fillAvail;
	}
	mProducer += nbytes;
	assert(mProducer <= mEnd);
	return nbytes;
}

int IOBufferData::Consume(int nbytes)
{
	int consumeAvail = mProducer - mConsumer;

	if (nbytes > consumeAvail)
	{
		mConsumer = mProducer;
		return consumeAvail;
	}
	mConsumer += nbytes;
	assert(mConsumer <= mProducer);
	return nbytes;
}

int IOBufferData::Trim(int nbytes)
{
	int bytesAvail = mProducer - mConsumer;

	// you can't trim and grow the data in the buffer
	if (bytesAvail < nbytes)
		return bytesAvail;

	mProducer = mConsumer + nbytes;
	return nbytes;
}

// 读fd文件数据到缓冲器中
int IOBufferData::Read(int fd)
{
	int numBytes = mEnd - mProducer;
	int nread;

	assert(numBytes> 0);

	if (numBytes <= 0)
		return -1;

	// 从文件fd中读入numBytes字节到mProducer中
	nread = read(fd, mProducer, numBytes);

	if (nread > 0)
	{
		mProducer += nread;
		globals().ctrNetBytesRead.Update(nread);
	}

	return nread;
}

// 从缓冲器中写数据到磁盘，写完之后所写的数据将不再可用
int IOBufferData::Write(int fd)
{
	int numBytes = mProducer - mConsumer;
	int nwrote;

	assert(numBytes> 0);

	if (numBytes <= 0)
	{
		return -1;
	}

	nwrote = write(fd, mConsumer, numBytes);

	if (nwrote > 0)
	{
		mConsumer += nwrote;
		globals().ctrNetBytesWritten.Update(nwrote);
	}

	return nwrote;
}

// 将buffer中的数据拷贝到本类中，并相应移动producer指针
int IOBufferData::CopyIn(const char *buf, int numBytes)
{
	int bytesToCopy = mEnd - mProducer;

	if (bytesToCopy < numBytes)
	{
		memcpy(mProducer, buf, bytesToCopy);
		// 向后移动producer指针.
		Fill(bytesToCopy);
		return bytesToCopy;
	}
	else
	{
		memcpy(mProducer, buf, numBytes);
		// 向后移动producer指针.
		Fill(numBytes);
		return numBytes;
	}
}

// 从其他buffer中拷贝数据（数据的起始位置为consumer），相应改变本类中producer指针，
// 并且不应该影响被拷贝的数据buffer
int IOBufferData::CopyIn(const IOBufferData *other, int numBytes)
{
	int bytesToCopy = mEnd - mProducer;

	if (bytesToCopy < numBytes)
	{
		memcpy(mProducer, other->mConsumer, bytesToCopy);
		Fill(bytesToCopy);
		return bytesToCopy;
	}
	else
	{
		memcpy(mProducer, other->mConsumer, numBytes);
		Fill(numBytes);
		return numBytes;
	}
}

int IOBufferData::CopyOut(char *buf, int numBytes)
{
	int bytesToCopy = mProducer - mConsumer;

	assert(bytesToCopy >= 0);

	if (bytesToCopy <= 0)
	{
		return 0;
	}

	if (bytesToCopy > numBytes)
		bytesToCopy = numBytes;

	memcpy(buf, mConsumer, bytesToCopy);
	return bytesToCopy;
}

IOBuffer::IOBuffer()
{
}

IOBuffer::~IOBuffer()
{

}

void IOBuffer::Append(IOBufferDataPtr &buf)
{
	mBuf.push_back(buf);
}

void IOBuffer::Append(IOBuffer *ioBuf)
{
	list<IOBufferDataPtr>::iterator iter;
	IOBufferDataPtr data;

	for (iter = ioBuf->mBuf.begin(); iter != ioBuf->mBuf.end(); iter++)
	{
		data = *iter;
		mBuf.push_back(data);
	}
	ioBuf->mBuf.clear();
}

// 将other中的前numBytes数据剪切到本类中，并且对相应指针作适当移动
void IOBuffer::Move(IOBuffer *other, int numBytes)
{
	list<IOBufferDataPtr>::iterator iter;
	IOBufferDataPtr data, dataCopy;
	int bytesMoved = 0;

	assert(other->BytesConsumable() >= numBytes);

	iter = other->mBuf.begin();
	while ((iter != other->mBuf.end()) && (bytesMoved < numBytes))
	{
		data = *iter;
		if (data->BytesConsumable() + bytesMoved < numBytes)
		{
			other->mBuf.pop_front();
			bytesMoved += data->BytesConsumable();
			mBuf.push_back(data);
		}
		else
		{	// 拷贝不足一个BufferData的数据
			// this is the last buffer being moved; only partial data
			// from the buffer needs to be moved.  do the move by
			// sharing the block (and therby avoid data copy)
			int bytesToMove = numBytes - bytesMoved;
			dataCopy.reset(new IOBufferData(data, data->Consumer(),
					data->Consumer() + bytesToMove));
			mBuf.push_back(dataCopy);
			other->Consume(bytesToMove);
			bytesMoved += bytesToMove;
			assert(bytesMoved >= numBytes);
		}
		iter = other->mBuf.begin();
	}
}

/// 把other中的数据插入到*this中的offset位置，并且删除从offset开始的numBytes个字节
void IOBuffer::Splice(IOBuffer *other, int offset, int numBytes)
{
	list<IOBufferDataPtr>::iterator iter, insertPt = mBuf.begin();
	IOBufferDataPtr data, dataCopy;
	int startPos = 0, extra;

	// 先对本类进行存储扩充，扩充所需要的字节数，并进行0填充
	extra = offset - BytesConsumable();
	while (extra > 0)
	{
		int zeroed = min(IOBUFSIZE, (uint32_t) extra);
		data.reset(new IOBufferData());
		data->ZeroFill(zeroed);
		extra -= zeroed;
		mBuf.push_back(data);
	}

	// 确保插入位置合法
	assert(BytesConsumable() >= offset);

	// 确保有足够的数据可以供插入
	assert(other->BytesConsumable() >= numBytes);

	// find the insertion point
	iter = mBuf.begin();

	// 找到offset所在块，并且将这一个块分成两个部分，完成之后，startPos在分开后的
	// 第二个块的开始位置
	while ((iter != mBuf.end()) && (startPos < offset))
	{
		data = *iter;
		if (data->BytesConsumable() + startPos > offset)
		{	// 此时说明插入点就在这一个块中

			// offset刚刚好在本块时，移动前半部分数据
			int bytesToCopy = offset - startPos;

			dataCopy.reset(new IOBufferData());

			// 备份原来的块的前一部分，大小：bytesToCopy
			dataCopy->CopyIn(data.get(), bytesToCopy);

			// 向后移动consumer指针bytesToCopy个位置。
			data->Consume(bytesToCopy);

			// 将这一个块插入到iter之前
			mBuf.insert(iter, dataCopy);
			startPos += dataCopy->BytesConsumable();
		}
		else
		{
			startPos += data->BytesConsumable();
			++iter;
		}
		insertPt = iter;
	}

	// get rid of stuff between [offset...offset+numBytes]
	// 此时的iter指向新生成的第二个块，startPos在新生成的第二个块的块首，
	// offset也指向第二个块的块首
	while ((iter != mBuf.end()) && (startPos < offset + numBytes))
	{
		data = *iter;
		extra = data->BytesConsumable();

		// 界定数据范围
		if (startPos + extra > offset + numBytes)
		{	// 插入的终点在这一个块中

			extra = offset + numBytes - startPos;
		}
		data->Consume(extra);
		startPos += extra;
		++iter;
	}

	// now, put the thing at insertPt
	if (insertPt != mBuf.end())
		mBuf.splice(insertPt, other->mBuf);
	else
	{
		iter = other->mBuf.begin();
		while (iter != other->mBuf.end())
		{
			data = *iter;
			mBuf.push_back(data);
			other->mBuf.pop_front();
			iter = other->mBuf.begin();
		}
	}
}

// 在mBuf之后扩展numBytes个字节的已经被0填充的空间
void IOBuffer::ZeroFill(int numBytes)
{
	IOBufferDataPtr data;

	while (numBytes > 0)
	{
		int zeroed = min(IOBUFSIZE, (uint32_t) numBytes);
		data.reset(new IOBufferData());
		data->ZeroFill(zeroed);
		numBytes -= zeroed;
		mBuf.push_back(data);
	}
}

/// 从文件fd中读入数据，读入完成后返回读入数据的大小。
/// 将整个文件的所有数据读入到缓存中
int IOBuffer::Read(int fd)
{
	IOBufferDataPtr data;
	int numRead = 0, res = -EAGAIN;

	if (mBuf.empty())
	{
		data.reset(new IOBufferData());
		mBuf.push_back(data);
	}

	while (1)
	{
		// 获取最后一个元素
		data = mBuf.back();

		if (data->IsFull())
		{
			data.reset(new IOBufferData());
			mBuf.push_back(data);
			continue;
		}
		res = data->Read(fd);

		// 当读文件完成时终止
		if (res <= 0)
			break;
		numRead += res;
#if 0
		// XXX: experimental; we want to read minimal amoutn and see
		// if we can process it.
		if (numRead >= (int) (2 * IOBUFSIZE))
		break;
#endif
	}

	if ((numRead == 0) && (res < 0))
		// even when res = -1, we get an errno of 0
		return errno == 0 ? -EAGAIN : errno;

	return numRead;
}

// 向文件中写入缓存中的数据，完成写操作之后，缓存中数据将清空
int IOBuffer::Write(int fd)
{
	int res = -EAGAIN, numWrote = 0;
	IOBufferDataPtr data;
	bool didSend = false;

	while (!mBuf.empty())
	{
		data = mBuf.front();
		if (data->IsEmpty())
		{
			mBuf.pop_front();
			continue;
		}
		assert(data->BytesConsumable()> 0);
		didSend = true;
		res = data->Write(fd);
		if (res <= 0)
			break;

		numWrote += res;
	}
	if (!didSend)
		return -EAGAIN;

	if ((numWrote == 0) && (res < 0))
	{
		// even when res = -1, we get an errno of 0
		return errno == 0 ? -EAGAIN : errno;
	}

	return numWrote;
}

// 计算buffer中所有的BufferData中可用字节数
int IOBuffer::BytesConsumable()
{
	list<IOBufferDataPtr>::iterator iter;
	IOBufferDataPtr data;
	int numBytes = 0;

	for (iter = mBuf.begin(); iter != mBuf.end(); iter++)
	{
		data = *iter;
		numBytes += data->BytesConsumable();
	}
	return numBytes;
}

/// 删除Buffer中的nbyte个字节的数据（即向后移动comsumer指针），但不影响list的节点数目
void IOBuffer::Consume(int nbytes)
{
	list<IOBufferDataPtr>::iterator iter;
	IOBufferDataPtr data;
	int bytesConsumed;

	assert(nbytes >= 0);
	iter = mBuf.begin();
	while (iter != mBuf.end())
	{
		data = *iter;
		bytesConsumed = data->Consume(nbytes);
		nbytes -= bytesConsumed;
		if (data->IsEmpty())
			mBuf.pop_front();
		if (nbytes <= 0)
			break;
		iter = mBuf.begin();
	}
	assert(nbytes == 0);
}

/// 截去尾部的nbytes个字节
void IOBuffer::Trim(int nbytes)
{
	list<IOBufferDataPtr>::iterator iter;
	IOBufferDataPtr data;
	int bytesAvail, totBytes = 0;

	if (nbytes <= 0)
		return;

	iter = mBuf.begin();
	while (iter != mBuf.end())
	{
		data = *iter;
		bytesAvail = data->BytesConsumable();
		if (bytesAvail + totBytes <= nbytes)
		{
			totBytes += bytesAvail;
			++iter;
			continue;
		}
		if (totBytes == nbytes)
			break;

		data->Trim(nbytes - totBytes);
		++iter;
		break;
	}

	while (iter != mBuf.end())
	{
		data = *iter;
		data->Consume(data->BytesConsumable());
		++iter;
	}
	assert(BytesConsumable() == nbytes);
}

/// 从buf中拷贝numBytes个字节的数据到buffer末尾
int IOBuffer::CopyIn(const char *buf, int numBytes)
{
	IOBufferDataPtr data;
	int numCopied = 0, bytesCopied;

	if (mBuf.empty())
	{
		data.reset(new IOBufferData());
		mBuf.push_back(data);
	}
	else
	{
		data = mBuf.back();
	}

	while (numCopied < numBytes)
	{
		assert(data.get() != NULL);
		bytesCopied = data->CopyIn(buf + numCopied, numBytes - numCopied);
		numCopied += bytesCopied;
		if (numCopied >= numBytes)
			break;
		data.reset(new IOBufferData());
		mBuf.push_back(data);
	}

	return numCopied;
}

/// 从Buffer的开头拷贝numBytes个字节到buf中，但并不改变任何指针位置
int IOBuffer::CopyOut(char *buf, int numBytes)
{
	list<IOBufferDataPtr>::iterator iter;
	IOBufferDataPtr data;
	char *curr = buf;
	int nread = 0, copied;

	buf[0] = '\0';
	for (iter = mBuf.begin(); iter != mBuf.end(); iter++)
	{
		data = *iter;
		copied = data->CopyOut(curr, numBytes - nread);
		assert(copied >= 0);
		curr += copied;
		nread += copied;
		assert(curr <= buf + numBytes);

		if (nread >= numBytes)
			break;
	}

	return nread;
}

//
// Clone the contents of an IOBuffer by block sharing
//
IOBuffer *IOBuffer::Clone()
{
	IOBuffer *clone = new IOBuffer();

	list<IOBufferDataPtr>::iterator iter;
	IOBufferDataPtr data1, data2;
	int numBytes;

	for (iter = mBuf.begin(); iter != mBuf.end(); iter++)
	{
		data1 = *iter;
		numBytes = data1->BytesConsumable();
		data2.reset(new IOBufferData(data1, data1->Consumer(),
				data1->Consumer() + numBytes));
		clone->mBuf.push_back(data2);
	}

	return clone;
}
