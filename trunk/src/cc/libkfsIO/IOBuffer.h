//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: IOBuffer.h 71 2008-07-07 15:49:14Z sriramsrao $
//
// Created 2006/03/14
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

#ifndef _LIBIO_IOBUFFER_H
#define _LIBIO_IOBUFFER_H

#include <cassert>
#include <list>
#include <exception>

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

namespace KFS
{

///
/// @file IOBuffer.h
/// @brief Buffers used for I/O in KFS.
///
/// An IOBuffer in KFS is a stream of IOBufferData.  Each IOBufferData
/// is an immutable buffer---that is, there is a single producer for
/// data in the buffer and that data maybe consumed by multiple
/// consumers.
///
/// In the current implementation, IOBufferData objects are single
/// producer, multiple consumers.
///

class IOBufferData;

typedef boost::shared_array<char> IOBufferBlockPtr;
///
/// \typedef IOBufferDataPtr
/// Since an IOBufferData can be shared, encapsulate it in a smart
/// pointer so that the cleanup occurs when all references are
/// released.
///
typedef boost::shared_ptr<IOBufferData> IOBufferDataPtr;

///
/// \class IOBufferData
/// \brief An IOBufferData contains a buffer and associated
/// producer/consumer points.

class IOBufferData
{
public:
	IOBufferData();

	/// Create an IOBufferData blob with a backing buffer of the specified size.
	IOBufferData(uint32_t bufsz);

	/// Create an IOBufferData blob by sharing data block from other;
	/// set the producer/consumer based on the start/end positions
	/// that are passed in
	/// 共享数据，指出被共享块的开头和结尾指针
	IOBufferData(IOBufferDataPtr &other, char *s, char *e);
	~IOBufferData();

	///
	/// Read data from file descriptor into the buffer.
	/// @param[in] fd file descriptor to be used for reading.
	/// @result Returns the # of bytes read
	///

	/// 从文件fd中读入数据，数据指针在文件指针中
	int Read(int fd);

	///
	/// Write data from the buffer to the file descriptor.
	/// @param[in] fd file descriptor to be used for writing.
	/// @result Returns the # of bytes written
	///
	/// 从缓冲器中写数据到磁盘，写完之后所写的数据将不再可用
	int Write(int fd);

	///
	/// Copy data into the buffer.  For doing a copy, data is appended
	/// to the buffer starting at the offset corresponding to
	/// mProducer.  # of bytes copied is min (# of bytes, space
	/// avail), where space avail = mEnd - mProducer.
	///
	/// NOTE: As a result of copy, the "producer" pointer is not
	/// advanced.
	///
	/// @param[out] buf A containing the data to be copied in.
	/// @param[in] numBytes # of bytes to be copied.
	/// @retval Returns the # of bytes copied.
	///
	// 将buffer中的数据拷贝到本类中，并相应移动producer指针
	int CopyIn(const char *buf, int numBytes);
	// 从其他buffer中拷贝数据（数据的起始位置为consumer），相应改变本类中producer指针，
	// 并且不应该影响被拷贝的数据buffer
	int CopyIn(const IOBufferData *other, int numBytes);
	///
	/// Copy data out the buffer.  For doing a copy, data is copied
	/// out of the buffer starting at the offset corresponding to
	/// mConsumer.  # of bytes copied is min (# of bytes, bytes
	/// avail), where bytes avail = mProducer - mConsumer.
	///
	/// NOTE: As a result of copy, the "consumer" pointer is not
	/// advanced.
	///
	/// @param[out] buf A containing the data to be copied in.
	/// @param[in] numBytes # of bytes to be copied.
	/// @retval Returns the # of bytes copied.
	///
	/// 把数据拷贝到buffer，并且不作任何指针移动的操作
	int CopyOut(char *buf, int numBytes);

	char *Start()
	{
		return mStart;
	}
	char *Producer()
	{
		return mProducer;
	}
	char *Consumer()
	{
		return mConsumer;
	}

	///
	/// Some data has been filled in the buffer.  So, advance
	/// mProducer.
	/// @param[in] nbytes # of bytes of data filled
	/// @retval # of bytes filled in this buffer.
	///
	/// 只是移动mProducer指针，并没有实际意义上的填充
	int Fill(int nbytes);
	/// 填充0，从producer指针开始。若可用空间不够，则填充到数组末尾
	int ZeroFill(int nbytes);

	///
	/// Some data has been consumed from the buffer.  So, advance
	/// mConsumer.
	/// @param[in] nbytes # of bytes of data consumed
	/// @retval # of bytes consumed from this buffer.
	///
	/// 只是向后移动consumer指针，并没有实际意义上的“消费”
	int Consume(int nbytes);

	///
	/// Remove some data from the end of the buffer.  So, pull back
	/// mProducer
	/// @param[in] nbytes # of bytes of data to be trimmed
	/// @retval # of bytes in this buffer.
	///
	/// 截取从consumer指针开始的nbytes的数据。若没有那么多数据，则不作操作。
	/// 返回截取之后缓存中的字节数
	int Trim(int nbytes);

	/// Returns the # of bytes available for consumption.
	int BytesConsumable()
	{
		return mProducer - mConsumer;
	}

	/// Return the space available in the buffer
	size_t SpaceAvailable()
	{
		return mEnd - mProducer;
	}
	int IsFull()
	{
		return mProducer == mEnd;
	}
	int IsEmpty()
	{
		return mProducer == mConsumer;
	}

private:
	/// Data buffer that is ref-counted for sharing.
	/// 用char数组存放缓冲数据
	IOBufferBlockPtr mData;
	/// Pointers that correspond to the start/end of the buffer
	/// buffer空间的起始位置
	char *mStart, *mEnd;
	/// Pointers into mData that correspond to producer/consumer
	/// 读写数据的起始位置
	char *mProducer, *mConsumer;

	/// Allocate memory and init the pointers.
	void Init(uint32_t bufsz);

};

///
/// @struct IOBuffer
/// An IOBuffer consists of a list of IOBufferDataPtr.  It provides
/// API's for reading/writing data to/from the buffer.  Operations on
/// IOBuffer translates to operations on appropriate IOBufferDataPtr.
///
/// 一个buffer里边包含若干个IOBufferData
/// 提供由文件到缓存，由缓存到文件的管理控制
struct IOBuffer
{
	IOBuffer();
	~IOBuffer();

	/// 通过块共享的方式克隆IOBuffer
	IOBuffer *Clone();

	/// Append the IOBufferData block to the list stored in this buffer.
	void Append(IOBufferDataPtr &buf);

	/// Append the contents of ioBuf to this buffer.
	/// 把ioBuf中的数据，合并的这个buufer中
	void Append(IOBuffer *ioBuf);

	/// 从文件fd中读入数据，读入完成后返回读入数据的大小。
	/// 将整个文件的所有数据读入到缓存中
	int Read(int fd);

	/// 向文件中写入缓存中的数据，完成写操作之后，缓存中数据将清空
	int Write(int fd);

	/// Move data from one buffer to another.  This involves (mostly)
	///   pointers without incurring data copying.
	/// The requirement is that "other" better have as much bytes as
	/// we are trying to move.
	/// @param[in] other  Buffer from which data has to be moved
	/// @param[in] numBytes  # of bytes of data to be moved over
	///
	/// 将other中的前numBytes数据剪切到本类中，并且对相应指针作适当移动
	void Move(IOBuffer *other, int numBytes);

	/// Splice data from other to "this".  The key here is that, data
	/// from other is inserted starting at the specified offset.  The
	/// requirements are that: (1) data in "this" should be at least
	/// of length offset bytes, and (2) other better have as much
	/// bytes as we are trying to move.
	/// @param[in] other  Buffer from which data has to be spliced
	/// @param[in] offset  The offset at which data has to be spliced in
	/// @param[in] numBytes  # of bytes of data to be moved over
	/// 把other中的数据插入到*this中的offset位置，并且删除从offset开始的numBytes个字节
	void Splice(IOBuffer *other, int offset, int numBytes);

	/// Zero fill the buffer for length numBytes.
	/// @param[in] numBytes  # of bytes to be zero-filled.
	/// 在mBuf之后扩展numBytes个字节的已经被0填充的空间
	void ZeroFill(int numBytes);

	///
	/// Copy data into the buffer.  For doing a copy, data is appended
	/// to the last buffer in mBuf.  If the amount of data to be
	/// copied exceeds space in the last buffer, additional buffers
	/// are allocated and copy operation runs to finish.
	///
	/// NOTE: As a result of copy, the "producer" portion of an
	/// IOBufferData is not advanced.
	///
	/// @param[in] buf A containing the data to be copied in.
	/// @param[in] numBytes # of bytes to be copied in.
	/// @retval Returns the # of bytes copied.
	/// 从buf中拷贝numBytes个字节的数据到buffer末尾，并且移动producer指针
	int CopyIn(const char *buf, int numBytes);

	///
	/// Copy data out of the buffer.  For doing a copy, data is copied
	/// from the first buffer in mBuf.  If the amount of data to be
	/// copied exceeds what is available in the first buffer, the list
	/// of buffers is walked to copy out data.
	///
	/// NOTE: As a result of copy, the "consumer" portion of an
	/// IOBufferData is not advanced.
	///
	/// @param[out] buf A null-terminated buffer containing the data
	/// copied out.
	/// @param[in] bufLen Length of buf passed in.  At most bufLen
	/// bytes are copied out.
	/// @retval Returns the # of bytes copied.
	/// 从Buffer的开头拷贝numBytes个字节到buf中，但并不改变任何指针位置
	int CopyOut(char *buf, int bufLen);

	///
	/// Consuming data in the IOBuffer translates to advancing the
	/// "consumer" point on underlying IOBufferDataPtr.  From the head
	/// of the list, the consumer point will be advanced on sufficient
	/// # of buffers.
	/// 删除Buffer中的nbyte个字节的数据（即向后移动comsumer指针），但不影响list的节点数目
	void Consume(int nbytes);

	/// Returns the # of bytes that are available for consumption.
	/// 计算buffer中“所有”的BufferData中可用字节数
	int BytesConsumable();

	/// Trim data from the end of the buffer to nbytes.  This is the
	/// converse of consume, where data is removed from the front of
	/// the buffer.
	/// 截去尾部的nbytes个字节
	void Trim(int nbytes);

	/// List of IOBufferData blocks that comprise this buffer.
	std::list<IOBufferDataPtr> mBuf;
};

namespace libkfsio
{
/// API to set the default allocation when allocating
/// IOBufferData().  The default allocation unit is 4K unless
/// changed by this API call.
void SetIOBufferSize(uint32_t bufsz);
}

}

#endif // _LIBIO_IOBUFFER_H
