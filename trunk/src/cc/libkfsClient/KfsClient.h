//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: KfsClient.h 139 2008-08-26 04:00:27Z sriramsrao $
//
// Created 2006/04/18
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
// \file KfsClient.h
// \brief Kfs Client-library code.
//
//----------------------------------------------------------------------------

#ifndef LIBKFSCLIENT_KFSCLIENT_H
#define LIBKFSCLIENT_KFSCLIENT_H

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

#include <boost/shared_ptr.hpp>
#include <sys/stat.h>

#include "KfsAttr.h"

namespace KFS
{

class KfsClientImpl;

/// Maximum length of a filename
const size_t MAX_FILENAME_LEN = 256;

///
/// \brief The KfsClient is the "bridge" between applications and the
/// KFS servers (either the metaserver or chunkserver): there can be
/// only one client per metaserver.
///
/// The KfsClientFactory class can be used to produce KfsClient
/// objects, where each client is used to interface with a different
/// metaserver. The preferred method of creating a client object is
/// thru the client factory.
///

///
/// @brief KfsClient的一个实例，是通过KfsClientImp来实现的
///
class KfsClient
{
public:
	///
	KfsClient();
	///
	~KfsClient();

	///
	/// @param[in] metaServerHost  Machine on meta is running
	/// @param[in] metaServerPort  Port at which we should connect to
	/// @retval 0 on success; -1 on failure
	///
	int Init(const std::string metaServerHost, int metaServerPort);

	///
	bool IsInitialized();

	///
	/// Provide a "cwd" like facility for KFS.
	/// @param[in] pathname  The pathname to change the "cwd" to
	/// @retval 0 on sucess; -errno otherwise
	///
	// Terminal: cd
	int Cd(const char *pathname);

	/// Get cwd
	/// @retval a string that describes the current working dir.
	///
	// Terminal: pwd
	std::string GetCwd();

	///
	/// Make a directory hierarcy in KFS.  If the parent dirs are not
	/// present, they are also made.
	/// @param[in] pathname		The full pathname such as /.../dir
	/// @retval 0 if mkdir is successful; -errno otherwise
	// 建立一个分层的目录，若父亲目录不存在，则同时创建父亲目录
	int Mkdirs(const char *pathname);

	///
	/// Make a directory in KFS.
	/// @param[in] pathname		The full pathname such as /.../dir
	/// @retval 0 if mkdir is successful; -errno otherwise
	// Terminal: mkdir
	int Mkdir(const char *pathname);

	///
	/// Remove a directory in KFS.
	/// @param[in] pathname		The full pathname such as /.../dir
	/// @retval 0 if rmdir is successful; -errno otherwise
	// Terminal: rmdir
	int Rmdir(const char *pathname);

	///
	/// Remove a directory hierarchy in KFS.
	/// @param[in] pathname		The full pathname such as /.../dir
	/// @retval 0 if rmdir is successful; -errno otherwise
	// 删除一系列的目录
	int Rmdirs(const char *pathname);

	///
	/// Read a directory's contents
	/// @param[in] pathname	The full pathname such as /.../dir
	/// @param[out] result	The contents of the directory
	/// @retval 0 if readdir is successful; -errno otherwise
	// Terminal: ls
	int Readdir(const char *pathname, std::vector<std::string> &result);

	///
	/// Read a directory's contents and retrieve the attributes
	/// @param[in] pathname	The full pathname such as /.../dir
	/// @param[out] result	The files in the directory and their attributes.
	/// @retval 0 if readdirplus is successful; -errno otherwise
	// Terminal: ls -l
	int ReaddirPlus(const char *pathname, std::vector<KfsFileAttr> &result);

	///
	/// Stat a file and get its attributes.
	/// @param[in] pathname	The full pathname such as /.../foo
	/// @param[out] result	The attributes that we get back from server
	/// @param[in] computeFilesize  When set, for files, the size of
	/// file is computed and the value is returned in result.st_size
	/// @retval 0 if stat was successful; -errno otherwise
	// 获取文件属性
	int Stat(const char *pathname, struct stat &result, bool computeFilesize =
			true);

	///
	/// Helper APIs to check for the existence of (1) a path, (2) a
	/// file, and (3) a directory.
	/// @param[in] pathname	The full pathname such as /.../foo
	/// @retval status: True if it exists; false otherwise
	///
	bool Exists(const char *pathname);

	///
	bool IsFile(const char *pathname);

	///
	bool IsDirectory(const char *pathname);

	///
	/// For testing/debugging purposes, would be nice to know where
	/// the blocks of a file are and what their sizes happen to be.
	/// @param[in] pathname   The full path to the file that is being
	/// queried
	/// @retval status code
	///
	int EnumerateBlocks(const char *pathname);

	/// Given a vector of checksums, one value per checksum block,
	/// verify that it matches with what is stored at each of the
	/// replicas in KFS.
	/// @retval status code
	/// 验证整个文件的校验和是否正确
	bool VerifyDataChecksums(const char *pathname,
			const std::vector<uint32_t> &checksums);

	/// Helper variety of verifying checksums: given a region of a
	/// file, compute the checksums and verify them.  This is useful
	/// for testing purposes.
	/// 文件号，偏移地址，字符串，字符串长度
	bool VerifyDataChecksums(int fd, off_t offset, const char *buf,
			size_t numBytes);

	///
	/// Create a file which is specified by a complete path.
	/// @param[in] pathname that has to be created
	/// @param[in] numReplicas the desired degree of replication for
	/// the file.
	/// @param[in] exclusive  create will fail if the exists (O_EXCL flag)
	/// @retval on success, fd corresponding to the created file;
	/// -errno on failure.
	/// 创建一个文件，存在pathname中，默认拷贝是3个，默认忽略已经存在 >>???<<
	int Create(const char *pathname, int numReplicas = 3, bool exclusive =
			false);

	///
	/// Remove a file which is specified by a complete path.
	/// @param[in] pathname that has to be removed
	/// @retval status code
	///
	/// Terminal: rm
	int Remove(const char *pathname);

	///
	/// Rename file/dir corresponding to oldpath to newpath
	/// @param[in] oldpath   path corresponding to the old name
	/// @param[in] newpath   path corresponding to the new name
	/// @param[in] overwrite  when set, overwrite the newpath if it
	/// exists; otherwise, the rename will fail if newpath exists
	/// @retval 0 on success; -1 on failure
	/// Terminal: rename
	int Rename(const char *oldpath, const char *newpath, bool overwrite = true);

	///
	/// Open a file
	/// @param[in] pathname that has to be opened
	/// @param[in] openFlags modeled after open().  The specific set
	/// of flags currently supported are:
	/// O_CREAT, O_CREAT|O_EXCL, O_RDWR, O_RDONLY, O_WRONLY, O_TRUNC, O_APPEND
	/// @param[in] numReplicas if O_CREAT is specified, then this the
	/// desired degree of replication for the file
	/// @retval fd corresponding to the opened file; -errno on failure
	/// Open a file
	int Open(const char *pathname, int openFlags, int numReplicas = 3);

	///
	/// Return file descriptor for an open file
	/// @param[in] pathname of file
	/// @retval file descriptor if open, error code < 0 otherwise
	/// 获取一个已经打开的文件的号码。意思是这个文件多次打开的话，用的是同一指针？？
	int Fileno(const char *pathname);

	///
	/// Close a file
	/// @param[in] fd that corresponds to a previously opened file
	/// table entry.
	/// 关闭文件
	int Close(int fd);

	///
	/// Read/write the desired # of bytes to the file, starting at the
	/// "current" position of the file.
	/// @param[in] fd that corresponds to a previously opened file
	/// table entry.
	/// @param buf For read, the buffer will be filled with data; for
	/// writes, this buffer supplies the data to be written out.
	/// @param[in] numBytes   The # of bytes of I/O to be done.
	/// @retval On success, return of bytes of I/O done (>= 0);
	/// on failure, return status code (< 0).
	///
	ssize_t Read(int fd, char *buf, size_t numBytes);
	ssize_t Write(int fd, const char *buf, size_t numBytes);

	///
	/// \brief Sync out data that has been written (to the "current" chunk).
	/// @param[in] fd that corresponds to a file that was previously
	/// opened for writing.
	/// >>???<<
	int Sync(int fd);

	/// \brief Adjust the current position of the file pointer similar
	/// to the seek() system call.
	/// @param[in] fd that corresponds to a previously opened file
	/// @param[in] offset offset to which the pointer should be moved
	/// relative to whence.
	/// @param[in] whence one of SEEK_CUR, SEEK_SET, SEEK_END
	/// @retval On success, the offset to which the filer
	/// pointer was moved to; (off_t) -1 on failure.
	///
	off_t Seek(int fd, off_t offset, int whence);

	/// In this version of seek, whence == SEEK_SET
	off_t Seek(int fd, off_t offset);

	/// Return the current position of the file pointer in the file.
	/// @param[in] fd that corresponds to a previously opened file
	/// @retval value returned is analogous to calling ftell()
	off_t Tell(int fd);

	///
	/// Truncate a file to the specified offset.
	/// @param[in] fd that corresponds to a previously opened file
	/// @param[in] offset  the offset to which the file should be truncated
	/// @retval status code
	int Truncate(int fd, off_t offset);

	///
	/// Given a starting offset/length, return the location of all the
	/// chunks that cover this region.  By location, we mean the name
	/// of the chunkserver that is hosting the chunk. This API can be
	/// used for job scheduling.
	///
	/// @param[in] pathname	The full pathname of the file such as /../foo
	/// @param[in] start	The starting byte offset
	/// @param[in] len		The length in bytes that define the region
	/// @param[out] locations	The location(s) of various chunks
	/// @retval status: 0 on success; -errno otherwise
	/// 为什么locations用一个二维string数组表示呢？不同的拷贝？
	int GetDataLocation(const char *pathname, off_t start, size_t len,
			std::vector<std::vector<std::string> > &locations);

	///
	int GetDataLocation(int fd, off_t start, size_t len, std::vector<
			std::vector<std::string> > &locations);

	///
	/// Get the degree of replication for the pathname.
	/// @param[in] pathname	The full pathname of the file such as /../foo
	/// @retval count
	///
	int16_t GetReplicationFactor(const char *pathname);

	///
	/// Set the degree of replication for the pathname.
	/// @param[in] pathname	The full pathname of the file such as /../foo
	/// @param[in] numReplicas  The desired degree of replication.
	/// @retval -1 on failure; on success, the # of replicas that will be made.
	///
	int16_t SetReplicationFactor(const char *pathname, int16_t numReplicas);

	///
	ServerLocation GetMetaserverLocation() const;
private:
	/// 功能的实现体
	KfsClientImpl *mImpl;
};

/// @brief 定义智能指针类型
typedef boost::shared_ptr<KfsClient> KfsClientPtr;

/// @brief 制造Client实例，一个Client进程能且只能使用一个KfsClientFactory的实例
// 一个ClientFactory中可能有一个或者多个Client实例，但是对于同一个MetaServer，只能有一个
// Client实例
class KfsClientFactory
{
	/// Make the constructor private to get a Singleton.
	KfsClientFactory()
	{
	}

	///
	KfsClientFactory(const KfsClientFactory &other);

	///
	const KfsClientFactory & operator=(const KfsClientFactory &other);

	///
	KfsClientPtr mDefaultClient;

	///
	std::vector<KfsClientPtr> mClients;
public:
	///
	static KfsClientFactory *Instance()
	{
		static KfsClientFactory instance;
		return &instance;
	}

	///
	void SetDefaultClient(KfsClientPtr &clnt)
	{
		mDefaultClient = clnt;
	}

	///
	KfsClientPtr GetClient()
	{
		return mDefaultClient;
	}

	///
	/// @param [in] 配置文件
	/// @return Client实例的指针
	///
	KfsClientPtr GetClient(const char *propFile);

	///
	/// @brief 从KfsClientFactory中取出指定用户，若此用户不存在，则创建此用户，并存入Factory中
	/// @note 查找是否有用户，判断两个用户等价的条件：主机地址和端口地址都相同\n
	/// @param metaServerHost 服务器主机名
	/// @param metaServerPort 服务器端口号
	/// @note 保证一个客户端中，对于同一个主机同一个端口的client只有一个
	///
	KfsClientPtr
			GetClient(const std::string metaServerHost, int metaServerPort);
};

/// @param[in] status  The status code for an error.
/// @retval String that describes what the error is.
/// @brief 将错误代码转换成字符串信息
///
extern std::string ErrorCodeToStr(int status);

///
/// @return KfsClientFactory的实例
/// @note 任何时刻，该函数返回的KfsClientFactory都是同一个实例
///
extern KfsClientFactory *getKfsClientFactory();

}

#endif // LIBKFSCLIENT_KFSCLIENT_H
