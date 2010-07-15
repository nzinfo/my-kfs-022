//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: KfsClient.cc 164 2008-09-21 20:17:57Z sriramsrao $
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
// \file KfsClient.cc
// \brief Kfs Client-library code.
//
//----------------------------------------------------------------------------

#include "KfsClient.h"
#include "KfsClientInt.h"

#include "common/config.h"
#include "common/properties.h"
#include "common/log.h"
#include "meta/kfstypes.h"
#include "libkfsIO/Checksum.h"
#include "Utils.h"

extern "C"
{
#include <signal.h>
#include <stdlib.h>
}
#include <cerrno>
#include <iostream>
#include <string>
#include <boost/scoped_array.hpp>

using std::string;
using std::ostringstream;
using std::istringstream;
using std::min;
using std::max;
using std::map;
using std::vector;
using std::sort;
using std::transform;

using std::cout;
using std::endl;

using namespace KFS;

const int CMD_BUF_SIZE = 1024;

// Set the default timeout for server I/O's to be 5 mins for now.
// This is intentionally large so that we can do stuff in gdb and not
// have the client timeout in the midst of a debug session.
struct timeval gDefaultTimeout =
{ 300, 0 };

namespace
{
// 其返回只也具有唯一性
Properties & theProps()
{
	static Properties p;
	return p;
}
}

///
/// @return KfsClientFactory的实例
/// @note 任何时刻，该函数返回的KfsClientFactory都是同一个实例
///
KfsClientFactory * KFS::getKfsClientFactory()
{
	return KfsClientFactory::Instance();
}

///
/// @param [in] 配置文件
/// @return Client实例的指针
///
KfsClientPtr KfsClientFactory::GetClient(const char *propFile)
{
	bool verbose = false;
#ifdef DEBUG
	verbose = true;
#endif
	// 如果成功，则返回0
	if (theProps().loadProperties(propFile, '=', verbose) != 0)
	{
		KfsClientPtr clnt;
		return clnt;
	}

	return GetClient(theProps().getValue("metaServer.name", ""),
			theProps().getValue("metaServer.port", -1));
}

/// 比较两个服务器是否相同。Usage: a(b);
class MatchingServer
{
	ServerLocation loc;
public:
	MatchingServer(const ServerLocation &l) :
		loc(l)
	{
	}
	bool operator()(KfsClientPtr &clnt) const
	{
		return clnt->GetMetaserverLocation() == loc;
	}
	bool operator()(const ServerLocation &other) const
	{
		return other == loc;
	}
};

///
/// @brief 从KfsClientFactory中取出指定用户，若此用户不存在，则创建此用户，并存入Factory中
/// @note 查找是否有用户，判断两个用户等价的条件：主机地址和端口地址都相同\n
/// @param metaServerHost 服务器主机名
/// @param metaServerPort 服务器端口号
/// @note 保证一个客户端中，对于同一个主机同一个端口的client只有一个
///
KfsClientPtr KfsClientFactory::GetClient(const std::string metaServerHost,
		int metaServerPort)
{
	vector<KfsClientPtr>::iterator iter;
	ServerLocation loc(metaServerHost, metaServerPort);

	iter = find_if(mClients.begin(), mClients.end(), MatchingServer(loc));
	if (iter != mClients.end())
		return *iter;

	KfsClientPtr clnt;

	// 创建一个对象，并把该对象的指针赋值给clnt.
	clnt.reset(new KfsClient());

	clnt->Init(metaServerHost, metaServerPort);
	if (clnt->IsInitialized())
		mClients.push_back(clnt);
	else
		clnt.reset();

	return clnt;
}

KfsClient::KfsClient()
{
	mImpl = new KfsClientImpl();
}

KfsClient::~KfsClient()
{
	delete mImpl;
}

int KfsClient::Init(const std::string metaServerHost, int metaServerPort)
{
	return mImpl->Init(metaServerHost, metaServerPort);
}

bool KfsClient::IsInitialized()
{
	return mImpl->IsInitialized();
}

int KfsClient::Cd(const char *pathname)
{
	return mImpl->Cd(pathname);
}

string KfsClient::GetCwd()
{
	return mImpl->GetCwd();
}

int KfsClient::Mkdirs(const char *pathname)
{
	return mImpl->Mkdirs(pathname);
}

int KfsClient::Mkdir(const char *pathname)
{
	return mImpl->Mkdir(pathname);
}

int KfsClient::Rmdir(const char *pathname)
{
	return mImpl->Rmdir(pathname);
}

int KfsClient::Rmdirs(const char *pathname)
{
	return mImpl->Rmdirs(pathname);
}

int KfsClient::Readdir(const char *pathname, std::vector<std::string> &result)
{
	return mImpl->Readdir(pathname, result);
}

int KfsClient::ReaddirPlus(const char *pathname,
		std::vector<KfsFileAttr> &result)
{
	return mImpl->ReaddirPlus(pathname, result);
}

int KfsClient::Stat(const char *pathname, struct stat &result,
		bool computeFilesize)
{
	return mImpl->Stat(pathname, result, computeFilesize);
}

bool KfsClient::Exists(const char *pathname)
{
	return mImpl->Exists(pathname);
}

bool KfsClient::IsFile(const char *pathname)
{
	return mImpl->IsFile(pathname);
}

bool KfsClient::IsDirectory(const char *pathname)
{
	return mImpl->IsDirectory(pathname);
}

int KfsClient::EnumerateBlocks(const char *pathname)
{
	return mImpl->EnumerateBlocks(pathname);
}

bool KfsClient::VerifyDataChecksums(const char *pathname,
		const vector<uint32_t> &checksums)
{
	return mImpl->VerifyDataChecksums(pathname, checksums);
}

bool KfsClient::VerifyDataChecksums(int fd, off_t offset, const char *buf,
		size_t numBytes)
{
	return mImpl->VerifyDataChecksums(fd, offset, buf, numBytes);
}

int KfsClient::Create(const char *pathname, int numReplicas, bool exclusive)
{
	return mImpl->Create(pathname, numReplicas, exclusive);
}

int KfsClient::Remove(const char *pathname)
{
	return mImpl->Remove(pathname);
}

int KfsClient::Rename(const char *oldpath, const char *newpath, bool overwrite)
{
	return mImpl->Rename(oldpath, newpath, overwrite);
}

int KfsClient::Open(const char *pathname, int openFlags, int numReplicas)
{
	return mImpl->Open(pathname, openFlags, numReplicas);
}

int KfsClient::Fileno(const char *pathname)
{
	return mImpl->Fileno(pathname);
}

int KfsClient::Close(int fd)
{
	return mImpl->Close(fd);
}

ssize_t KfsClient::Read(int fd, char *buf, size_t numBytes)
{
	return mImpl->Read(fd, buf, numBytes);
}

ssize_t KfsClient::Write(int fd, const char *buf, size_t numBytes)
{
	return mImpl->Write(fd, buf, numBytes);
}

int KfsClient::Sync(int fd)
{
	return mImpl->Sync(fd);
}

off_t KfsClient::Seek(int fd, off_t offset, int whence)
{
	return mImpl->Seek(fd, offset, whence);
}

off_t KfsClient::Seek(int fd, off_t offset)
{
	return mImpl->Seek(fd, offset, SEEK_SET);
}

off_t KfsClient::Tell(int fd)
{
	return mImpl->Tell(fd);
}

int KfsClient::Truncate(int fd, off_t offset)
{
	return mImpl->Truncate(fd, offset);
}

int KfsClient::GetDataLocation(const char *pathname, off_t start, size_t len,
		std::vector<std::vector<std::string> > &locations)
{
	return mImpl->GetDataLocation(pathname, start, len, locations);
}

int KfsClient::GetDataLocation(int fd, off_t start, size_t len, std::vector<
		std::vector<std::string> > &locations)
{
	return mImpl->GetDataLocation(fd, start, len, locations);
}

int16_t KfsClient::GetReplicationFactor(const char *pathname)
{
	return mImpl->GetReplicationFactor(pathname);
}

int16_t KfsClient::SetReplicationFactor(const char *pathname,
		int16_t numReplicas)
{
	return mImpl->SetReplicationFactor(pathname, numReplicas);
}

ServerLocation KfsClient::GetMetaserverLocation() const
{
	return mImpl->GetMetaserverLocation();
}

//
// Now, the real work is done by the impl object....
//

KfsClientImpl::KfsClientImpl()
{
	pthread_mutexattr_t mutexAttr;
	int rval;
	const int hostnamelen = 256;
	char hostname[hostnamelen];

	// 获取当前系统的主机名
	if (gethostname(hostname, hostnamelen))
	{
		perror("gethostname: ");
		exit(-1);
	}

	mHostname = hostname;

	// store the entry for "/"
	// 先打开root目录，并且这个目录从启动完成之后是一直打开的。它在“已经打开的文件目录”中的
	// 位置一定是第一个
	// 为"/"分配一个文件表，返回打开的文件在FileTable中的位置
	int UNUSED_ATTR rootfte = ClaimFileTableEntry(KFS::ROOTFID, "/", "/");
	assert(rootfte == 0);
	mFileTable[0]->fattr.fileId = KFS::ROOTFID;
	mFileTable[0]->fattr.isDirectory = true;

	// 将当前工作目录（current working directory）设置为'/'
	mCwd = "/";
	mIsInitialized = false;
	// 命令的序号从第0个开始
	mCmdSeqNum = 0;

	// Setup the mutex to allow recursive locking calls.  This
	// simplifies things when a public method (eg., read) in KFS client calls
	// another public method (eg., seek) and both need to take the lock
	rval = pthread_mutexattr_init(&mutexAttr);
	assert(rval == 0);

	// 递归属性的信号量：同一线程循环给互斥量上锁，那么系统将会知道该上锁行为来自同一线程，
	// 那么就会同意线程给该互斥量上锁，而不是形成对自身的等待，造成死锁。
	rval = pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE);
	assert(rval == 0);
	rval = pthread_mutex_init(&mMutex, &mutexAttr);
	assert(rval == 0);

	// whenever a socket goes kaput, don't crash the app
	// block some signals to avoid corruption of the application.
	signal(SIGPIPE, SIG_IGN);

	// for random # generation, seed it
	// 设置随机数发生器，可以保证每个进程的随机数产生器都不相同
	srand(getpid());
}

///
/// @brief 获取并连接metaServer
/// @return 返回操作结果
///
int KfsClientImpl::Init(const string metaServerHost, int metaServerPort)
{
	// Initialize the logger
	MsgLogger::Init(NULL);

	MsgLogger::SetLevel(log4cpp::Priority::INFO);

	mRandSeed = time(NULL);

	mMetaServerLoc.hostname = metaServerHost;
	mMetaServerLoc.port = metaServerPort;

	KFS_LOG_VA_DEBUG("Connecting to metaserver at: %s:%d",
			metaServerHost.c_str(), metaServerPort);

	if (!mMetaServerLoc.IsValid())
	{	// 全局服务器的地址不是有效的（即该地址是空的）
		mIsInitialized = false;
		KFS_LOG_VA_ERROR("Unable to connect to metaserver at: %s:%d",
				metaServerHost.c_str(), metaServerPort);
		return -1;
	}

	// 连接全局服务器
	if (!ConnectToMetaServer())
	{
		mIsInitialized = false;
		KFS_LOG_VA_ERROR("Unable to connect to metaserver at: %s:%d",
				metaServerHost.c_str(), metaServerPort);
		return -1;
	}

	mIsInitialized = true;
	return 0;
}

///
/// @brief 连接metaserver，此时在其成员变量中已经存在一个MetaServer的socket和MetaServer\n
/// @brief 的位置
///
bool KfsClientImpl::ConnectToMetaServer()
{
	return mMetaServerSock.Connect(mMetaServerLoc) >= 0;
}

///
/// @note pathname可以给定绝对路径，也可以给定相对路径
///
int KfsClientImpl::Cd(const char *pathname)
{
	MutexLock l(&mMutex);

	struct stat s;

	// 将mCwd和pathname指定的路径转换为绝对路径
	string path = build_path(mCwd, pathname);
	// 从metaServer中获取该文件的属性，即确定目录的可用性
	int status = Stat(path.c_str(), s);

	if (status < 0)
	{
		KFS_LOG_VA_DEBUG("Non-existent path: %s", pathname);
		return -ENOENT;
	}

	if (!S_ISDIR(s.st_mode))
	{
		KFS_LOG_VA_DEBUG("Non-existent dir: %s", pathname);
		return -ENOTDIR;
	}

	// strip the trailing '/'
	string::size_type pathlen = path.size();
	string::size_type rslash = path.rfind('/');
	// 删除结尾的'/'
	if (rslash + 1 == pathlen)
	{
		// path looks like: /.../; so, get rid of the last '/'
		path.erase(rslash);
	}

	mCwd = path;
	return 0;
}

///
/// To allow tools to get at "pwd"
///
string KfsClientImpl::GetCwd()
{
	return mCwd;
}

///
/// @breif 创建给定目录中的所有父级目录，pathname须给定绝对路径
/// @note pathname须给定绝对路径
///
int KfsClientImpl::Mkdirs(const char *pathname)
{
	MutexLock l(&mMutex);

	int res;
	string path = pathname;
	string component;
	const char slash = '/';
	string::size_type startPos = 1, endPos;
	bool done = false;

	//
	// Walk from the root down to the last part of the path making the
	// directory hierarchy along the way.  If any of the components of
	// the path is a file, error out.
	//
	// 检测所有路径以确认他们都不是文件
	while (!done)
	{
		endPos = path.find(slash, startPos);
		if (endPos == string::npos)
		{
			done = true;
			component = pathname;
		}
		else
		{
			component = path.substr(0, endPos);
			startPos = endPos + 1;
		}
		// 查看该文件是否存在
		if (Exists(component.c_str()))
		{
			if (IsFile(component.c_str()))
				return -ENOTDIR;
			continue;
		}

		// 创建目录
		res = Mkdir(component.c_str());
		if (res < 0)
			return res;
	}

	return 0;
}

///
/// @brief 创建一个文件夹
/// @param[in] pathname The full pathname such as /.../dir
/// @retval 0 if mkdir is successful; -errno otherwise
/// @note pathname必须是一个绝对路径
///
int KfsClientImpl::Mkdir(const char *pathname)
{
	MutexLock l(&mMutex);

	kfsFileId_t parentFid;
	string dirname;
	int res = GetPathComponents(pathname, &parentFid, dirname);
	if (res < 0)
		return res;

	MkdirOp op(nextSeq(), parentFid, dirname.c_str());
	DoMetaOpWithRetry(&op);
	if (op.status < 0)
	{
		return op.status;
	}

	// Everything is good now...
	// 目录创建完成 ，打开目录
	int fte = ClaimFileTableEntry(parentFid, dirname.c_str(), pathname);
	if (fte < 0) // Too many open files
		return -EMFILE;

	// 初始化目录，写入创建，修改时间等
	mFileTable[fte]->fattr.fileId = op.fileId;
	// setup the times and such
	mFileTable[fte]->fattr.Init(true);

	return 0;
}

///
/// @brief 删除指定目录
/// @param[in] pathname		The full pathname such as /.../dir
/// @retval 0 if rmdir is successful; -errno otherwise
/// @note 必须该出该目录的绝对路径
///
int KfsClientImpl::Rmdir(const char *pathname)
{
	MutexLock l(&mMutex);

	string dirname;
	kfsFileId_t parentFid;
	int res = GetPathComponents(pathname, &parentFid, dirname);
	if (res < 0)
		return res;

	int fte = LookupFileTableEntry(parentFid, dirname.c_str());
	if (fte > 0)
		ReleaseFileTableEntry(fte);

	RmdirOp op(nextSeq(), parentFid, dirname.c_str());
	(void) DoMetaOpWithRetry(&op);
	return op.status;
}

///
/// @brief 删除当前目录以及其中的文件和目录
/// @param[in] pathname		The full pathname such as /.../dir
/// @retval 0 if rmdir is successful; -errno otherwise
///
int KfsClientImpl::Rmdirs(const char *pathname)
{
	MutexLock l(&mMutex);

	vector<KfsFileAttr> entries;
	int res;

	// 查看当前路径下的所有文件属性
	if ((res = ReaddirPlus(pathname, entries, false)) < 0)
		return res;

	string dirname = pathname;

	int len = dirname.size();
	if (dirname[len - 1] == '/')
		dirname.erase(len - 1);

	for (size_t i = 0; i < entries.size(); i++)
	{
		if ((entries[i].filename == ".") || (entries[i].filename == ".."))
			continue;

		string d = dirname;

		d += "/" + entries[i].filename;

		// 递归删除其中的子文件夹
		if (entries[i].isDirectory)
		{
			res = Rmdirs(d.c_str());
		}
		// 删除其中的文件
		else
		{
			res = Remove(d.c_str());
		}
		if (res < 0)
			break;
	}
	if (res < 0)
		return res;

	// 删除当前目录
	res = Rmdir(pathname);

	return res;

}

///
/// @brief 读目录，返回目录中的文件名
/// @param[in] pathname	The full pathname such as /.../dir
/// @param[out] result	The filenames in the directory
/// @retval 0 if readdir is successful; -errno otherwise
///
int KfsClientImpl::Readdir(const char *pathname, vector<string> &result)
{
	MutexLock l(&mMutex);

	int fte = LookupFileTableEntry(pathname);
	if (fte < 0)
	{
		// open the directory for reading
		fte = Open(pathname, O_RDONLY);
	}

	if (fte < 0)
		return fte;

	if (!mFileTable[fte]->fattr.isDirectory)
		return -ENOTDIR;

	kfsFileId_t dirFid = mFileTable[fte]->fattr.fileId;

	ReaddirOp op(nextSeq(), dirFid);
	DoMetaOpWithRetry(&op);
	int res = op.status;
	if (res < 0)
		return res;

	istringstream ist;
	char filename[MAX_FILENAME_LEN];
	assert(op.contentBuf != NULL);
	ist.str(op.contentBuf);
	result.resize(op.numEntries);
	// 读入文件名列表，每个文件名占一行
	for (int i = 0; i < op.numEntries; ++i)
	{
		// ist >> result[i];
		ist.getline(filename, MAX_FILENAME_LEN);
		result[i] = filename;
		// KFS_LOG_VA_DEBUG("Entry: %s", filename);
	}
	sort(result.begin(), result.end());
	return res;
}

///
/// @brief
/// @param[in] pathname	The full pathname such as /.../dir
/// @param[out] result	存放目录中所有文件的属性
/// @param[in] computeFilesize 指出是否要获取文件大小信息
/// @retval 0 if readdir is successful; -errno otherwise
/// @note pathname是文件的绝对路径
///
int KfsClientImpl::ReaddirPlus(const char *pathname,
		vector<KfsFileAttr> &result, bool computeFilesize)
{
	MutexLock l(&mMutex);

	int fte = LookupFileTableEntry(pathname);

	if (fte < 0) // open the directory for reading
		fte = Open(pathname, O_RDONLY);
	if (fte < 0)
		return fte;

	FileAttr *fa = FdAttr(fte);
	if (!fa->isDirectory)
		return -ENOTDIR;

	kfsFileId_t dirFid = fa->fileId;

	// 向全局服务器发送请求命令
	ReaddirPlusOp op(nextSeq(), dirFid);
	(void) DoMetaOpWithRetry(&op);
	int res = op.status;
	if (res < 0)
	{
		return res;
	}

	// 从服务器的响应中解析所需要的数据
	vector<FileChunkInfo> fileChunkInfo;	// 每一个文件的最后一个块的信息
	istringstream ist;
	string entryInfo;	// 存放目录信息，每个目录存放一行
	boost::scoped_array<char> line;
	int count = 0, linelen = 1 << 20, numchars;
	const string entryDelim = "Begin-entry";
	string s(op.contentBuf, op.contentLength);

	// ist存放了响应内容的一个副本
	ist.str(s);

	KFS_LOG_VA_DEBUG("# of entries: %d", op.numEntries);

	// 申请1MB的空间
	line.reset(new char[linelen]);

	// the format is:
	// Begin-entry <values> Begin-entry <values>
	// the last entry doesn't have a end-marker
	// 逐个获取每一个文件的信息，并且存储到fileChunkInfo中
	// result中存放文件属性，fileChunkInfo中存储文件最后一个chunk的信息，两者一一对应
	while (count < op.numEntries)
	{
		// 从ist中取出目录信息，并解析出来存放到entryInfo中
		ist.getline(line.get(), linelen);

		numchars = ist.gcount();
		if (numchars != 0)
		{
			if (line[numchars - 2] == '\r')
				line[numchars - 2] = '\0';

			KFS_LOG_VA_DEBUG("entry: %s", line.get());

			// const string entryDelim = "Begin-entry";
			if (line.get() != entryDelim)
			{
				entryInfo += line.get();
				entryInfo += "\r\n";
				continue;
			}
			// we hit a delimiter; if this is the first one, we
			// continue so that we can build up the key/value pairs
			// for the entry.
			if (entryInfo == "")
				continue;
		}
		count++;
		// sanity
		if (entryInfo == "")
			continue;

		// previous entry is all done...process it
		Properties prop;
		KfsFileAttr fattr;
		string s;
		istringstream parserStream(entryInfo);

		// 每一行信息中包含文件的全部信息
		const char separator = ':';

		// 文件信息列表如下：
		prop.loadProperties(parserStream, separator, false);
		fattr.filename = prop.getValue("Name", "");
		fattr.fileId = prop.getValue("File-handle", -1);
		s = prop.getValue("Type", "");
		fattr.isDirectory = (s == "dir");

		s = prop.getValue("M-Time", "");
		GetTimeval(s, fattr.mtime);

		s = prop.getValue("C-Time", "");
		GetTimeval(s, fattr.ctime);

		s = prop.getValue("CR-Time", "");
		GetTimeval(s, fattr.crtime);

		entryInfo = "";

		fattr.numReplicas = prop.getValue("Replication", 1);
		fattr.fileSize = prop.getValue("File-size", (off_t) -1);
		if (fattr.fileSize != -1)
		{
			KFS_LOG_VA_DEBUG("Got file size from server for %s: %lld",
					fattr.filename.c_str(), fattr.fileSize);
		}

		// get the location info for the last chunk
		// 获取文件最后一个块的信息
		FileChunkInfo lastChunkInfo(fattr.filename, fattr.fileId);

		lastChunkInfo.lastChunkOffset = prop.getValue("Chunk-offset", 0);
		lastChunkInfo.chunkCount = prop.getValue("Chunk-count", 0);
		lastChunkInfo.numReplicas = prop.getValue("Replication", 1);
		lastChunkInfo.cattr.chunkId = prop.getValue("Chunk-handle",
				(kfsFileId_t) -1);
		lastChunkInfo.cattr.chunkVersion = prop.getValue("Chunk-version",
				(int64_t) -1);

		int numReplicas = prop.getValue("Num-replicas", 0);
		// 其中存放的是每一个chunkserver的主机名和端口号
		string replicas = prop.getValue("Replicas", "");

		// 解析chunkserver的信息并存储到lastChunkInfo中
		if (replicas != "")
		{
			istringstream ser(replicas);
			ServerLocation loc;

			for (int i = 0; i < numReplicas; ++i)
			{
				ser >> loc.hostname;
				ser >> loc.port;
				lastChunkInfo.cattr.chunkServerLoc.push_back(loc);
			}
		}
		fileChunkInfo.push_back(lastChunkInfo);
		result.push_back(fattr);
	}

	// 计算文件大小，如果要求
	if (computeFilesize)
	{
		for (uint32_t i = 0; i < result.size(); i++)
		{
			// 如果是空文件或者是目录
			if ((fileChunkInfo[i].chunkCount == 0) || (result[i].isDirectory))
			{
				result[i].fileSize = 0;
				continue;
			}

			// 若文件已经打开，则获取其在“已经打开的文件目录”中的位置
			int fte = LookupFileTableEntry(dirFid, result[i].filename.c_str());

			if (fte >= 0)
			{
				result[i].fileSize = mFileTable[fte]->fattr.fileSize;
			}
		}
		// 计算所有文件的大小
		ComputeFilesizes(result, fileChunkInfo);

		for (uint32_t i = 0; i < result.size(); i++)
			if (result[i].fileSize < 0)
				result[i].fileSize = 0;
	}

	// if there are too many entries in the dir, then the caller is
	// probably scanning the directory.  don't put it in the cache
	if (result.size() > 128)
	{
		sort(result.begin(), result.end());
		return res;
	}

	string dirname = build_path(mCwd, pathname);
	string::size_type len = dirname.size();
	if ((len > 0) && (dirname[len - 1] == '/'))
		dirname.erase(len - 1);

	for (uint32_t i = 0; i < result.size(); i++)
	{
		int fte = LookupFileTableEntry(dirFid, result[i].filename.c_str());

		// 如果文件已经打开或者缓存，则更新其中的文件大小信息
		if (fte >= 0)
		{
			// if we computed the filesize, then we stash it; otherwise, we'll
			// set the value to -1 and force a recompute later...
			mFileTable[fte]->fattr.fileSize = result[i].fileSize;
			continue;
		}

		if (fte < 0)
		{
			string fullpath;
			if ((result[i].filename == ".") || (result[i].filename == ".."))
				fullpath = "";
			else
				fullpath = dirname + "/" + result[i].filename;

			// 为文件分配缓存
			fte = AllocFileTableEntry(dirFid, result[i].filename.c_str(),
					fullpath);
			if (fte < 0)
				continue;
		}

		mFileTable[fte]->fattr.fileId = result[i].fileId;
		mFileTable[fte]->fattr.mtime = result[i].mtime;
		mFileTable[fte]->fattr.ctime = result[i].ctime;
		mFileTable[fte]->fattr.ctime = result[i].crtime;
		mFileTable[fte]->fattr.isDirectory = result[i].isDirectory;
		mFileTable[fte]->fattr.chunkCount = fileChunkInfo[i].chunkCount;
		mFileTable[fte]->fattr.numReplicas = fileChunkInfo[i].numReplicas;

		mFileTable[fte]->openMode = 0;
		// if we computed the filesize, then we stash it; otherwise, we'll
		// set the value to -1 and force a recompute later...
		mFileTable[fte]->fattr.fileSize = result[i].fileSize;
	}

	sort(result.begin(), result.end());

	return res;
}

///
/// @brief 获取文件的属性
/// @param [in] pathname 文件或文件夹路径
/// @param [out] result 获取到的文件属性
///
int KfsClientImpl::Stat(const char *pathname, struct stat &result,
		bool computeFilesize)
{
	MutexLock l(&mMutex);

	KfsFileAttr kfsattr;

	// 查找指定的路径是否已经打开，并返回其在文件表中的位置（若查找失败，则返回-1）
	int fte = LookupFileTableEntry(pathname);

	if (fte >= 0)
	{	// 如果查找成功，则直接获取文件属性
		kfsattr = mFileTable[fte]->fattr;
	}

	// either we don't have the attributes cached or it is a file and
	// we are asked to compute the size and we don't know the size,
	// lookup the attributes
	if ((fte < 0) || ((!kfsattr.isDirectory) && computeFilesize
			&& (kfsattr.fileSize < 0)))
	{
		kfsFileId_t parentFid;
		string filename;
		// 获取该文件（或目录）的父亲目录和文件名（或目录名）
		int res = GetPathComponents(pathname, &parentFid, filename);

		// 获取文件或目录属性
		if (res == 0)
			res = LookupAttr(parentFid, filename.c_str(), kfsattr,
					computeFilesize);
		if (res < 0)
			return res;
	}

	// 存放文件属性，用于返回
	memset(&result, 0, sizeof(struct stat));
	result.st_mode = kfsattr.isDirectory ? S_IFDIR : S_IFREG;
	result.st_size = kfsattr.fileSize;
	result.st_atime = kfsattr.crtime.tv_sec;
	result.st_mtime = kfsattr.mtime.tv_sec;
	result.st_ctime = kfsattr.ctime.tv_sec;
	return 0;
}

///
/// @brief 通过查看文件属性来判断该文件是否存在
///
bool KfsClientImpl::Exists(const char *pathname)
{
	MutexLock l(&mMutex);

	struct stat dummy;

	return Stat(pathname, dummy, false) == 0;
}

///
/// @brief 通过获取文件的属性来判断所查看的pathname是否是文件
///
bool KfsClientImpl::IsFile(const char *pathname)
{
	MutexLock l(&mMutex);

	struct stat statInfo;

	// 若程序从这里返回，则说明该文件不存在
	if (Stat(pathname, statInfo, false) != 0)
		return false;

	// 通过返回属性来判断是否是文件
	return S_ISREG(statInfo.st_mode);
}

///
/// @brief 通过Stat查看某路径是否为目录
///
bool KfsClientImpl::IsDirectory(const char *pathname)
{
	MutexLock l(&mMutex);

	struct stat statInfo;

	// 通过Stat函数返回是否是目录：Stat要求pathname是目录，否则则出错
	if (Stat(pathname, statInfo, false) != 0)
		return false;

	return S_ISDIR(statInfo.st_mode);
}

///
/// @brief 查询文件或目录的属性
/// @note 若文件在“已经打开的文件列表”中，则更新它；否则，以缓存的\n
/// @note 方式存储在“已经打开的文件列表”中
///
int KfsClientImpl::LookupAttr(kfsFileId_t parentFid, const char *filename,
		KfsFileAttr &result, bool computeFilesize)
{
	MutexLock l(&mMutex);

	if (parentFid < 0)
		return -EINVAL;

	int fte = LookupFileTableEntry(parentFid, filename);
	// 根据文件路径和名称创建一个查询类
	LookupOp op(nextSeq(), parentFid, filename);

	if (fte >= 0)
	{	// 如果在“已经打开的文件目录”中存在该条目，则直接取出结果放到result中
		result = mFileTable[fte]->fattr;
	}
	else
	{	// 否则执行查询操作
		(void) DoMetaOpWithRetry(&op);
		if (op.status < 0)
			return op.status;
		// 并将查询结果放到result中
		result = op.fattr;
	}

	// 如果要查询的不是目录，并且要求计算文件大小，则计算文件大小，放到result中
	if ((!result.isDirectory) && computeFilesize)
	{
		if (result.fileSize < 0)
		{
			result.fileSize = ComputeFilesize(result.fileId);
		}
	}
	else
	{
		// 否则该目录项无效
		result.fileSize = -1;
	}

	if (fte >= 0)
	{
		// if we computed the filesize, then we stash it; otherwise, we'll
		// set the value to -1 and force a recompute later...
		// 既然已经计算出来了，那就存上呗；若没有计算出来，则强制置为-1，要求晚些计算
		mFileTable[fte]->fattr.fileSize = result.fileSize;
		return 0;
	}

	// cache the entry if possible
	// 将这次查询放入缓存，因为要用到的可能性很大

	fte = AllocFileTableEntry(parentFid, filename, "");
	if (fte < 0)
		return op.status;

	mFileTable[fte]->fattr = op.fattr;
	mFileTable[fte]->openMode = 0;	// 模式为0，表示为缓存
	// if we computed the filesize, then we stash it; otherwise, we'll
	// set the value to -1 and force a recompute later...
	mFileTable[fte]->fattr.fileSize = result.fileSize;

	return op.status;
}

///
/// @brief 创建并打开新建的文件
/// @note 打开模式为 可读可写, 路径须为绝对路径
///
int KfsClientImpl::Create(const char *pathname, int numReplicas, bool exclusive)
{
	MutexLock l(&mMutex);

	kfsFileId_t parentFid;
	string filename;
	int res = GetPathComponents(pathname, &parentFid, filename);
	if (res < 0)
	{
		KFS_LOG_VA_DEBUG("status %d for pathname %s", res, pathname);
		return res;
	}

	if (filename.size() >= MAX_FILENAME_LEN)
		return -ENAMETOOLONG;

	CreateOp op(nextSeq(), parentFid, filename.c_str(), numReplicas, exclusive);
	(void) DoMetaOpWithRetry(&op);
	if (op.status < 0)
	{
		KFS_LOG_VA_DEBUG("status %d from create RPC", op.status);
		return op.status;
	}

	// Everything is good now...
	// 打开文件
	int fte = ClaimFileTableEntry(parentFid, filename.c_str(), pathname);
	if (fte < 0)
	{ // XXX Too many open files
		KFS_LOG_VA_DEBUG("status %d from ClaimFileTableEntry", fte);
		return fte;
	}

	FileAttr *fa = FdAttr(fte);
	fa->fileId = op.fileId;
	fa->Init(false); // is an ordinary file

	FdInfo(fte)->openMode = O_RDWR;

	return fte;
}

///
/// @brief 删除文件
/// @note 之前先清除该文件在“已经打开的文件列表”中的目录
///
int KfsClientImpl::Remove(const char *pathname)
{
	MutexLock l(&mMutex);

	kfsFileId_t parentFid;
	string filename;
	int res = GetPathComponents(pathname, &parentFid, filename);
	if (res < 0)
		return res;

	// 先从“已经打开的文件列表”中删除该文件的目录，然后再发送删除命令
	int fte = LookupFileTableEntry(parentFid, filename.c_str());
	if (fte > 0)
		ReleaseFileTableEntry(fte);

	RemoveOp op(nextSeq(), parentFid, filename.c_str());
	(void) DoMetaOpWithRetry(&op);
	return op.status;
}

///
/// @brief 给一个文件重命名
/// @note 先发送重命名指令，然后再更新文件在本地的缓存
///
int KfsClientImpl::Rename(const char *oldpath, const char *newpath,
		bool overwrite)
{
	MutexLock l(&mMutex);

	kfsFileId_t parentFid;
	string oldfilename;
	int res = GetPathComponents(oldpath, &parentFid, oldfilename);
	if (res < 0)
		return res;

	string absNewpath = build_path(mCwd, newpath);
	RenameOp op(nextSeq(), parentFid, oldfilename.c_str(), absNewpath.c_str(),
			overwrite);
	(void) DoMetaOpWithRetry(&op);

	KFS_LOG_VA_DEBUG("Status of renaming %s -> %s is: %d",
			oldpath, newpath, op.status);

	// update the path cache
	if (op.status == 0)
	{	// 重命名完成，如果该文件已经打开，则更新该文件的缓存和“已经打开的文件目录“
		int fte = LookupFileTableEntry(parentFid, oldfilename.c_str());
		if (fte > 0)
		{
			string oldn = string(oldpath);
			NameToFdMapIter iter = mPathCache.find(oldn);

			if (iter != mPathCache.end())
				mPathCache.erase(iter);

			mPathCache[absNewpath] = fte;
			mFileTable[fte]->pathname = absNewpath;
		}
	}

	return op.status;
}

///
/// @brief 查找指定文件名在“已经打开的文件目录”中的位置
///
int KfsClientImpl::Fileno(const char *pathname)
{
	kfsFileId_t parentFid;
	string filename;
	int res = GetPathComponents(pathname, &parentFid, filename);
	if (res < 0)
		return res;

	return LookupFileTableEntry(parentFid, filename.c_str());
}

///
/// @brief 根据不同的打开模式打开文件，为之分配一个目录并且获取该文件的基本属性和大小属性
///
int KfsClientImpl::Open(const char *pathname, int openMode, int numReplicas)
{
	MutexLock l(&mMutex);

	kfsFileId_t parentFid;
	string filename;
	int res = GetPathComponents(pathname, &parentFid, filename);
	if (res < 0)
		return res;

	if (filename.size() >= MAX_FILENAME_LEN)
		return -ENAMETOOLONG;

	LookupOp op(nextSeq(), parentFid, filename.c_str());
	(void) DoMetaOpWithRetry(&op);

	// 如果查询出错，则可能是文件不存在或者真是出错
	if (op.status < 0)
	{
		if (openMode & O_CREAT)
		{
			// file doesn't exist.  Create it
			return Create(pathname, numReplicas, openMode & O_EXCL);
		}
		return op.status;
	}
	else
	{
		// 通常只有创建文件时才允许独占模式，若文件已存在，则总是失败
		// file exists; now fail open if: O_CREAT | O_EXCL
		if ((openMode & (O_CREAT|O_EXCL)) == (O_CREAT|O_EXCL))
			return -EEXIST;
	}

	// 为文件在“已经打开的文件列表”中分配一个目录，若文件已经在目录中，则返回目录下标
	int fte = AllocFileTableEntry(parentFid, filename.c_str(), pathname);
	if (fte < 0) // Too many open files
		return fte;

	// 可读可写模式
	if (openMode & O_RDWR)
		mFileTable[fte]->openMode = O_RDWR;
	// 只写模式
	else if (openMode & O_WRONLY)
		mFileTable[fte]->openMode = O_WRONLY;
	// 只读模式
	else if (openMode & O_RDONLY)
		mFileTable[fte]->openMode = O_RDONLY;
	// 用于缓存
	else
		mFileTable[fte]->openMode = 0;

	// We got a path...get the fattr
	mFileTable[fte]->fattr = op.fattr;

	if (mFileTable[fte]->fattr.chunkCount > 0)
	{
		mFileTable[fte]->fattr.fileSize = ComputeFilesize(op.fattr.fileId);
	}

	// 若次文件已经存在，则清空该文件
	if (openMode & O_TRUNC)
		Truncate(fte, 0);

	// 移动文件指针到文件末尾
	if (openMode & O_APPEND)
		Seek(fte, 0, SEEK_END);

	return fte;
}

///
/// @brief 关闭一个文件
/// @note 如果该文件的数据已经更新，则同步该文件\n
/// @note 这个操作不清除缓存中的文件信息
///
int KfsClientImpl::Close(int fd)
{
	MutexLock l(&mMutex);
	int status = 0;

	if ((!valid_fd(fd)) || (mFileTable[fd] == NULL))
		return -EBADF;

	if (mFileTable[fd]->buffer.dirty)
	{
		status = FlushBuffer(fd);
	}
	ReleaseFileTableEntry(fd);
	return status;
}

///
/// @brief 对文件fd进行数据同步
/// @note 同步完成后，缓存中的数据也就被删除了
///
int KfsClientImpl::Sync(int fd)
{
	MutexLock l(&mMutex);

	if (!valid_fd(fd))
		return -EBADF;

	if (mFileTable[fd]->buffer.dirty)
	{
		int status = FlushBuffer(fd);
		if (status < 0)
			return status;
	}
	return 0;
}

///
/// @brief 截断文件fd中从offset之后的所有数据
///
int KfsClientImpl::Truncate(int fd, off_t offset)
{
	MutexLock l(&mMutex);

	if (!valid_fd(fd))
		return -EBADF;

	// for truncation, file should be opened for writing
	if (mFileTable[fd]->openMode == O_RDONLY)
		return -EBADF;

	// 获取文件中当前正在处理的chunk的缓存
	ChunkBuffer *cb = FdBuffer(fd);
	if (cb->dirty)
	{	// 如果该缓存被修改过，则同步服务器
		int res = FlushBuffer(fd);
		if (res < 0)
			return res;
	}

	// invalidate buffer in case it is past new EOF
	// 释放缓存中的所有数据
	cb->invalidate();
	FilePosition *pos = FdPos(fd);
	// 断开同chunkserver的连接
	pos->ResetServers();

	FileAttr *fa = FdAttr(fd);
	// 截去文件从offset开始之后的所有数据
	TruncateOp op(nextSeq(), fa->fileId, offset);
	(void) DoMetaOpWithRetry(&op);
	int res = op.status;

	if (res == 0)
	{
		fa->fileSize = offset;
		if (fa->fileSize == 0)
			fa->chunkCount = 0;
		// else
		// chunkcount is off...but, that is ok; it is never exposed to
		// the end-client.

		gettimeofday(&fa->mtime, NULL);
		// force a re-lookup of locations
		FdInfo(fd)->cattr.clear();
	}
	return res;
}

///
/// @brief 根据文件名获得从start开始的len长度的数据所在的物理位置（各个块的主机名和端口）
///
int KfsClientImpl::GetDataLocation(const char *pathname, off_t start,
		size_t len, vector<vector<string> > &locations)
{
	MutexLock l(&mMutex);

	int fd;

	// Non-existent
	if (!IsFile(pathname))
		return -ENOENT;

	// load up the fte
	fd = LookupFileTableEntry(pathname);

	// 如果在目录中没有，则需要打开文件
	if (fd < 0)
	{
		// Open the file and cache the attributes
		fd = Open(pathname, 0);
		// we got too many open files?
		if (fd < 0)
			return fd;
	}

	return GetDataLocation(fd, start, len, locations);
}

///
/// @brief 根据fd获取上一个函数所需要的数据
///
int KfsClientImpl::GetDataLocation(int fd, off_t start, size_t len, vector<
		vector<string> > &locations)
{
	MutexLock l(&mMutex);

	int res;
	// locate each chunk and get the hosts that are storing the chunk.
	for (size_t pos = start; pos < start + len; pos += KFS::CHUNKSIZE)
	{
		ChunkAttr *chunkAttr;
		int chunkNum = pos / KFS::CHUNKSIZE;

		if ((res = LocateChunk(fd, chunkNum)) < 0)
		{
			return res;
		}

		chunkAttr = &(mFileTable[fd]->cattr[chunkNum]);

		vector<string> hosts;
		for (vector<string>::size_type i = 0; i
				< chunkAttr->chunkServerLoc.size(); i++)
			hosts.push_back(chunkAttr->chunkServerLoc[i].hostname);

		locations.push_back(hosts);
	}

	return 0;
}

///
/// @brief 查看文件pathname有多少个chunk拷贝
///
int16_t KfsClientImpl::GetReplicationFactor(const char *pathname)
{
	MutexLock l(&mMutex);

	int fd;

	// Non-existent
	if (!IsFile(pathname))
		return -ENOENT;

	// load up the fte
	fd = LookupFileTableEntry(pathname);
	if (fd < 0)
	{
		// Open the file for reading...this'll get the attributes setup
		fd = Open(pathname, 0);
		// we got too many open files?
		if (fd < 0)
			return fd;
	}
	return mFileTable[fd]->fattr.numReplicas;
}

///
/// @brief 设置文件的拷贝数目
///
int16_t KfsClientImpl::SetReplicationFactor(const char *pathname,
		int16_t numReplicas)
{
	MutexLock l(&mMutex);

	int res, fd;

	// Non-existent
	if (!IsFile(pathname))
		return -ENOENT;

	// load up the fte
	fd = LookupFileTableEntry(pathname);
	if (fd < 0)
	{
		// Open the file and get the attributes cached
		fd = Open(pathname, 0);
		// we got too many open files?
		if (fd < 0)
			return fd;
	}
	ChangeFileReplicationOp op(nextSeq(), FdAttr(fd)->fileId, numReplicas);
	(void) DoMetaOpWithRetry(&op);

	if (op.status == 0)
	{
		FdAttr(fd)->numReplicas = op.numReplicas;
		res = op.numReplicas;
	}
	else
		res = op.status;

	return res;
}

off_t KfsClientImpl::Seek(int fd, off_t offset)
{
	return Seek(fd, offset, SEEK_SET);
}

///
/// @brief 按对应命令移动文件指针
/// @return 返回移动之后文件指针的位置
///
off_t KfsClientImpl::Seek(int fd, off_t offset, int whence)
{
	MutexLock l(&mMutex);

	if (!valid_fd(fd) || mFileTable[fd]->fattr.isDirectory)
		return (off_t) -EBADF;

	FilePosition *pos = FdPos(fd);
	off_t newOff;
	switch (whence)
	{
	// 将文件指针设置为offset
	case SEEK_SET:
		newOff = offset;
		break;
	// 将文件指针向后移动offset
	case SEEK_CUR:
		newOff = pos->fileOffset + offset;
		break;
	// 将文件指针设置为：末尾+offset
	case SEEK_END:
		newOff = mFileTable[fd]->fattr.fileSize + offset;
		break;
	default:
		return (off_t) -EINVAL;
	}

	// 改变当前chunkNum
	int32_t chunkNum = newOff / KFS::CHUNKSIZE;
	// If we are changing chunks, we need to reset the socket so that
	// it eventually points to the right place
	if (pos->chunkNum != chunkNum)
	{	// 如果chunk号有变动，则将chunk缓存写回服务器，并关闭同chunk服务器的所有连接
		ChunkBuffer *cb = FdBuffer(fd);
		if (cb->dirty)
		{
			FlushBuffer(fd);
		}
		assert(!cb->dirty);
		// better to panic than silently lose a write
		if (cb->dirty)
		{
			KFS_LOG_ERROR("Unable to flush data to server...aborting");
			abort();
		}
		// Disconnect from all the servers we were connected for this chunk
		pos->ResetServers();
	}

	pos->fileOffset = newOff;
	pos->chunkNum = chunkNum;
	pos->chunkOffset = newOff % KFS::CHUNKSIZE;

	return newOff;
}

///
/// @return 当前文件指针的位置
///
off_t KfsClientImpl::Tell(int fd)
{
	MutexLock l(&mMutex);

	return mFileTable[fd]->currPos.fileOffset;
}

///
/// @brief 为文件fd的“当前位置”分配一个块，
/// @param[in] fd 文件描述符
/// @param[in] numBytes  要写入的字节数
/// @retval 0 if successful; -errno otherwise
/// @note 给出chunkserver的位置和端口
///
int KfsClientImpl::AllocChunk(int fd)
{
	FileAttr *fa = FdAttr(fd);
	assert(valid_fd(fd) && !fa->isDirectory);

	struct timeval startTime, endTime;
	double timeTaken;

	gettimeofday(&startTime, NULL);

	AllocateOp op(nextSeq(), fa->fileId);
	FilePosition *pos = FdPos(fd);
	// fileOffset指定块的起始地址
	op.fileOffset = ((pos->fileOffset / KFS::CHUNKSIZE) * KFS::CHUNKSIZE);

	// 操作返回：chunk号，版本号，各个服务器地址
	(void) DoMetaOpWithRetry(&op);
	if (op.status < 0)
	{
		// KFS_LOG_VA_DEBUG("AllocChunk(%d)", op.status);
		return op.status;
	}
	ChunkAttr chunk;
	chunk.chunkId = op.chunkId;
	chunk.chunkVersion = op.chunkVersion;
	chunk.chunkServerLoc = op.chunkServers;
	FdInfo(fd)->cattr[pos->chunkNum] = chunk;

	FdPos(fd)->ResetServers();
	// for writes, [0] is the master; that is the preferred server
	if (op.chunkServers.size() > 0)
	{
		FdPos(fd)->SetPreferredServer(op.chunkServers[0]);
		SizeChunk(fd);
	}

	KFS_LOG_VA_DEBUG("Fileid: %lld, chunk : %lld, version: %lld, hosted on:",
			fa->fileId, chunk.chunkId, chunk.chunkVersion);

	for (uint32_t i = 0; i < op.chunkServers.size(); i++)
	{
		KFS_LOG_VA_DEBUG("%s", op.chunkServers[i].ToString().c_str());
	}

	gettimeofday(&endTime, NULL);

	timeTaken = (endTime.tv_sec - startTime.tv_sec) + (endTime.tv_usec
			- startTime.tv_usec) * 1e-6;

	KFS_LOG_VA_DEBUG("Total Time to allocate chunk: %.4f secs", timeTaken);

	return op.status;
}

///
/// @brief 定位一个块，返回他的所有拷贝的物理位置，存入到本地缓存中
///
int KfsClientImpl::LocateChunk(int fd, int chunkNum)
{
	assert(valid_fd(fd) && !mFileTable[fd]->fattr.isDirectory);

	if (chunkNum < 0)
		return -EINVAL;

	map<int, ChunkAttr>::iterator c;
	c = mFileTable[fd]->cattr.find(chunkNum);

	// Avoid unnecessary look ups.
	// 如果在目录中有这个块的信息，那么就不用查找了
	if (c != mFileTable[fd]->cattr.end() && c->second.chunkId > 0)
		return 0;

	// 向服务器请求块信息
	GetAllocOp op(nextSeq(), mFileTable[fd]->fattr.fileId, (off_t) chunkNum
			* KFS::CHUNKSIZE);
	(void) DoMetaOpWithRetry(&op);
	if (op.status < 0)
	{
		string errstr = ErrorCodeToStr(op.status);
		KFS_LOG_VA_DEBUG("LocateChunk (%d): %s", op.status, errstr.c_str());
		return op.status;
	}

	ChunkAttr chunk;
	chunk.chunkId = op.chunkId;
	chunk.chunkVersion = op.chunkVersion;
	chunk.chunkServerLoc = op.chunkServers;
	mFileTable[fd]->cattr[chunkNum] = chunk;

	return 0;
}

///
/// @brief 查看当前指针所在的数据块是否在本地有缓存
///
bool KfsClientImpl::IsCurrChunkAttrKnown(int fd)
{
	map<int, ChunkAttr> *c = &FdInfo(fd)->cattr;
	return c->find(FdPos(fd)->chunkNum) != c->end();
}

///
/// @brief 通过指定端口发送操作指令
/// @param[in] op 要进行的操作
/// @param[in] sock 要操作的服务器的地址
/// @retval 0 on success; -1 on failure
///
int KFS::DoOpSend(KfsOp *op, TcpSocket *sock)
{
	ostringstream os;

	if ((sock == NULL) || (!sock->IsGood()))
	{
		// KFS_LOG_VA_DEBUG("Trying to do I/O on a closed socket..failing it");
		op->status = -EHOSTUNREACH;
		return -1;
	}

	// 用os存储op命令, 用于网络传输
	op->Request(os);

	cout << os.str() << endl;

	// 1. 发送命令
	int numIO = sock->DoSynchSend(os.str().c_str(), os.str().length());
	if (numIO <= 0)
	{	// 发送的字节数小于0
		sock->Close();
		KFS_LOG_DEBUG("Send failed...closing socket");
		op->status = -EHOSTUNREACH;
		return -1;
	}

	cout << "Content Length: " << op->contentLength << endl;
	// 2. 发送内容
	if (op->contentLength > 0)
	{
		numIO = sock->DoSynchSend(op->contentBuf, op->contentLength);
		if (numIO <= 0)
		{
			sock->Close();
			KFS_LOG_DEBUG("Send failed...closing socket");
			op->status = -EHOSTUNREACH;
			return -1;
		}
	}
	return 0;
}

///
/// @brief 获取服务器的响应，并且判断该响应是否以"\r\n\r\n"结尾
/// @param [in/out] buf 要发送或者接收数据的地址
/// @param [in] bufSize 缓存大小
/// @param [out] "\r\n\r\n"的位置
/// @param [in] sock 服务器的socket
/// @note 若不是以"\r\n\r\n"结尾，则delims为-1
///
static int GetResponse(char *buf, int bufSize, int *delims, TcpSocket *sock)
{
	*delims = -1;

	while (1)
	{
		struct timeval timeout = gDefaultTimeout;

		// 将数据放入buf中，同时不清除socket中的数据
		int nread = sock->DoSynchPeek(buf, bufSize, timeout);
		if (nread <= 0)
			return nread;

		// 寻找结尾符号："\r\n\r\n"
		for (int i = 4; i <= nread; i++)
		{
			// 会因为多线程出现问题？
			if (i < 4)
				break;
			if ((buf[i - 3] == '\r') && (buf[i - 2] == '\n') && (buf[i - 1]
					== '\r') && (buf[i] == '\n'))
			{
				// valid stuff is from 0..i; so, length of resulting
				// string is i+1.
				memset(buf, '\0', bufSize);
				*delims = (i + 1);
				nread = sock->Recv(buf, *delims);
				return nread;
			}
		}
	}
	return -ENOBUFS;
}

///
/// @brief 从相应用解析出命令序号和内容长度
/// @note 序号和内容长度都是作为Properties的一个成员变量
///
static void GetSeqContentLen(const char *resp, int respLen, kfsSeq_t *seq,
		int *contentLength)
{
	string respStr(resp, respLen);
	Properties prop;
	istringstream ist(respStr);
	const char separator = ':';

	prop.loadProperties(ist, separator, false);
	*seq = prop.getValue("Cseq", (kfsSeq_t) -1);
	*contentLength = prop.getValue("Content-length", 0);
}

///
/// 在服务器的socket中寻找op对应的Response，并获取该op的状态(status)和操作内容(content)
/// @param [in] op 要接收相应的操作
/// @param [in] 服务器的socket
/// @retval 0 on success; -1 on failure
///
int KFS::DoOpResponse(KfsOp *op, TcpSocket *sock)
{
	int numIO;
	char buf[CMD_BUF_SIZE];
	int nread = 0, len;
	ssize_t navail, nleft;
	kfsSeq_t resSeq;
	int contentLen;
	bool printMatchingResponse = false;

	if ((sock == NULL) || (!sock->IsGood()))
	{
		op->status = -EHOSTUNREACH;
		KFS_LOG_DEBUG("Trying to do I/O on a closed socket..failing it");
		return -1;
	}

	// 无条件循环，直到找到了于op相对应的命令序号（即对应于该命令的响应）
	while (1)
	{
		memset(buf, '\0', CMD_BUF_SIZE);

		// 获取返回信息。其中buf存储返回的信息，len指定返回命令的长度，numIO为读入的字节数
		// buf中存放的是整个命令的数据，包含header和content
		numIO = GetResponse(buf, CMD_BUF_SIZE, &len, sock);
		assert(numIO != -ENOBUFS);

		// 若numIO<＝0，则操作出错，分别处理，之后退出该函数
		if (numIO <= 0)
		{
			if (numIO == -ENOBUFS)
			{	// 若读入数目为-ENOBUFS，则操作状态出错，值为-1
				op->status = -1;
			}
			else if (numIO == -ETIMEDOUT)
			{
				op->status = -ETIMEDOUT;
				KFS_LOG_DEBUG("Get response recv timed out...");
			}
			else
			{
				KFS_LOG_DEBUG("Get response failed...closing socket");
				sock->Close();
				op->status = -EHOSTUNREACH;
			}

			return -1;
		}

		assert(len > 0);

		// 从响应中解析出命令序号和命令内容的长度
		GetSeqContentLen(buf, len, &resSeq, &contentLen);

		if (resSeq == op->seq)
		{
			if (printMatchingResponse)
			{
				KFS_LOG_VA_DEBUG("Seq #'s match (after mismatch seq): Expect: %lld, got: %lld",
						op->seq, resSeq);
			}
			break;
		}
		KFS_LOG_VA_DEBUG("Seq #'s dont match: Expect: %lld, got: %lld",
				op->seq, resSeq);
		printMatchingResponse = true;

		if (contentLen > 0)
		{
			struct timeval timeout = gDefaultTimeout;
			// 此时响应有效，直接丢弃socket中的数据
			int len = sock->DoSynchDiscard(contentLen, timeout);

			// 此时丢弃的字节数应该等于内容的长度，否则socket中将会出现垃圾数据
			if (len != contentLen)
			{
				sock->Close();
				op->status = -EHOSTUNREACH;
				return -1;
			}
		}
	}

	/* * 此时已经找到了与给定操作相对应的服务器响应 * */

	contentLen = op->contentLength;

	// 将状态信息和内容长度信息存放在op中
	op->ParseResponseHeader(buf, len);

	if (op->contentLength == 0)
	{
		// restore it back: when a write op is sent out and this
		// method is invoked with the same op to get the response, the
		// op's status should get filled in; we shouldn't be stomping
		// over content length.
		// 如果是一个写操作，那么我们只需要操作的状态信息，而不能修改内容长度
		// （因为写之后，内容长度必然为0）
		op->contentLength = contentLen;
		return numIO;
	}

	// This is the annoying part...we may have read some of the data
	// related to attributes already.  So, copy them out and then read
	// whatever else is left

	// 将操作的内容拷贝出来到op的缓冲区
	if (op->contentBufLen == 0)
	{
		op->contentBuf = new char[op->contentLength + 1];
		op->contentBuf[op->contentLength] = '\0';
	}

	// len bytes belongs to the RPC reply.  Whatever is left after
	// stripping that data out is the data.
	// len是整个数据包的长度，numIO是命令（不包含content）的长度
	navail = numIO - len;
	if (navail > 0)
	{
		assert(navail <= (ssize_t)op->contentLength);
		// navail是content的长度，将其拷贝到op的缓冲器中
		memcpy(op->contentBuf, buf + len, navail);
	}
	// 此时，剩余的字节数应该为0，否则则是因为socket中的数据并没有完全放入buf中，因为buf的
	// 空间小于socket中数据的长度。此时则需要补全数据
	nleft = op->contentLength - navail;

	assert(nleft >= 0);

	// 补全socket中没有读入到buf中的数据到op的缓冲器中
	if (nleft > 0)
	{
		struct timeval timeout = gDefaultTimeout;

		// 读入剩余数据
		nread = sock->DoSynchRecv(op->contentBuf + navail, nleft, timeout);
		if (nread == -ETIMEDOUT)
		{
			KFS_LOG_DEBUG("Recv timed out...");
			op->status = -ETIMEDOUT;
		}
		else if (nread <= 0)
		{
			KFS_LOG_DEBUG("Recv failed...closing socket");
			op->status = -EHOSTUNREACH;
			sock->Close();
		}

		if (nread <= 0)
		{
			return 0;
		}
	}

	return nread + numIO;
}

///
/// 执行命令的常规操作：创建请求，发送给服务器，获取响应，解析结果
/// @param [in] op 要接收相应的操作
/// @param [in] 服务器的socket
/// @return 读入的字节数
/// @note 完成时op中已经包含服务器给出的执行状态和必要的数据（content）
///
int KFS::DoOpCommon(KfsOp *op, TcpSocket *sock)
{
	if (sock == NULL)
	{
		KFS_LOG_VA_DEBUG("%s: send failed; no socket", op->Show().c_str());
		assert(sock);
		return -EHOSTUNREACH;
	}

	// 向服务器发送命令，包括header和content
	int res = DoOpSend(op, sock);
	if (res < 0)
	{
		KFS_LOG_VA_DEBUG("%s: send failure code: %d", op->Show().c_str(), res);
		return res;
	}

	// 获取服务器的响应：status和content(包括读和写的content)
	res = DoOpResponse(op, sock);

	if (res < 0)
	{
		KFS_LOG_VA_DEBUG("%s: recv failure code: %d", op->Show().c_str(), res);
		return res;
	}

	if (op->status < 0)
	{
		string errstr = ErrorCodeToStr(op->status);
		KFS_LOG_VA_DEBUG("%s failed with code(%d): %s", op->Show().c_str(), op->status, errstr.c_str());
	}

	return res;
}

///
/// @brief 计算chunk的大小
/// @note 只要有一个chunkserver还在工作，就可以获取chunk的大小信息
///
struct RespondingServer
{
	KfsClientImpl *client;
	const ChunkLayoutInfo &layout;
	int *status;
	off_t *size;
	RespondingServer(KfsClientImpl *cli, const ChunkLayoutInfo &lay, off_t *sz,
			int *st) :
		client(cli), layout(lay), status(st), size(sz)
	{
	}

	// 计算由layout指定的chunk的大小，通过sz指针返回
	bool operator()(ServerLocation loc)
	{
		TcpSocket sock;

		if (sock.Connect(loc) < 0)
		{
			*size = 0;
			*status = -1;
			return false;
		}

		// 计算这个chunk的大小
		SizeOp sop(client->nextSeq(), layout.chunkId, layout.chunkVersion);
		int numIO = DoOpCommon(&sop, &sock);
		if (numIO < 0 && !sock.IsGood())
		{
			return false;
		}

		*status = sop.status;
		if (*status >= 0)
			*size = sop.size;

		return *status >= 0;
	}
};

/*
 * @brief 向指定的服务器请求chunk大小信息
 * @note 没有上一个类那样的可靠性
 */
struct RespondingServer2
{
	KfsClientImpl *client;
	const ChunkLayoutInfo &layout;
	RespondingServer2(KfsClientImpl *cli, const ChunkLayoutInfo &lay) :
		client(cli), layout(lay)
	{
	}
	ssize_t operator()(ServerLocation loc)
	{
		TcpSocket sock;

		if (sock.Connect(loc) < 0)
		{
			return -1;
		}

		SizeOp sop(client->nextSeq(), layout.chunkId, layout.chunkVersion);
		int numIO = DoOpCommon(&sop, &sock);
		if (numIO < 0 && !sock.IsGood())
		{
			return -1;
		}

		return sop.size;
	}
};

///
/// @brief 计算文件的大小：先获取文件的最后一个chunk，然后通过该chunk在文件中\n
/// @brief 的偏移位置和该chunk的大小计算出文件大小
///
off_t KfsClientImpl::ComputeFilesize(kfsFileId_t kfsfid)
{
	// 生成操作
	GetLayoutOp lop(nextSeq(), kfsfid);
	// 执行操作
	(void) DoMetaOpWithRetry(&lop);
	if (lop.status < 0)
	{
		// XXX: This can only during concurrent I/O when someone is
		// deleting a file and we are trying to compute the size of
		// this file.  For now, assert away.
		assert(lop.status != -ENOENT);
		return 0;
	}

	// 转换命令中的content缓冲器到LayoutInfo中
	if (lop.ParseLayoutInfo())
	{
		KFS_LOG_DEBUG("Unable to parse layout info");
		return -1;
	}

	// 如果没有chunk，则文件大小为0
	if (lop.chunks.size() == 0)
		return 0;

	vector<ChunkLayoutInfo>::reverse_iterator last = lop.chunks.rbegin();
	off_t filesize = last->fileOffset;
	off_t endsize = 0;
	int rstatus = 0;
	RespondingServer responder(this, *last, &endsize, &rstatus);

	// 从该chunk所在的几个chunkServer中查询chunk的大小，为保证信息的可用性，用这种方法
	vector<ServerLocation>::iterator s = find_if(last->chunkServers.begin(),
			last->chunkServers.end(), responder);

	if (s != last->chunkServers.end())
	{	// 说明查询成功
		if (rstatus < 0)
		{
			KFS_LOG_VA_DEBUG("RespondingServer status %d", rstatus);
			return 0;
		}
		filesize += endsize;
	}

	return filesize;
}

///
/// @brief 计算一系列文件中每一个文件的大小
///
void KfsClientImpl::ComputeFilesizes(vector<KfsFileAttr> &fattrs, vector<
		FileChunkInfo> &lastChunkInfo)
{
	for (uint32_t i = 0; i < lastChunkInfo.size(); i++)
	{
		if (lastChunkInfo[i].chunkCount == 0)
			continue;
		if (fattrs[i].fileSize >= 0)
			continue;
		for (uint32_t j = 0; j < lastChunkInfo[i].cattr.chunkServerLoc.size(); j++)
		{
			TcpSocket sock;
			ServerLocation loc = lastChunkInfo[i].cattr.chunkServerLoc[j];

			// get all the filesizes we can from this server
			// 从服务器上获取文件大小，优化策略见函数注释
			ComputeFilesizes(fattrs, lastChunkInfo, i, loc);
		}

		bool alldone = true;
		for (uint32_t j = i; j < fattrs.size(); j++)
		{
			if (fattrs[j].fileSize < 0)
			{
				alldone = false;
				break;
			}
		}

		// 如果全部都已经完成了，则提前退出
		if (alldone)
			break;
	}
}

///
/// @brief 计算出从startIdx开始的文件的大小
/// @note 如果在loc上有其他文件的最后一个块，则连同那个块一起计算
///
void KfsClientImpl::ComputeFilesizes(vector<KfsFileAttr> &fattrs, vector<
		FileChunkInfo> &lastChunkInfo, uint32_t startIdx,
		const ServerLocation &loc)
{
	TcpSocket sock;
	vector<ServerLocation>::const_iterator iter;

	if (sock.Connect(loc) < 0)
	{
		return;
	}

	// 计算该文件大小，如果其他文件在该服务器上也有最后一个块，那么同时取出那个文件的大小
	for (uint32_t i = startIdx; i < lastChunkInfo.size(); i++)
	{
		if (lastChunkInfo[i].chunkCount == 0)
			continue;
		if (fattrs[i].fileSize >= 0)
			continue;

		iter = find_if(lastChunkInfo[i].cattr.chunkServerLoc.begin(),
				lastChunkInfo[i].cattr.chunkServerLoc.end(),
				MatchingServer(loc));
		if (iter == lastChunkInfo[i].cattr.chunkServerLoc.end())
			continue;

		SizeOp sop(nextSeq(), lastChunkInfo[i].cattr.chunkId,
				lastChunkInfo[i].cattr.chunkVersion);
		int numIO = DoOpCommon(&sop, &sock);
		if (numIO < 0 && !sock.IsGood())
		{
			return;
		}
		if (sop.status >= 0)
		{
			lastChunkInfo[i].cattr.chunkSize = sop.size;
			fattrs[i].fileSize = lastChunkInfo[i].lastChunkOffset
					+ lastChunkInfo[i].cattr.chunkSize;
		}
	}

}

/// A simple functor to match chunkserver by hostname
/// 用于获取和指定地址相同的chunkserver
class ChunkserverMatcher
{
	string myHostname;
public:
	ChunkserverMatcher(const string &l) :
		myHostname(l)
	{
	}
	bool operator()(const ServerLocation &loc) const
	{
		return loc.hostname == myHostname;
	}
};

///
/// @brief 打开一个chunk，即为chunk设置一个优先服务器
/// @return 返回chunk的大小
///
int KfsClientImpl::OpenChunk(int fd, bool nonblockingConnect)
{
	if (!IsCurrChunkAttrKnown(fd))
	{
		// Nothing known about this chunk
		return -EINVAL;
	}

	ChunkAttr *chunk = GetCurrChunk(fd);
	// chunk缓存无效
	if (chunk->chunkId == (kfsChunkId_t) -1)
	{
		chunk->chunkSize = 0;
		// don't send bogus chunk id's
		return -EINVAL;
	}

	// try the local server first
	// 如果本地也是一个chunkserver，则从本地读入数据
	vector<ServerLocation>::iterator s = find_if(chunk->chunkServerLoc.begin(),
			chunk->chunkServerLoc.end(), ChunkserverMatcher(mHostname));
	if (s != chunk->chunkServerLoc.end())
	{
		FdPos(fd)->SetPreferredServer(*s, nonblockingConnect);
		if (FdPos(fd)->GetPreferredServer() != NULL)
		{
			KFS_LOG_VA_DEBUG("Picking local server: %s", s->ToString().c_str());
			return SizeChunk(fd);
		}
	}

	// if they are on different subnets, pick the one on the same subnet as us
	// 本地没有chunk数据，则优先选择子网号最接近的服务器
	vector<ServerLocation> loc = chunk->chunkServerLoc;

	// 这个版本中并没有采用选择最近的服务器，而是随机选择一个服务器作为优先服务器
	// else pick one at random
	for (vector<ServerLocation>::size_type i = 0; (FdPos(fd)->GetPreferredServer()
			== NULL && i != loc.size()); ++i)
	{
		uint32_t j = rand_r(&mRandSeed) % loc.size();

		FdPos(fd)->SetPreferredServer(loc[j], nonblockingConnect);
		if (FdPos(fd)->GetPreferredServer() != NULL)
			KFS_LOG_VA_DEBUG("Randomly chose: %s", loc[j].ToString().c_str());
	}

	return (FdPos(fd)->GetPreferredServer() == NULL) ? -EHOSTUNREACH
													 : SizeChunk(fd);
}

///
/// @brief 获取一个chunk的大小
///
int KfsClientImpl::SizeChunk(int fd)
{
	ChunkAttr *chunk = GetCurrChunk(fd);

	assert(FdPos(fd)->preferredServer != NULL);
	if (FdPos(fd)->preferredServer == NULL)
		return -EHOSTUNREACH;

	SizeOp op(nextSeq(), chunk->chunkId, chunk->chunkVersion);
	(void) DoOpCommon(&op, FdPos(fd)->preferredServer);
	chunk->chunkSize = op.size;

	KFS_LOG_VA_DEBUG("Chunk: %lld, size = %zd",
			chunk->chunkId, chunk->chunkSize);

	return op.status;
}

///
/// @brief 对metaserver的操作
/// @note 若不成功则会自动重试NUM_RETRIES_PER_OP次
///
int KfsClientImpl::DoMetaOpWithRetry(KfsOp *op)
{
	int res;

	// 连接metaserver
	if (!mMetaServerSock.IsGood())
		ConnectToMetaServer();

	for (int attempt = 0; attempt < NUM_RETRIES_PER_OP; attempt++)
	{
		// 1. 执行操作
		res = DoOpCommon(op, &mMetaServerSock);
		// 若没有错误或超时出现就可以退出了
		if (op->status != -EHOSTUNREACH && op->status != -ETIMEDOUT)
			break;
		// 2. 等待RETRY_DELAY_SECS秒钟
		Sleep(RETRY_DELAY_SECS);
		// 3. 重新连接metaServer
		ConnectToMetaServer();
		// re-issue the op with a new sequence #
		// 4. 换一个新的序号重新执行操作
		op->seq = nextSeq();
	}
	return res;
}

static bool null_fte(const FileTableEntry *ft)
{
	return (ft == NULL);
}

///
/// @brief 用于对目录排序
/// @note 1. 目录排前，文件次之，缓存最后 2. 最近访问时间越早越靠前
///
static bool fte_compare(const FileTableEntry *first,
		const FileTableEntry *second)
{
	bool dir1 = first->fattr.isDirectory;
	bool dir2 = second->fattr.isDirectory;

	if (dir1 == dir2)
		return first->lastAccessTime < second->lastAccessTime;
	// 1是文件缓存，2是目录，2放在前面
	else if ((!dir1) && (first->openMode == 0))
		return dir1;
	// 1是目录，2是文件缓存，2放在前面？？
	else if ((!dir2) && (second->openMode == 0))
		return dir2;

	return dir1;
}

///
/// @brief 查找空闲目录表，若没有则为之分配一个
///
int KfsClientImpl::FindFreeFileTableEntry()
{
	vector<FileTableEntry *>::iterator b = mFileTable.begin();
	vector<FileTableEntry *>::iterator e = mFileTable.end();
	vector<FileTableEntry *>::iterator i = find_if(b, e, null_fte);
	// 采用首次适应算法
	if (i != e)
		return i - b; // Use NULL entries first

	// 未找到空闲目录，则此时需要创建一个新的目录
	int last = mFileTable.size();
	if (last != MAX_FILES)
	{ // Grow vector up to max. size
		mFileTable.push_back(NULL);
		return last;
	}

	// recycle directory entries or files open for attribute caching
	// 此时目录已经存满，需要回收一部分用于缓存的目录，优先查找文件，若同是文件或同不是文件，
	// 则查找最久未被访问的一个
	vector<FileTableEntry *>::iterator oldest = min_element(b, e, fte_compare);
	if ((*oldest)->fattr.isDirectory || ((*oldest)->openMode == 0))
	{
		ReleaseFileTableEntry(oldest - b);
		return oldest - b;
	}

	// 若查找到的entry是一个目录或者是一个已经打开的文件（不是缓存），则不能删除任何一个！
	return -EMFILE; // No luck
}

///
/// @brief 用于查找相同的目录项
/// @note 相同是指：父亲目录和文件名称相同的
///
class FTMatcher
{
	kfsFileId_t parentFid;
	string myname;
public:
	FTMatcher(kfsFileId_t f, const char *n) :
		parentFid(f), myname(n)
	{
	}
	bool operator ()(FileTableEntry *ft)
	{
		return (ft != NULL && ft->parentFid == parentFid && ft->name == myname);
	}
};

///
/// @brief 查看当前目录项是否有效
///
bool KfsClientImpl::IsFileTableEntryValid(int fte)
{
	// The entries for files open for read/write are valid.  This is a
	// handle that is given to the application. The entries for
	// directories need to be revalidated every N secs.  The one
	// exception for directory entries is that for "/"; that is always
	// 2 and is valid.  That entry will never be deleted from the fs.
	// Any other directory can be deleted and we don't want to hold on
	// to stale entries.
	time_t now = time(NULL);

	// 如果指定的“已经打开的文件列表”不是目录并且已经打开 OR 这个目录是根目录 OR
	// 缓存“已经打开的文件列表”还有效
	if (( (!FdAttr(fte)->isDirectory) && (FdInfo(fte)->openMode != 0) )
			|| (FdAttr(fte)->fileId == KFS::ROOTFID) || (now
			- FdInfo(fte)->validatedTime < FILE_CACHE_ENTRY_VALID_TIME))
		return true;

	return false;
}

///
/// @brief 在已经打开的文件目录中查找当前文件
/// @return 若查不到或该文件已经过期返回－1
///
int KfsClientImpl::LookupFileTableEntry(kfsFileId_t parentFid, const char *name)
{
	FTMatcher match(parentFid, name);
	vector<FileTableEntry *>::iterator i;
	i = find_if(mFileTable.begin(), mFileTable.end(), match);
	if (i == mFileTable.end())
		return -1;
	int fte = i - mFileTable.begin();

	if (IsFileTableEntryValid(fte))
		return fte;

	KFS_LOG_VA_DEBUG("Entry for <%lld, %s> is likely stale; forcing revalidation",
			parentFid, name);
	// the entry maybe stale; force revalidation
	// 此时FileTableEntryValid()已经为：否！强制重新获取认证
	ReleaseFileTableEntry(fte);

	return -1;

}

///
/// @brief 根据pathname查找文件名到“已经打开的文件目录”的位置
/// @note先从cache中查找，若失败则查找已经打开的文件列表
///
int KfsClientImpl::LookupFileTableEntry(const char *pathname)
{
	string p(pathname);
	// 文件名称到文件id的map
	NameToFdMapIter iter = mPathCache.find(p);

	if (iter != mPathCache.end())
	{	// 如果已经找到目录

		// 取出文件在“已打开文件目录”中的位置
		int fte = iter->second;

		if (IsFileTableEntryValid(fte))
		{
			// mFileTable维护了一个已打开文件的列表
			assert(mFileTable[fte]->pathname == pathname);
			return fte;
		}

		// 如果fte不可用，则清空该位置的“已打开文件目录”
		ReleaseFileTableEntry(fte);
		return -1;
	}

	// 如果未找到该文件，即该文件未打开或者缓存
	kfsFileId_t parentFid;
	string name;

	// 获取文件的目录和文件名
	int res = GetPathComponents(pathname, &parentFid, name);
	if (res < 0)
		return res;

	// 此时，cache中已经没有找到目录，此时从打开的文件列表中查找
	return LookupFileTableEntry(parentFid, name.c_str());
}

///
/// @brief 向“已经打开的文件目录”索取该文件的位置
/// @note 若该文件不在目录中，则为之分配一个目录
///
int KfsClientImpl::ClaimFileTableEntry(kfsFileId_t parentFid, const char *name,
		string pathname)
{
	// 查找出给定文件在“已经打开的文件目录”中的位置（下标号码）
	int fte = LookupFileTableEntry(parentFid, name);
	if (fte >= 0)
		return fte;

	// 若没有查找到，说明此文件未打开或者已经过期（需要重新认证）
	return AllocFileTableEntry(parentFid, name, pathname);
}

///
/// @brief 采用首次适应算法查找第一个空目录（使用过程中因为过期或文件关闭等清空的位置）
/// @note 若没有空闲目录则在最后添加一个目录;
/// @note 若目录数目已经超过最大数目，则删除其中的一个缓存;
/// @note 若没有缓存文件，则返回错误！
///
int KfsClientImpl::AllocFileTableEntry(kfsFileId_t parentFid, const char *name,
		string pathname)
{

	int fte = FindFreeFileTableEntry();

	if (fte >= 0)
	{
		/*
		 if (parentFid != KFS::ROOTFID)
		 KFS_LOG_VA_INFO("Alloc'ing fte: %d for %d, %s", fte,
		 parentFid, name);
		 */

		// 完成“已经打开的文件目录”条目
		mFileTable[fte] = new FileTableEntry(parentFid, name);
		mFileTable[fte]->validatedTime
				= mFileTable[fte]->lastAccessTime = time(NULL);
		if (pathname != "")
		{	// 为这个文件建立缓存
			string fullpath = build_path(mCwd, pathname.c_str());
			mPathCache[pathname] = fte;
			// mFileTable[fte]->pathCacheIter = mPathCache.find(pathname);
		}
		mFileTable[fte]->pathname = pathname;
	}
	return fte;
}

///
/// @brief 释放“已经打开的文件目录”中的第fte个条目
/// @note 单纯的删除就可以了
///
void KfsClientImpl::ReleaseFileTableEntry(int fte)
{
	if (mFileTable[fte]->pathname != "")
		mPathCache.erase(mFileTable[fte]->pathname);
	/*
	 if (mFileTable[fte]->pathCacheIter != mPathCache.end())
	 mPathCache.erase(mFileTable[fte]->pathCacheIter);
	 */
	delete mFileTable[fte];
	mFileTable[fte] = NULL;
}

///
/// @brief 查询指定文件
/// @note 如果在目录表中，直接返回它的位置，否则为它分配一个目录
///
int KfsClientImpl::Lookup(kfsFileId_t parentFid, const char *name)
{
	int fte = LookupFileTableEntry(parentFid, name);
	if (fte >= 0)
		return fte;

	LookupOp op(nextSeq(), parentFid, name);
	(void) DoOpCommon(&op, &mMetaServerSock);
	if (op.status < 0)
	{
		return op.status;
	}
	// Everything is good now...
	fte = ClaimFileTableEntry(parentFid, name, "");
	if (fte < 0) // too many open files
		return -EMFILE;

	FileAttr *fa = FdAttr(fte);
	*fa = op.fattr;

	return fte;
}

///
/// @brief 取出文件或文件夹的父目录以及文件或文件夹名称，同时检验路径是否错误
/// @param[in] path	The path string that needs to be extracted
/// @param[out] parentFid  The file-id corresponding to the parent dir
/// @param[out] name    The filename following the final "/".
/// @retval 0 on success; -errno on failure
///
int KfsClientImpl::GetPathComponents(const char *pathname,
		kfsFileId_t *parentFid, string &name)
{
	const char slash = '/';

	// 主要处理相对路径和绝对路径，把所有的路径转换为系统可以识别的绝对路径
	string pathstr = build_path(mCwd, pathname);

	string::size_type pathlen = pathstr.size();

	// 若此处返回，则说明build_path()函数出错
	if (pathlen == 0 || pathstr[0] != slash)
		return -EINVAL;

	// find the trailing '/'
	string::size_type rslash = pathstr.rfind('/');
	if (rslash + 1 == pathlen)
	{
		// 清除结尾的'/'
		pathstr.erase(rslash);
		pathlen = pathstr.size();
		rslash = pathstr.rfind('/');
	}

	// 如果path是根目录
	if (pathlen == 0)
		name = "/";
	else
	{
		// the component of the name we want is between trailing slash
		// and the end of string
		name.assign(pathstr, rslash + 1, string::npos);
		// get rid of the last component of the path as we have copied
		// it out.
		pathstr.erase(rslash + 1, string::npos);
		pathlen = pathstr.size();
	}

	// 说明路径有误，属于用户错误
	if (name.size() == 0)
		return -EINVAL;

	*parentFid = KFS::ROOTFID;
	if (pathlen == 0)
		return 0;

	// 确认name之前的所有路径都是目录
	string::size_type start = 1;
	while (start != string::npos)
	{
		// 每次进入一个目录，最初始目录是root，每次循环向下进入一层
		string::size_type next = pathstr.find(slash, start);
		if (next == string::npos)
			break;

		if (next == start)
			return -EINVAL; // don't allow "//" in path
		string component(pathstr, start, next - start);
		int fte = Lookup(*parentFid, component.c_str());
		if (fte < 0)
			return fte;
		else if (!FdAttr(fte)->isDirectory)	// 如果不是目录，则出错
			return -ENOTDIR;
		else	// 如果没有错误，目录向下
			*parentFid = FdAttr(fte)->fileId;
		start = next + 1; // next points to '/'
	}

	KFS_LOG_VA_DEBUG("file-id for dir: %s (file = %s) is %lld",
			pathstr.c_str(), name.c_str(), *parentFid);
	return 0;
}

///
/// @brief 将错误代码转换成字符串信息
///
string KFS::ErrorCodeToStr(int status)
{

	if (status == 0)
		return "";

	char buf[4096];
	char *errptr = NULL;

#if defined (__APPLE__) || defined(__sun__)
	// 根据status，给出错误信息
	if (strerror_r(-status, buf, sizeof buf) == 0)
	errptr = buf;
	else
	{
		strcpy(buf, "<unknown error>");
		errptr = buf;
	}
#else
	if ((errptr = strerror_r(-status, buf, sizeof buf)) == NULL)
	{
		strcpy(buf, "<unknown error>");
		errptr = buf;
	}
#endif
	return string(errptr);

}

///
/// @brief 向metaserver获取某一个chunk的lease
/// @note 若服务器忙，则重试（最多重试三次）
///
int KfsClientImpl::GetLease(kfsChunkId_t chunkId)
{
	int res;

	assert(chunkId >= 0);

	for (int i = 0; i < 3; i++)
	{ // XXX Evil constant
		LeaseAcquireOp op(nextSeq(), chunkId);
		res = DoOpCommon(&op, &mMetaServerSock);

		if (op.status == 0)
			mLeaseClerk.RegisterLease(op.chunkId, op.leaseId);
		if (op.status != -EBUSY)
		{
			res = op.status;
			break;
		}

		KFS_LOG_DEBUG("Server says lease is busy...waiting");
		// Server says the lease is busy...so wait
		Sleep(KFS::LEASE_INTERVAL_SECS);
	}
	return res;
}

///
/// @brief 更新指定chunk的lease的信息
///
void KfsClientImpl::RenewLease(kfsChunkId_t chunkId)
{
	int64_t leaseId;

	int res = mLeaseClerk.GetLeaseId(chunkId, leaseId);
	if (res < 0)
		return;

	LeaseRenewOp op(nextSeq(), chunkId, leaseId);
	res = DoOpCommon(&op, &mMetaServerSock);
	if (op.status == 0)
	{
		mLeaseClerk.LeaseRenewed(op.chunkId);
		return;
	}

	// 如果lease失效，则吊销这个lease
	if (op.status == -EINVAL)
	{
		mLeaseClerk.UnRegisterLease(op.chunkId);
	}
}

///
/// @brief 计算文件中每一个块的每一个副本的大小，并且统计数据总量
/// @note 所有均输出到标准输出
///
int KfsClientImpl::EnumerateBlocks(const char *pathname)
{
	struct stat s;
	int res, fte;

	MutexLock l(&mMutex);

	if ((res = Stat(pathname, s, false)) < 0)
	{
		cout << "Unable to stat path: " << pathname << ' ' << ErrorCodeToStr(
				res) << endl;
		return -ENOENT;
	}

	if (S_ISDIR(s.st_mode))
	{
		cout << "Path: " << pathname << " is a directory" << endl;
		return -EISDIR;
	}

	// 要求pathname指定的文件已经存在或者在缓存中
	fte = LookupFileTableEntry(pathname);
	assert(fte >= 0);

	KFS_LOG_VA_DEBUG("Fileid for %s is: %d", pathname, FdAttr(fte)->fileId);

	GetLayoutOp lop(nextSeq(), FdAttr(fte)->fileId);
	(void) DoMetaOpWithRetry(&lop);
	if (lop.status < 0)
	{
		cout << "Get layout failed on path: " << pathname << " "
				<< ErrorCodeToStr(lop.status) << endl;
		return lop.status;
	}

	if (lop.ParseLayoutInfo())
	{
		cout << "Unable to parse layout for path: " << pathname << endl;
		return -1;
	}

	ssize_t filesize = 0;

	for (vector<ChunkLayoutInfo>::const_iterator i = lop.chunks.begin(); i
			!= lop.chunks.end(); ++i)
	{
		RespondingServer2 responder(this, *i);
		vector<ssize_t> chunksize;

		// Get the size for the chunk from all the responding servers
		// 从每一个服务器上计算这个块的大小；该块所对应的所有chunkserver均要计算
		chunksize.reserve(i->chunkServers.size());
		transform(i->chunkServers.begin(), i->chunkServers.end(),
				chunksize.begin(), responder);

		// 输出到标准输出
		cout << i->fileOffset << '\t' << i->chunkId << endl;
		for (uint32_t k = 0; k < i->chunkServers.size(); k++)
		{
			cout << "\t\t" << i->chunkServers[k].ToString() << '\t'
					<< chunksize[k] << endl;
		}

		for (uint32_t k = 0; k < i->chunkServers.size(); k++)
		{
			if (chunksize[k] >= 0)
			{
				filesize += chunksize[k];
				break;
			}
		}
	}
	cout << "File size computed from chunksizes: " << filesize << endl;
	return 0;
}

///
/// @brief 获取校验和，存储到checksum中
///
bool KfsClientImpl::GetDataChecksums(const ServerLocation &loc,
		kfsChunkId_t chunkId, uint32_t *checksums)
{
	TcpSocket sock;

	// chenksum存放在conent中
	GetChunkMetadataOp op(nextSeq(), chunkId);
	uint32_t numChecksums = CHUNKSIZE / CHECKSUM_BLOCKSIZE;

	if (sock.Connect(loc) < 0)
	{
		return false;
	}

	int numIO = DoOpCommon(&op, &sock);
	if (numIO < 0 && !sock.IsGood())
	{
		return false;
	}
	if (op.contentLength < numChecksums * sizeof(uint32_t))
	{
		return false;
	}
	memcpy((char *) checksums, op.contentBuf, numChecksums * sizeof(uint32_t));
	return true;
}

///
/// @brief 用checksums中的数据对pathname指定的文件进行校验
///
bool KfsClientImpl::VerifyDataChecksums(const char *pathname, const vector<
		uint32_t> &checksums)
{
	struct stat s;
	int res, fte;

	MutexLock l(&mMutex);

	if ((res = Stat(pathname, s, false)) < 0)
	{
		cout << "Unable to stat path: " << pathname << ' ' << ErrorCodeToStr(
				res) << endl;
		return false;
	}

	if (S_ISDIR(s.st_mode))
	{
		cout << "Path: " << pathname << " is a directory" << endl;
		return false;
	}

	fte = LookupFileTableEntry(pathname);
	assert(fte >= 0);

	return VerifyDataChecksums(fte, checksums);
}

///
/// @brief 校验buf和chunkserver上的数据
/// @note 先计算出buf中数据校验和，然后再取出metaserver中各个块的数据校验和进行比较\n
/// @note buf应该是文件的整个数据把
///
bool KfsClientImpl::VerifyDataChecksums(int fd, off_t offset, const char *buf,
		size_t numBytes)
{
	MutexLock l(&mMutex);
	vector<uint32_t> checksums;

	if (FdAttr(fd)->isDirectory)
	{
		cout << "Can't verify checksums on a directory" << endl;
		return false;
	}

	boost::scoped_array<char> tempBuf;

	tempBuf.reset(new char[CHECKSUM_BLOCKSIZE]);
	for (off_t i = 0; i < (off_t) numBytes; i += CHECKSUM_BLOCKSIZE)
	{
		uint32_t bytesToCopy = min(CHECKSUM_BLOCKSIZE,
				(uint32_t) (numBytes - i));
		if (bytesToCopy != CHECKSUM_BLOCKSIZE)
			memset(tempBuf.get(), 0, CHECKSUM_BLOCKSIZE);
		memcpy(tempBuf.get(), buf, bytesToCopy);
		uint32_t cksum =
				ComputeBlockChecksum(tempBuf.get(), CHECKSUM_BLOCKSIZE);

		checksums.push_back(cksum);
	}

	return VerifyDataChecksums(fd, checksums);

}

///
/// @brief 对文件fte的所有chunkserver上的数据进行校验，checksums提供校验和
///
bool KfsClientImpl::VerifyDataChecksums(int fte,
		const vector<uint32_t> &checksums)
{
	GetLayoutOp lop(nextSeq(), FdAttr(fte)->fileId);
	(void) DoMetaOpWithRetry(&lop);
	if (lop.status < 0)
	{
		cout << "Get layout failed with error: " << ErrorCodeToStr(lop.status)
				<< endl;
		return false;
	}

	if (lop.ParseLayoutInfo())
	{
		cout << "Unable to parse layout info!" << endl;
		return false;
	}

	int blkstart = 0;
	uint32_t numChecksums = CHUNKSIZE / CHECKSUM_BLOCKSIZE;

	// 对于每一个chunk的数据进行校验
	for (vector<ChunkLayoutInfo>::const_iterator i = lop.chunks.begin(); i
			!= lop.chunks.end(); ++i)
	{
		boost::scoped_array<uint32_t> chunkChecksums;

		chunkChecksums.reset(new uint32_t[numChecksums]);

		for (uint32_t k = 0; k < i->chunkServers.size(); k++)
		{
			// 获取每一个chunkserver上数据的校验
			if (!GetDataChecksums(i->chunkServers[k], i->chunkId,
					chunkChecksums.get()))
			{
				KFS_LOG_VA_INFO("Didn't get checksums from server %s", i->chunkServers[k].ToString().c_str());
				return false;
			}

			bool mismatch = false;
			// 和checksums指定的校验和进行比较
			for (uint32_t v = 0; v < numChecksums; v++)
			{
				if (blkstart + v >= checksums.size())
					break;
				if (chunkChecksums[v] != checksums[blkstart + v])
				{
					cout << "computed: " << chunkChecksums[v] << " got: "
							<< checksums[blkstart + v] << endl;
					KFS_LOG_VA_INFO("Checksum mismatch for block %d on server %s", blkstart + v,
							i->chunkServers[k].ToString().c_str());
					mismatch = true;
				}
			}
			if (mismatch)
				return false;
		}
		blkstart += numChecksums;
	}
	return true;
}
