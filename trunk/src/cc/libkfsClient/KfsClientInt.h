//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: KfsClientInt.h 154 2008-09-17 19:15:35Z sriramsrao $
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
//
//----------------------------------------------------------------------------

#ifndef LIBKFSCLIENT_KFSCLIENTINT_H
#define LIBKFSCLIENT_KFSCLIENTINT_H

#include <string>
#include <vector>
#include <tr1/unordered_map>
#include <sys/select.h>
#include "common/log.h"
#include "common/hsieh_hash.h"
#include "common/kfstypes.h"
#include "libkfsIO/TcpSocket.h"
#include "libkfsIO/Checksum.h"

#include "KfsAttr.h"

#include "KfsOps.h"
#include "LeaseClerk.h"

#include "concurrency.h"

namespace KFS
{

/// Set this to 1MB: 64K * 16
const size_t MIN_BYTES_PIPELINE_IO = CHECKSUM_BLOCKSIZE * 16 * 4;

/// Per write, push out at most one checksum block size worth of data
const size_t MAX_BYTES_PER_WRITE_IO = CHECKSUM_BLOCKSIZE;
/// on zfs, blocks are 128KB on disk; so, align reads appropriately
const size_t MAX_BYTES_PER_READ_IO = CHECKSUM_BLOCKSIZE * 2;

/// If an op fails because the server crashed, retry the op.  This
/// constant defines the # of retries before declaring failure.
const uint8_t NUM_RETRIES_PER_OP = 3;

/// Whenever an op fails, we need to give time for the server to
/// recover.  So, introduce a delay of 5 secs between retries.
const int RETRY_DELAY_SECS = 5;

/// Directory entries that we may have cached are valid for 30 secs;
/// after that force a revalidataion.
const int FILE_CACHE_ENTRY_VALID_TIME = 30;

///
/// A KfsClient maintains a file-table that stores information about
/// KFS files on that client.  Each file in the file-table is composed
/// of some number of chunks; the meta-information about each
/// chunk is stored in a chunk-table associated with that file.  Thus, given a
/// <file-id, offset>, we can map it to the appropriate <chunk-id,
/// offset within the chunk>; we can also find where that piece of
/// data is located and appropriately access it.
///

///
/// @brief Buffer for speeding up small reads and writes; holds
/// on to a piece of data from one chunk.
/// chunk在本地的缓存，默认大小为4MB
struct ChunkBuffer
{
	/// set the client buffer to be fairly big...for sequential reads,
	/// we will hit the network few times and on each occasion, we read
	/// a ton and thereby get decent performance; having a big buffer
	/// obviates the need to do read-ahead :-)
	static const size_t ONE_MB = 1 << 20;
	/// to reduce memory footprint, keep a decent size buffer; we used to have
	/// 64MB before
	static const size_t BUF_SIZE =
			KFS::CHUNKSIZE < 4 * ONE_MB ? KFS::CHUNKSIZE : 4 * ONE_MB;

	///
	ChunkBuffer() :
		chunkno(-1), start(0), length(0), dirty(false), buf(NULL)
	{
	}

	///
	~ChunkBuffer()
	{
		delete[] buf;
	}

	///
	void invalidate()
	{
		chunkno = -1;
		start = 0;
		length = 0;
		dirty = false;
		delete[] buf;
	}

	///
	void allocate()
	{
		if (buf)
			return;
		buf = new char[BUF_SIZE];
	}
	int chunkno; 	/// which chunk
	off_t start; 	/// offset with chunk
	size_t length; 	/// length of valid data
	bool dirty;		/// must flush to server if true
	char *buf;		/// the data
};

/// @brief 用于记录同服务器的连接
struct ChunkServerConn
{
	/// name/port of the chunk server to which this socket is
	/// connected.
	ServerLocation location;
	/// connected TCP socket.  If this object is copy constructed, we
	/// really can't afford this socket to close when the "original"
	/// is destructed. To protect such issues, make this a smart pointer.
	TcpSocketPtr sock;

	///
	ChunkServerConn(const ServerLocation &l) :
		location(l)
	{
		sock.reset(new TcpSocket());
	}

	///
	void Connect(bool nonblockingConnect = false)
	{
		if (sock->IsGood())
			return;
		int res;

		res = sock->Connect(location, nonblockingConnect);
		if (res == -EINPROGRESS)
		{
			struct timeval selectTimeout;
			fd_set writeSet;
			int sfd = sock->GetFd();

			FD_ZERO(&writeSet);
			FD_SET(sfd, &writeSet);

			selectTimeout.tv_sec = 30;
			selectTimeout.tv_usec = 0;

			res = select(sfd + 1, NULL, &writeSet, NULL, &selectTimeout);
			if ((res > 0) && (FD_ISSET(sfd, &writeSet)))
			{
				// connection completed
				return;
			}
			KFS_LOG_VA_INFO("Non-blocking connect to location %s failed", location.ToString().c_str());
			res = -EHOSTUNREACH;
		}
		if (res < 0)
		{
			sock.reset(new TcpSocket());
		}

	}

	///
	bool operator ==(const ServerLocation &other) const
	{
		return other == location;
	}
};

///
/// @brief Location of the file pointer in a file consists of two
/// parts: the offset in the file, which then translates to a chunk #
/// and an offset within the chunk.  Also, for performance, we do some
/// client-side buffering (for both reads and writes).  The buffer
/// stores data corresponding to the "current" chunk.
///
/// 文件当中的位置（逻辑位置）
struct FilePosition
{
	///
	FilePosition()
	{
		fileOffset = chunkOffset = 0;
		chunkNum = 0;
		preferredServer = NULL;
	}
	///
	~FilePosition()
	{

	}
	///
	void Reset()
	{
		fileOffset = chunkOffset = 0;
		chunkNum = 0;
		chunkServers.clear();
		preferredServer = NULL;
	}

	off_t fileOffset; /// offset within the file
	/// which chunk are we at: this is an index into fattr.chunkTable[]
	/// 当前数据所在的chunk号码
	int32_t chunkNum;
	/// offset within the chunk
	/// chunk内地址
	off_t chunkOffset;

	/// For the purpose of write, we may have to connect to multiple servers
	std::vector<ChunkServerConn> chunkServers;

	/// For reads as well as meta requests about a chunk, this is the
	/// preferred server to goto.  This is a pointer to a socket in
	/// the vector<ChunkServerConn> structure.
	/// 优先使用的读数据服务器
	TcpSocket *preferredServer;

	/// Track the location of the preferred server so we can print debug messages
	/// 优先使用的服务器的位置
	ServerLocation preferredServerLocation;

	///
	void ResetServers()
	{
		chunkServers.clear();
		preferredServer = NULL;
	}

	/// 查找指定的ChunkServer的socket，若找不到指定服务器，则为之建立一个连接，并且存储
	/// 在chunkServers中
	TcpSocket *GetChunkServerSocket(const ServerLocation &loc,
			bool nonblockingConnect = false)
	{
		std::vector<ChunkServerConn>::iterator iter;

		iter = std::find(chunkServers.begin(), chunkServers.end(), loc);
		if (iter != chunkServers.end())
		{
			iter->Connect(nonblockingConnect);
			TcpSocket *s = iter->sock.get();

			if (s->IsGood())
				return s;
			return NULL;
		}

		/* 查找失败 */
		// Bit of an issue here: The object that is being pushed is
		// copy constructed; when that object is destructed, the
		// socket it has will go.  To avoid that, we need the socket
		// to be a smart pointer.
		chunkServers.push_back(ChunkServerConn(loc));
		chunkServers[chunkServers.size() - 1].Connect(nonblockingConnect);

		TcpSocket *s = chunkServers[chunkServers.size() - 1].sock.get();
		if (s->IsGood())
			return s;
		return NULL;
	}

	///
	void SetPreferredServer(const ServerLocation &loc, bool nonblockingConnect =
			false)
	{
		preferredServer = GetChunkServerSocket(loc, nonblockingConnect);
		preferredServerLocation = loc;
	}

	///
	const ServerLocation &GetPreferredServerLocation() const
	{
		return preferredServerLocation;
	}

	/// 获取其socket
	TcpSocket *GetPreferredServer()
	{
		return preferredServer;
	}

	///
	int GetPreferredServerAddr(struct sockaddr_in &saddr)
	{
		if (preferredServer == NULL)
			return -1;
		return preferredServer->GetPeerName((struct sockaddr *) &saddr);
	}
};
///
typedef std::tr1::unordered_map<std::string, int, Hsieh_hash_fcn> NameToFdMap;
///
typedef std::tr1::unordered_map<std::string, int, Hsieh_hash_fcn>::iterator
		NameToFdMapIter;

///
/// @brief A table of entries that describe each open KFS file.
///
struct FileTableEntry
{
	// the fid of the parent dir in which this entry "resides"
	kfsFileId_t parentFid;	/// 文件描述符
	// stores the name of the file/directory.
	std::string name;		/// 文件或者目录名

	// store a pointer to the associated name-cache entry
	// NameToFdMapIter pathCacheIter;

	// the full pathname
	std::string pathname;	/// 绝对路径

	// one of O_RDONLY, O_WRONLY, O_RDWR; when it is 0 for a file,
	// this entry is used for attribute caching
	int openMode;			/// 打开模式

	/// 文件属性
	FileAttr fattr;

	/// chunk号码到chunk的映射
	std::map<int, ChunkAttr> cattr;

	// the position in the file at which the next read/write will occur
	FilePosition currPos;	/// 当前读写位置
	/// For the current chunk, do some amount of buffering on the
	/// client.  This helps absorb network latencies for small
	/// reads/writes.
	ChunkBuffer buffer;		/// 用于处理当前块的缓冲器

	/// for LRU reclamation of file table entries, track when this
	/// entry was last accessed
	time_t lastAccessTime;

	/// directory entries are cached; ala NFS, keep the entries cached
	/// for a max of 30 secs; after that revalidate
	time_t validatedTime;

	///
	FileTableEntry(kfsFileId_t p, const char *n) :
		parentFid(p), name(n), lastAccessTime(0), validatedTime(0)
	{
	}
};

///
/// @brief The implementation object.
///
class KfsClientImpl
{

public:
	///
	KfsClientImpl();

	///
	/// @param[in] metaServerHost  Machine on meta is running
	/// @param[in] metaServerPort  Port at which we should connect to
	/// @retval 0 on success; -1 on failure
	/// @brief 获取并连接metaServer
	/// @return 返回操作结果
	///
	int Init(const std::string metaServerHost, int metaServerPort);

	///
	ServerLocation GetMetaserverLocation() const
	{
		return mMetaServerLoc;
	}

	///
	bool IsInitialized()
	{
		return mIsInitialized;
	}
	;

	///
	/// Provide a "cwd" like facility for KFS.
	/// @param[in] pathname  The pathname to change the "cwd" to
	/// @retval 0 on sucess; -errno otherwise
	/// @note pathname可以给定绝对路径，也可以给定相对路径
	///
	int Cd(const char *pathname);

	/// Get cwd
	/// @retval a string that describes the current working dir.
	///
	std::string GetCwd();

	///
	/// @param[in] pathname		The full pathname such as /.../dir
	/// @retval 0 if mkdir is successful; -errno otherwise
	/// @breif 创建给定目录中的所有父级目录，pathname须给定绝对路径
	/// @note pathname须给定绝对路径
	int Mkdirs(const char *pathname);

	///
	/// @param[in] pathname		The full pathname such as /.../dir
	/// @retval 0 if mkdir is successful; -errno otherwise
	/// @brief 创建一个文件夹
	/// @note pathname必须是一个绝对路径
	///
	int Mkdir(const char *pathname);

	///
	/// Remove a directory in KFS.
	/// @param[in] pathname		The full pathname such as /.../dir
	/// @retval 0 if rmdir is successful; -errno otherwise
	/// @brief 删除指定目录
	/// @note 必须该出该目录的绝对路径
	///
	int Rmdir(const char *pathname);

	///
	/// @param[in] pathname		The full pathname such as /.../dir
	/// @retval 0 if rmdir is successful; -errno otherwise
	/// @brief 删除当前目录以及其中的文件和目录
	///
	int Rmdirs(const char *pathname);

	///
	/// @param[in] pathname	The full pathname such as /.../dir
	/// @param[out] result	The contents of the directory
	/// @retval 0 if readdir is successful; -errno otherwise
	/// @brief 读目录，返回目录中的文件名
	///
	int Readdir(const char *pathname, std::vector<std::string> &result);

	///
	/// Read a directory's contents and retrieve the attributes
	/// @param[in] pathname	The full pathname such as /.../dir
	/// @param[out] result	The files in the directory and their attributes.
	/// @param[in] computeFilesize  By default, compute file size
	/// @retval 0 if readdirplus is successful; -errno otherwise
	/// @note pathname是文件的绝对路径
	///
	int ReaddirPlus(const char *pathname, std::vector<KfsFileAttr> &result,
			bool computeFilesize = true);

	///
	/// @retval 0 if stat was successful; -errno otherwise
	/// @brief 获取文件的属性
	/// @param [in] pathname 文件或文件夹路径
	/// @param [out] result 获取到的文件属性
	/// @param[in] computeFilesize  When set, for files, the size of
	/// file is computed and the value is returned in result.st_size
	///
	int Stat(const char *pathname, struct stat &result, bool computeFilesize =
			true);

	///
	/// @param[in] pathname	The full pathname such as /.../foo
	/// @retval status: True if it exists; false otherwise
	/// @brief 通过查看文件属性来判断该文件是否存在
	///
	bool Exists(const char *pathname);

	///
	/// @brief 通过获取文件的属性来判断所查看的pathname是否是文件
	///
	bool IsFile(const char *pathname);

	///
	/// @brief 通过Stat查看某路径是否为目录
	///
	bool IsDirectory(const char *pathname);

	///
	/// @brief 计算文件中每一个块的每一个副本的大小，并且统计数据总量
	/// @note 所有均输出到标准输出
	///
	int EnumerateBlocks(const char *pathname);

	/// @retval status code
	/// @brief 用checksums中的数据对pathname指定的文件进行校验
	///
	bool VerifyDataChecksums(const char *pathname,
			const std::vector<uint32_t> &checksums);

	///
	/// @brief 校验buf和chunkserver上的数据
	/// @note 先计算出buf中数据校验和，然后再取出metaserver中各个块的数据校验和进行比较\n
	/// @note buf应该是文件的整个数据把
	///
	bool VerifyDataChecksums(int fd, off_t offset, const char *buf,
			size_t numBytes);

	///
	/// @param[in] pathname that has to be created
	/// @param[in] numReplicas the desired degree of replication for
	/// the file.
	/// @param[in] exclusive  create will fail if the exists (O_EXCL flag)
	/// @retval on success, fd corresponding to the created file;
	/// -errno on failure.
	/// @brief 创建并打开新建的文件
	/// @note 打开模式为 可读可写, 路径须为绝对路径
	///
	int Create(const char *pathname, int numReplicas = 3, bool exclusive =
			false);

	///
	/// @param[in] pathname that has to be removed
	/// @retval status code
	/// @brief 删除文件
	/// @note 之前先清除该文件在“已经打开的文件列表”中的目录
	///
	int Remove(const char *pathname);

	///
	/// @param[in] oldpath   path corresponding to the old name
	/// @param[in] newpath   path corresponding to the new name
	/// @param[in] overwrite  when set, overwrite the newpath if it
	/// exists; otherwise, the rename will fail if newpath exists
	/// @retval 0 on success; -1 on failure
	/// @brief 给一个文件重命名
	/// @note 先发送重命名指令，然后再更新文件在本地的缓存
	///
	int Rename(const char *oldpath, const char *newpath, bool overwrite = true);

	///
	/// @param[in] pathname that has to be opened
	/// @param[in] openFlags modeled after open().  The specific set
	/// of flags currently supported are:
	/// O_CREAT, O_CREAT|O_EXCL, O_RDWR, O_RDONLY, O_WRONLY, O_TRUNC, O_APPEND
	/// @param[in] numReplicas if O_CREAT is specified, then this the
	/// desired degree of replication for the file
	/// @retval fd corresponding to the opened file; -errno on failure
	/// @brief 根据不同的打开模式打开文件，为之分配一个目录并且获取该文件的基本属性和大小属性
	///
	int Open(const char *pathname, int openFlags, int numReplicas = 3);

	///
	/// @param[in] pathname of file
	/// @retval file descriptor if open, error code < 0 otherwise
	/// @brief 查找指定文件名在“已经打开的文件目录”中的位置
	///
	int Fileno(const char *pathname);

	///
	/// @param[in] fd that corresponds to a previously opened file
	/// table entry.
	/// @brief 关闭一个文件
	/// @note 如果该文件的数据已经更新，则同步该文件\n
	/// @note 这个操作不清除缓存中的文件信息
	///
	int Close(int fd);

	///
	/// Read/write the desired # of bytes to the file, starting at the
	/// "current" position of the file.
	/// @param [in] fd 文件描述符
	/// @param [out] buf 存放数据的地址
	/// @param [in] numBytes 要读入的字节数
	/// @return 读入的字节数
	///
	ssize_t Read(int fd, char *buf, size_t numBytes);

	///
	/// @brief 将numBytes的数据写入chunkserver
	/// @note 不需要为数据分块，WriteToServer函数已经完成分块工作了
	///
	ssize_t Write(int fd, const char *buf, size_t numBytes);

	///
	/// @param[in] fd that corresponds to a file that was previously
	/// opened for writing.
	/// @brief 对文件fd进行数据同步
	/// @note 同步完成后，缓存中的数据也就被删除了
	///
	int Sync(int fd);

	/// @param[in] fd that corresponds to a previously opened file
	/// @param[in] offset offset to which the pointer should be moved
	/// relative to whence.
	/// @param[in] whence one of SEEK_CUR, SEEK_SET, SEEK_END
	/// @retval On success, the offset to which the filer
	/// pointer was moved to; (off_t) -1 on failure.
	/// @brief 按对应命令移动文件指针
	/// @return 返回移动之后文件指针的位置
	///
	off_t Seek(int fd, off_t offset, int whence);
	/// In this version of seek, whence == SEEK_SET
	off_t Seek(int fd, off_t offset);

	/// Return the current position of the file pointer in the file.
	/// @param[in] fd that corresponds to a previously opened file
	/// @retval value returned is analogous to calling ftell()
	/// @return 当前文件指针的位置
	///
	off_t Tell(int fd);

	///
	/// @param[in] fd that corresponds to a previously opened file
	/// @param[in] offset  the offset to which the file should be truncated
	/// @retval status code
	/// @brief 截断文件fd中从offset之后的所有数据
	///
	int Truncate(int fd, off_t offset);

	///
	/// @param[in] pathname	The full pathname of the file such as /../foo
	/// @param[in] start	The starting byte offset
	/// @param[in] len		The length in bytes that define the region
	/// @param[out] locations	The location(s) of various chunks
	/// @retval status: 0 on success; -errno otherwise
	/// @brief 根据文件名获得从start开始的len长度的数据所在的物理位置（各个块的主机名和端口）
	///
	int GetDataLocation(const char *pathname, off_t start, size_t len,
			std::vector<std::vector<std::string> > &locations);

	///
	/// @brief 根据fd获取上一个函数所需要的数据
	///
	int GetDataLocation(int fd, off_t start, size_t len, std::vector<
			std::vector<std::string> > &locations);

	///
	/// @param[in] pathname	The full pathname of the file such as /../foo
	/// @retval count
	/// @brief 查看文件pathname有多少个chunk拷贝
	///
	int16_t GetReplicationFactor(const char *pathname);

	///
	/// @param[in] pathname	The full pathname of the file such as /../foo
	/// @param[in] numReplicas  The desired degree of replication.
	/// @retval -1 on failure; on success, the # of replicas that will be made.
	/// @brief 设置文件的拷贝数目
	///
	int16_t SetReplicationFactor(const char *pathname, int16_t numReplicas);

	/// Next sequence number for operations.
	/// This is called in a thread safe manner.
	kfsSeq_t nextSeq()
	{
		return mCmdSeqNum++;
	}

private:
	/// Maximum # of files a client can have open.
	static const int MAX_FILES = 512000;

	/// Primitive support for concurrent access in the KFS client: at
	/// each entry point from the public interfaces, grab the mutex
	/// before doing any work.  This ensures that all requests to the
	/// meta/chunk servers are serialized.
	pthread_mutex_t mMutex;

	/// Seed to the random number generator
	unsigned mRandSeed;
	///
	bool mIsInitialized;
	/// where is the meta server located
	ServerLocation mMetaServerLoc;

	/// 管理lease
	LeaseClerk mLeaseClerk;

	/// a tcp socket that holds the connection with the server
	TcpSocket mMetaServerSock;
	/// seq # that we send in each command
	kfsSeq_t mCmdSeqNum;

	/// The current working directory in KFS
	std::string mCwd;

	/// 本地主机名
	std::string mHostname;

	/// keep a table of open files/directory handles.
	/// 此处存放了一个“已经打开的文件的目录”
	std::vector<FileTableEntry *> mFileTable;
	/// 此处存放一个“已经打开的目录”的缓存，<string(pathname), int(fileTable index)>
	NameToFdMap mPathCache;

	/// Check that fd is in range
	bool valid_fd(int fd)
	{
		return (fd >= 0 && fd < MAX_FILES);
	}

	/// @retval true if connect succeeds; false otherwise.
	/// @brief 连接metaserver，此时在其成员变量中已经存在一个MetaServer的socket和MetaServer\n
	/// @brief 的位置
	///
	bool ConnectToMetaServer();

	/// @param[in] parentFid  file-id of the parent directory
	/// @param[in] filename   filename whose attributes are being
	/// asked
	/// @param[out] result   the resultant attributes
	/// @param[in] computeFilesize  when set, for files, the size of
	/// the file is computed and returned in result.fileSize
	/// @retval 0 on success; -errno otherwise
	/// @brief 查询文件或目录的属性
	/// @note 若文件在“已经打开的文件列表”中，则更新它；否则，以缓存的\n
	/// @note 方式存储在“已经打开的文件列表”中
	///
	int LookupAttr(kfsFileId_t parentFid, const char *filename,
			KfsFileAttr &result, bool computeFilesize);

	/// Helper functions that operate on individual chunks.

	/// @brief 为文件fd的“当前位置”分配一个块，
	/// @param[in] fd 文件描述符
	/// @param[in] numBytes  要写入的字节数
	/// @retval 0 if successful; -errno otherwise
	/// @note 给出chunkserver的位置和端口
	///
	int AllocChunk(int fd);

	///
	/// @param[in] fd  The index from mFileTable[] that corresponds to
	/// the file being accessed
	/// @brief 打开一个chunk，即为chunk设置一个优先服务器
	/// @return 返回chunk的大小
	///
	int OpenChunk(int fd, bool nonblockingConnect = false);

	///
	/// 判断一个chunk是否可读
	/// @param fd 文件描述符
	/// @return 是否可读
	/// @note 可读条件： 如果可以定位一个chunk的物理位置，并且租约有效
	///
	bool IsChunkReadable(int fd);

	/// @retval true if our lease is good; false otherwise.
	/// @brief 判断某个租约是否过期
	/// @param [in] chunkId chunk号码
	/// @return 是否过期
	/// @note 如果租约过期，则重新申请；如果应该更新租约，则更新租约
	///
	bool IsChunkLeaseGood(kfsChunkId_t chunkId);

	/// @retval  On success, # of bytes read; -1 on error
	/// @brief 从chunk中读入numBytes个字节到buf中
	/// @param [in] fd 文件描述符
	/// @param [out] buf 存放数据的地址
	/// @param [in] numBytes 要读入的字节数
	/// @return 读入的字节数
	///
	ssize_t ReadChunk(int fd, char *buf, size_t numBytes);

	/// @retval  On success, # of bytes read; -1 on error
	/// @brief 从服务器读入数据
	/// @param [in] fd 文件描述符
	/// @param [out] buf 存放数据的地址
	/// @param [in] numBytes 要读入的字节数
	/// @return 读入的字节数
	/// @note 每次最多读入一个chunk的数据
	///
	ssize_t ReadFromServer(int fd, char *buf, size_t numBytes);

	/// @retval  On success, # of bytes read; -1 on error
	/// @brief 向metaserver发送读取命令
	/// @param [in] fd 文件描述符
	/// @param [out] buf 存放数据的地址
	/// @param [in] numBytes 要读入的字节数
	/// @return 读入的字节数
	///
	ssize_t DoSmallReadFromServer(int fd, char *buf, size_t numBytes);

	/// Helper function that breaks up a single read into a bunch of
	/// small reads and pipelines the read to reduce latency.
	/// @param [in] fd 文件描述符
	/// @param [out] buf 存放数据的地址
	/// @param [in] numBytes 要读入的字节数
	/// @return 读入的字节数
	///
	ssize_t DoLargeReadFromServer(int fd, char *buf, size_t numBytes);

	/// @retval  # of bytes copied ( value >= 0).
	/// @brief 从缓存中拷贝尽量多的数据到buf中
	/// @param [in] fd 文件描述符
	/// @param [out] buf 存放数据的地址
	/// @param [in] numBytes 要读入的字节数
	/// @return 读入的字节数
	///
	size_t CopyFromChunkBuf(int fd, char *buf, size_t numBytes);

	/// @retval  # of bytes copied ( value >= 0).
	/// @brief 零填充buf中的数据，填充的字节数为：min(文件当前指针到文件末尾的字节数，块中剩余的可\n
	/// @brief 用字节数)
	/// @brief 向metaserver发送读取命令
	/// @param [in] fd 文件描述符
	/// @param [out] buf 存放数据的地址
	/// @param [in] numBytes 要读入的字节数
	/// @return 读入的字节数
	///
	size_t ZeroFillBuf(int fd, char *buf, size_t numBytes);

	/// @retval status code: 0 on success; < 0 => failure
	/// @brief 定位一个块，返回他的所有拷贝的物理位置，存入到本地缓存中
	///
	int LocateChunk(int fd, int chunkNum);

	// Helper functions to deal with write and buffering at the client.

	/// Write the data to the chunk buffer and ack the application.
	/// This can be used for doing delayed write-backs.
	/// @param[in] fd  The file to which data is to be written
	/// @param[out] buf  The buffer with the data to be written out
	/// @param[in] numBytes  The desired # of bytes to be written
	/// @retval  # of bytes written; -1 on failure
	/// @brief 将数据写入缓冲区，如果需要则同步一些数据
	/// @note 每次最多写满一个缓冲区的数据
	///
	ssize_t WriteToBuffer(int fd, const char *buf, size_t numBytes);

	/// @param[in] fd  The file to which data is to be written
	/// @retval  # of bytes written; -1 on failure
	/// @brief 将fd指定的文件的缓存同步到服务器上，并清空该缓存
	///
	ssize_t FlushBuffer(int fd);

	/// @param[in] fd  The file to which data is to be written
	/// @param[in] offset  The offset in the chunk at which data has
	/// to be written out
	/// @param[out] buf  The buffer with the data to be written out
	/// @param[in] numBytes  The desired # of bytes to be written
	/// @retval  # of bytes written; -1 on failure
	/// @param [in] fd 文件描述符
	/// @param [in] offset 在chunk内的偏移地址
	/// @param [in] buf 要写入的数据
	/// @param [in] 要传输的数据量
	/// @note 每次写入的数据最多有一个块！！这个是由DoLargeWriteToServer这个函数决定的！\n
	/// @note 在该版本中，DoLargeWriteToServer和DoSmallWriteToServer是同一个函数
	///
	ssize_t WriteToServer(int fd, off_t offset, const char *buf,
			size_t numBytes);

	/// Helper function that does a single write op to the server.
	/// @param[in] fd  The file to which data is to be written
	/// @param[in] offset  The offset in the chunk at which data has
	/// to be written out
	/// @param[out] buf  The buffer with the data to be written out
	/// @param[in] numBytes  The desired # of bytes to be written
	/// @retval  # of bytes written; -1 on failure
	/// @note 小数据写和大数据写合并，用同一种方法
	///
	ssize_t DoSmallWriteToServer(int fd, off_t offset, const char *buf,
			size_t numBytes);

	///
	/// @brief 向文件末尾写入数据
	/// @param [in] fd 文件描述符
	/// @param [in] offset chunk内偏移
	/// @return 返回写入的字节数目
	/// @note 写入的数据只能小于或等于一个chunk的最大值
	///
	ssize_t DoLargeWriteToServer(int fd, off_t offset, const char *buf,
			size_t numBytes);

	/// @brief 为文件分配存储位置
	/// @note 若force为真，则不论该块是否已经分配，都为之分配一个块
	///
	int DoAllocation(int fd, bool force = false);

	///
	/// @brief 查看当前指针所在的数据块是否在本地有缓存
	///
	bool IsCurrChunkAttrKnown(int fd);

	/// @param[in] fd  The index in mFileTable[] that corresponds to a
	/// previously opened file.
	/// @brief 获取一个chunk的大小
	///
	int SizeChunk(int fd);

	/// @brief 计算文件的大小：先获取文件的最后一个chunk，然后通过该chunk在文件中\n
	/// @brief 的偏移位置和该chunk的大小计算出文件大小
	///
	off_t ComputeFilesize(kfsFileId_t kfsfid);

	///
	/// @brief 计算一系列文件中每一个文件的大小
	///
	void ComputeFilesizes(vector<KfsFileAttr> &fattrs,
			vector<FileChunkInfo> &lastChunkInfo);

	///
	/// @brief 计算出从startIdx开始的文件的大小
	/// @note 如果在loc上有其他文件的最后一个块，则连同那个块一起计算
	///
	void ComputeFilesizes(vector<KfsFileAttr> &fattrs,
			vector<FileChunkInfo> &lastChunkInfo, uint32_t startIdx,
			const ServerLocation &loc);

	///
	FileTableEntry *FdInfo(int fd)
	{
		return mFileTable[fd];
	}

	///
	FilePosition *FdPos(int fd)
	{
		return &FdInfo(fd)->currPos;
	}

	///
	FileAttr *FdAttr(int fd)
	{
		return &FdInfo(fd)->fattr;
	}

	///
	ChunkBuffer *FdBuffer(int fd)
	{
		return &FdInfo(fd)->buffer;
	}

	///
	ChunkAttr *GetCurrChunk(int fd)
	{
		return &FdInfo(fd)->cattr[FdPos(fd)->chunkNum];
	}

	/// @brief 对metaserver的操作
	/// @note 若不成功则会自动重试NUM_RETRIES_PER_OP次
	///
	int DoMetaOpWithRetry(KfsOp *op);

	///
	/// @brief 流水线方式请求数据，先发送一小批，然后完成一个再发送一个，知道所有的请求均发送完毕
	/// @param[in] ops 要完成的操作队列
	/// @param[in] sock 通信的server socket
	/// @return 操作结果
	/// @retval 0: 成功; 1: 失败
	///
	int DoPipelinedRead(std::vector<ReadOp *> &ops, TcpSocket *sock);

	///
	/// @brief 发送一系列数据
	/// @return 返回发送结果
	/// @note 将数据分成更小的分组，每次发送一个分组的数据，同时发送第二个分组的数据，并对前一组\n
	/// @note 进行确认; 如果一组数据失败，则所有发送均失败
	///
	int DoPipelinedWrite(int fd, std::vector<WritePrepareOp *> &ops,
			TcpSocket *masterSock);

	///
	/// @brief 为该chunk的写数据分配写序号
	/// @note 每一个chunk都要分配一个属于自己的序号，通过writeId返回
	///
	int AllocateWriteId(int fd, off_t offset, size_t numBytes, std::vector<
			WriteInfo> &writeId, TcpSocket *masterSock);

	///
	/// @brief 分别将数据发送到masterSock中
	///
	int PushData(int fd, vector<WritePrepareOp *> &ops, uint32_t start,
			uint32_t count, TcpSocket *masterSock);

	///
	/// @brief 向服务器发送写数据确认命令
	///
	int SendCommit(int fd, vector<WriteInfo> &writeId, TcpSocket *masterSock,
			WriteSyncOp &sop);

	int GetCommitReply(WriteSyncOp &sop, TcpSocket *masterSock);

	/// this is going away...
	int IssueCommit(int fd, std::vector<WriteInfo> &writeId,
			TcpSocket *masterSock);

	/// Get a response from the server, where, the response is
	/// terminated by "\r\n\r\n".
	int GetResponse(char *buf, int bufSize, int *delims, TcpSocket *sock);

	///
	/// @brief 取出文件或文件夹的父目录以及文件或文件夹名称，同时检验路径是否错误
	/// @param[in] path	The path string that needs to be extracted
	/// @param[out] parentFid  The file-id corresponding to the parent dir
	/// @param[out] name    The filename following the final "/".
	/// @retval 0 on success; -errno on failure
	///
	int GetPathComponents(const char *pathname, kfsFileId_t *parentFid,
			std::string &name);

	/// @brief 查找空闲目录表，若没有则为之分配一个
	///
	int FindFreeFileTableEntry();

	///
	/// @brief 查看当前目录项是否有效
	///
	bool IsFileTableEntryValid(int fte);

	/// @brief 根据pathname查找文件名到“已经打开的文件目录”的位置
	/// @note先从cache中查找，若失败则查找已经打开的文件列表
	///
	int LookupFileTableEntry(const char *pathname);

	/// @return 返回文件目录，paraentFid指定目录位置，name指定文件或文件夹名称
	/// @brief 在已经打开的文件目录中查找当前文件
	/// @retval 若查不到或该文件已经过期返回－1
	///
	int LookupFileTableEntry(kfsFileId_t parentFid, const char *name);

	/// @brief 查询指定文件
	/// @note 如果在目录表中，直接返回它的位置，否则为它分配一个目录
	///
	int Lookup(kfsFileId_t parentFid, const char *name);

	/// @brief 向“已经打开的文件目录”索取该文件的位置
	/// @note 若该文件不在目录中，则为之分配一个目录
	///
	int ClaimFileTableEntry(kfsFileId_t parentFid, const char *name,
			std::string pathname);

	///
	/// @brief 采用首次适应算法查找第一个空目录（使用过程中因为过期或文件关闭等清空的位置）
	/// @note 若没有空闲目录则在最后添加一个目录;
	/// @note 若目录数目已经超过最大数目，则删除其中的一个缓存;
	/// @note 若没有缓存文件，则返回错误！
	///
	int AllocFileTableEntry(kfsFileId_t parentFid, const char *name,
			std::string pathname);

	///
	/// @brief 释放“已经打开的文件目录”中的第fte个条目
	/// @note 单纯的删除就可以了
	///
	void ReleaseFileTableEntry(int fte);

	/// @brief 向metaserver获取某一个chunk的lease
	/// @note 若服务器忙，则重试（最多重试三次）
	///
	int GetLease(kfsChunkId_t chunkId);

	///
	/// @brief 更新指定chunk的lease的信息
	///
	void RenewLease(kfsChunkId_t chunkId);

	///
	/// @brief 获取校验和，存储到checksum中
	///
	bool GetDataChecksums(const ServerLocation &loc, kfsChunkId_t chunkId,
			uint32_t *checksums);

	///
	/// @brief 对文件fte的所有chunkserver上的数据进行校验，checksums提供校验和
	///
	bool VerifyDataChecksums(int fte, const vector<uint32_t> &checksums);
};

///
/// @brief 通过指定端口发送操作指令
/// @param[in] op 要进行的操作
/// @param[in] sock 要操作的服务器的地址
/// @retval 0 on success; -1 on failure
///
extern int DoOpSend(KfsOp *op, TcpSocket *sock);

///
/// @brief 在服务器的socket中寻找op对应的Response，并获取该op的状态(status)和操作内容(content)
/// @param [in] op 要接收相应的操作
/// @param [in] 服务器的socket
/// @retval 0 on success; -1 on failure
///
extern int DoOpResponse(KfsOp *op, TcpSocket *sock);

///
/// 执行命令的常规操作：创建请求，发送给服务器，获取响应，解析结果
/// @param [in] op 要接收相应的操作
/// @param [in] 服务器的socket
/// @return 读入的字节数
/// @note 完成时op中已经包含服务器给出的执行状态和必要的数据（content）
///
extern int DoOpCommon(KfsOp *op, TcpSocket *sock);

}

#endif // LIBKFSCLIENT_KFSCLIENTINT_H
