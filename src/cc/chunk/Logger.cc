//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: Logger.cc 71 2008-07-07 15:49:14Z sriramsrao $
//
// Created 2006/06/20
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

#include<map>
#include<sstream>
extern "C"
{
#include <sys/stat.h>
#include <strings.h>
}

#include "libkfsIO/Globals.h"

#include "Logger.h"
#include "ChunkManager.h"
#include "ChunkServer.h"
#include "KfsOps.h"

using std::ios_base;
using std::map;
using std::ifstream;
using std::istringstream;
using std::ostringstream;
using std::list;
using std::ofstream;
using std::string;
using std::vector;

using namespace KFS;
using namespace KFS::libkfsio;

Logger KFS::gLogger;
/// checksums for a 64MB chunk can make a long line...
const int MAX_LINE_LENGTH = 32768;
//
char ckptLogVersionStr[128];

typedef void (*ParseHandler_t)(istringstream &ist);

/*!
 * \brief check whether a file exists
 * \param[in]	name	path name of the file
 * \return		true if stat says it is a plain file
 */
/// 查看一个Linux文件是否存在
static bool file_exists(string name)
{
	struct stat s;
	if (stat(name.c_str(), &s) == -1)
		return false;

	return S_ISREG(s.st_mode);
}

/// 构造函数，不指定日志存放目录和日志名称，启动一个定时器
Logger::Logger()
{
	mLogDir = "";
	mLogFilename = "";
	mLoggerTimeoutImpl = new LoggerTimeoutImpl(this);
	mLogGenNum = 1;
	sprintf(ckptLogVersionStr, "version: %d", KFS_LOG_VERSION);
}

Logger::~Logger()
{
	mFile.close();
	delete mLoggerTimeoutImpl;
}

/// 初始化，指定目录存放的路径
void Logger::Init(const string &logDir)
{
	mLogDir = logDir;
	mLogFilename = mLogDir;
	mLogFilename += "/logs";

}

static void *
logger_main(void *dummy)
{
	(void) dummy; // shut-up g++
	gLogger.MainLoop();
	return NULL;
}

/// Logger常驻程序。查看mPending中是否有要写入日志文件的操作，如果有的话，为之处理；否则，
/// 忙等指导有要写入的日志操作
void Logger::MainLoop()
{
	KfsOp *op;
	list<KfsOp *> done;
	list<KfsOp *>::iterator iter;

	// 一直查看mPending中是否有要处理的操作，如果有的话进行处理，否则进行忙等
	while (1)
	{
		op = mPending.dequeue();
		while (op != NULL)
		{
			// 处理mPending中所有的写日志操作
			// pull as many as we can and log them
			if (op->op == CMD_CHECKPOINT)
			{
				// 保存当前的日志文件，并且开始使用新的日志文件
				// Checkpoint ops are special.  There is log handling
				// that needs to be done.  After writing out the
				// checkpoint, get rid of the op.
				// 将缓存中的数据写入文件
				mFile.flush();

				// 换一个新的日志文件
				Checkpoint(op);
				delete op;
			}
			else
			{
				if (op->status >= 0)
				{
					// 将成功完成的操作输出到日志文件中
					op->Log(mFile);
				}
				// 添加到已经完成的日志中
				done.push_back(op);
			}
			// 取出最早注册的要写入日志的操作
			op = mPending.dequeue_nowait();
		}

		// 处理完mPending中所有的日志记录之后，写入日志文件
		// one flush for everything we have in the queue
		mFile.flush();
		// now, allow everything that was flushed
		// 把刚才完成的所有日志操作写入mLogged中
		while ((iter = done.begin()) != done.end())
		{
			op = *iter;
			done.erase(iter);

			/// 将op加入到已经完成的写日志操作队列
			mLogged.enqueue(op);
		}
		globals().netKicker.Kick();
		KFS_LOG_DEBUG("Kicked the net manager");
	}
}

/// 完成处理：对于CMD_CHECKPOINT不做处理；对CMD_WRITE进行回复；对于其他的，执行操作
void Logger::Submit(KfsOp *op)
{
	if (op->op == CMD_CHECKPOINT)
	{
		delete op;
		return;
	}
	if (op->op == CMD_WRITE)
	{
		KFS::SubmitOpResponse(op);
	}
	else
	{
		assert(op->clnt != NULL);
		op->clnt->HandleEvent(EVENT_CMD_DONE, op);
	}
}

/// 向Event Processor提交操作
void Logger::Dispatch()
{
	KfsOp *op;

#ifdef DEBUG
	verifyExecutingOnNetProcessor();
#endif

	// KFS_LOG_DEBUG("Logger timeout");

	while (!mLogged.empty())
	{
		// 与Submit不同的地方在于不删除已经完成的操作
		op = mLogged.dequeue_nowait();
		if (op == NULL)
			break;

		// When internally generated ops are done, they go
		// back to the event processor to finish up processing
		// 内部生成的操作完成之后，调入Event Processor中进行处理
		if (op->op == CMD_WRITE)
		{
			KFS::SubmitOpResponse(op);
		}
		else
		{
			assert(op->clnt != NULL);
			op->clnt->HandleEvent(EVENT_CMD_DONE, op);
		}
	}
}

/// 打开当前版本相关的日志文件（如果没有则创建），注册timeout处理程序，然后启动日志处理线程
void Logger::Start()
{
	string filename;
	bool writeHeader = false;

	// 关闭已经打开的日志文件
	if (mFile.is_open())
	{
		mFile.close();
	}

	// 获取当前日志版本相关的日志文件名称
	filename = MakeLogFilename();

	// 如果该日志文件已经存在，则其Header一定是已经完成了的
	if (!file_exists(filename.c_str()))
		writeHeader = true;

	// 打开或者创建日志文件
	mFile.open(filename.c_str(), ios_base::app);

	// 如果这个文件是新建的，则需要写入Log Header
	if (writeHeader)
	{
		// KFS_LOG_VA_DEBUG("Writing out a log header");
		mFile << ckptLogVersionStr << '\n';
		mFile.flush();
	}

	if (!mFile.is_open())
	{
		KFS_LOG_VA_WARN("Unable to open: %s", filename.c_str());
	}
	assert(!mFile.fail());

	// 生成并注册一个timeout处理程序
	globals().netManager.RegisterTimeoutHandler(mLoggerTimeoutImpl);
	mWorker.start(logger_main, NULL);
}

/// 关闭当前正在写入的日志文件，将last checkpoint指向该文件，然后生成一个新的日志文件，并开始
/// 向该文件写入日志
void Logger::Checkpoint(KfsOp *op)
{
	CheckpointOp *cop = static_cast<CheckpointOp *> (op);
	ofstream ofs;
	string ckptFilename;
	string lastCP;

	/// 生成检查点文件名
	ckptFilename = MakeCkptFilename();
	/// 生成最近检查点文件名
	lastCP = MakeLatestCkptFilename();

	// 创建CheckPoint文件
	ofs.open(ckptFilename.c_str(), ios_base::out);
	if (!ofs)
	{
		perror("Ckpt create failed: ");
		return;
	}

	// write out a header that has version and name of log file
	// 将CheckPoint的版本号写入文件Header中
	ofs << ckptLogVersionStr << '\n';

	// This is the log file associated with this checkpoint.  That is,
	// this log file contains all the activities since this checkpoint.
	ofs << "log: " << mLogFilename << '.' << mLogGenNum + 1 << '\n';

	// cop中包含了到这个checkpoint结束的所有的活动
	if (cop != NULL)
	{
		ofs << cop->data.str();
		ofs.flush();
		assert(!ofs.fail());
	}
	ofs.close();

	// now, link the latest
	// 删除最后一个checkpoint，更新最后一个checkpoint为刚刚生成的checkpoint文件
	unlink(lastCP.c_str());

	if (link(ckptFilename.c_str(), lastCP.c_str()) < 0)
	{
		perror("link of ckpt file failed: ");
	}

	// 将mFile指向一个新的日志文件
	RotateLog();
}

/// 获取当前日志版本相关的日志文件的路径
string Logger::MakeLogFilename()
{
	ostringstream os;

	os << mLogFilename << '.' << mLogGenNum;
	return os.str();
}

/// 获取当前generation的CheckPoint name
string Logger::MakeCkptFilename()
{
	ostringstream os;

	os << mLogDir << '/' << "ckpt" << '.' << mLogGenNum;
	return os.str();
}

/// 获取最近生成的日志文件完整名称
string Logger::MakeLatestCkptFilename()
{
	string s(mLogDir);

	s += "/ckpt_latest";
	return s;
}

/// 关闭当前文件，然后生成一个新版本的Log文件，并用mFile打开该文件。即：切换正在写入
/// 的Log文件到一个更新版本的Log文件
void Logger::RotateLog()
{
	string filename;

	// 关闭已经打开了的日志文件
	if (mFile.is_open())
	{
		mFile.close();
	}

	// 获取当前版本的日志文件名称
	filename = MakeLogFilename();
	// For log rotation, get rid of the old log and start a new one.
	// For now, preserve all the log files.

	// unlink(filename.c_str());

	mLogGenNum++;
	/// 获取新版本的log文件名称，并创建该文件
	filename = MakeLogFilename();
	mFile.open(filename.c_str());
	if (!mFile.is_open())
	{
		KFS_LOG_VA_WARN("Unable to open: %s", filename.c_str());
		return;
	}

	// 将CheckPoint版本号写入文件Header
	mFile << ckptLogVersionStr << '\n';
	mFile.flush();
}

/// 从最后一个checkPoint文件获取CheckPoint版本号
int Logger::GetVersionFromCkpt()
{
	string lastCP;
	ifstream ifs;
	char line[MAX_LINE_LENGTH];

	// 获取最后保存的俄日志文件路径
	lastCP = MakeLatestCkptFilename();

	// 如果日志文件不存在，则直接返回KFS_LOG_VERSION
	if (!file_exists(lastCP.c_str()))
		return KFS_LOG_VERSION;

	// 如果日志文件打开失败，则直接返回KFS_LOG_VERSION
	ifs.open(lastCP.c_str(), ios_base::in);
	if (!ifs)
	{
		return KFS_LOG_VERSION;
	}

	// Read the header
	// Line 1 is the version
	memset(line, '\0', MAX_LINE_LENGTH);
	// 将文件中的第一行读入到line中
	ifs.getline(line, MAX_LINE_LENGTH);
	if (ifs.eof())
	{
		// if we can't read the file...we claim to be new version
		return KFS_LOG_VERSION;
	}

	// 使用文件中第一行的内容获取CheckPoint Version
	return GetCkptVersion(line);
}

/// 从日志文件的第一行读入CheckPoint版本号
/// @note 如果versionLine和ckptLogVersionStr相同，则返回KFS_LOG_VERSION；如果和
/// old version相同，则返回KFS_LOG_VERSION_V1；否则，返回0
int Logger::GetCkptVersion(const char *versionLine)
{
	if (strncmp(versionLine, ckptLogVersionStr, strlen(ckptLogVersionStr)) == 0)
	{
		return KFS_LOG_VERSION;
	}

	// check if it is an earlier version
	char olderVersionStr[128];
	sprintf(olderVersionStr, "version: %d", KFS_LOG_VERSION_V1);
	if (strncmp(versionLine, olderVersionStr, strlen(olderVersionStr)) == 0)
	{
		return KFS_LOG_VERSION_V1;
	}
	return 0;
}

/// 获取日志的版本号，即Check Point的版本号
int Logger::GetLogVersion(const char *versionLine)
{
	// both are in the same format
	return GetCkptVersion(versionLine);
}

/// 恢复系统
void Logger::Restore()
{
	string lastCP;	// 最近生成的日志文件的完整名称
	ifstream ifs;
	char line[MAX_LINE_LENGTH], *genNum;
	ChunkInfoHandle_t *cih;
	ChunkInfo_t entry;
	int version;

	// 获取完整名称
	lastCP = MakeLatestCkptFilename();

	// 如果文件不存在，则直接退出
	if (!file_exists(lastCP.c_str()))
		goto out;

	ifs.open(lastCP.c_str(), ios_base::in);
	if (!ifs)
	{
		perror("Ckpt open failed: ");
		goto out;
	}

	// Read the header
	// Line 1 is the version
	memset(line, '\0', MAX_LINE_LENGTH);
	// 读入日志文件的第1行：Log版本号
	ifs.getline(line, MAX_LINE_LENGTH);
	if (ifs.eof())
		goto out;

	version = GetCkptVersion(line);
	/// 如果获取的版本号不是KFS_LOG_VERSION_V1，则说明出现了系统错误
	if (version != KFS_LOG_VERSION_V1)
	{
		KFS_LOG_VA_ERROR("Restore ckpt: Ckpt version str mismatch: read: %s",
				line);
		goto out;
	}

	// Line 2 is the log file name
	memset(line, '\0', MAX_LINE_LENGTH);
	// 读入文件的第2行：日志文件名称，包含生成版本的号码
	ifs.getline(line, MAX_LINE_LENGTH);
	if (ifs.eof())
		goto out;
	// 第2行的前4个字母肯定是："log:"
	if (strncmp(line, "log:", 4) != 0)
	{
		KFS_LOG_VA_ERROR("Restore ckpt: Log line mismatch: read: %s",
				line);
		goto out;
	}

	// 找到最后一个'.'出现的物理地址
	genNum = rindex(line, '.');
	if (genNum != NULL)
	{
		genNum++;
		// 获取Log文件版本号
		mLogGenNum = atoll(genNum);
		KFS_LOG_VA_DEBUG("Read log gen #: %lld", mLogGenNum);
	}

	// Read the checkpoint file
	while (!ifs.eof())
	{
		ifs.getline(line, MAX_LINE_LENGTH);
		// 将line中指定的内容转换为chunkInfo
		if (!ParseCkptEntry(line, entry))
			break;

		cih = new ChunkInfoHandle_t();
		cih->chunkInfo = entry;

		KFS_LOG_VA_DEBUG("Read chunk: %ld, %d, %lu",
				cih->chunkInfo.chunkId,
				cih->chunkInfo.chunkVersion,
				cih->chunkInfo.chunkSize);
		// 向chunkTable中添加chunk句柄
		gChunkManager.AddMapping(cih);
	}

	// 退出：关闭文件，重做日志
	out: ifs.close();

	// replay the logs
	// 重做日志文件中有效的操作
	ReplayLog();
}

/// 将line中的字符串转化成一个ChunkInfo：fileID, chunkId, chunkSize, chunkVersion,
/// chunkBlockChecksum[]等
bool Logger::ParseCkptEntry(const char *line, ChunkInfo_t &entry)
{
	const string l = line;
	istringstream ist(line);
	vector<uint32_t>::size_type count;

	if (l.empty())
		return false;

	ist.str(line);
	ist >> entry.fileId;
	ist >> entry.chunkId;
	ist >> entry.chunkSize;
	ist >> entry.chunkVersion;
	ist >> count;
	for (vector<uint32_t>::size_type i = 0; i < count; ++i)
	{
		ist >> entry.chunkBlockChecksum[i];
	}

	return true;
}

// Handlers for each of the entry types in the log file
/// 从日志文件中解析出来分配块操作：chunkID, fileId, chunkVersion
static void ParseAllocateChunk(istringstream &ist)
{
	kfsChunkId_t chunkId;
	kfsFileId_t fileId;
	int64_t chunkVersion;

	ist >> chunkId;
	ist >> fileId;
	ist >> chunkVersion;
	gChunkManager.ReplayAllocChunk(fileId, chunkId, chunkVersion);

}

/// 从日志文件中解析删除chunk的操作：chunkId
static void ParseDeleteChunk(istringstream &ist)
{
	kfsChunkId_t chunkId;

	ist >> chunkId;
	gChunkManager.ReplayDeleteChunk(chunkId);
}

/// 从日志文件中解析写操作：chunk ID, chunk size, offset, checksum block number,
/// checksum set.
static void ParseWrite(istringstream &ist)
{
	kfsChunkId_t chunkId;
	off_t chunkSize;
	vector<uint32_t> checksums;
	uint32_t offset;
	vector<uint32_t>::size_type n;

	ist >> chunkId;
	ist >> chunkSize;
	ist >> offset;
	ist >> n;
	for (vector<uint32_t>::size_type i = 0; i < n; ++i)
	{
		uint32_t v;
		ist >> v;
		checksums.push_back(v);
	}
	gChunkManager.ReplayWriteDone(chunkId, chunkSize, offset, checksums);
}

/// 重做截尾操作：chunk ID, chunk size.
static void ParseTruncateChunk(istringstream &ist)
{
	kfsChunkId_t chunkId;
	off_t chunkSize;

	ist >> chunkId;
	ist >> chunkSize;
	gChunkManager.ReplayTruncateDone(chunkId, chunkSize);
}

/// 重做改变chunk version的操作：chunk ID, fileId, chunkVersion.
static void ParseChangeChunkVers(istringstream &ist)
{
	kfsChunkId_t chunkId;
	kfsFileId_t fileId;
	int64_t chunkVersion;

	ist >> chunkId;
	ist >> fileId;
	ist >> chunkVersion;
	gChunkManager.ReplayChangeChunkVers(fileId, chunkId, chunkVersion);
}

//
// Each log entry is of the form <OP-NAME> <op args>\n
// To replay the log, read a line, from the <OP-NAME> identify the
// handler and call it to parse/replay the log entry.
/// 解析Log文件中的所有操作，并重新执行这些操作
void Logger::ReplayLog()
{
	istringstream ist;
	char line[MAX_LINE_LENGTH];
	string l;
	map<string, ParseHandler_t> opHandlers;
	map<string, ParseHandler_t>::iterator iter;
	ifstream ifs;
	string filename;
	string opName;
	int version;

	// 获取当前Log日志文件的完整名称
	filename = MakeLogFilename();

	// 如果日志文件不存在，则提示
	if (!file_exists(filename.c_str()))
	{
		KFS_LOG_VA_INFO("File: %s doesn't exist; no log replay",
				filename.c_str());
		return;
	}

	ifs.open(filename.c_str(), ios_base::in);
	if (!ifs)
	{
		KFS_LOG_VA_DEBUG("Unable to open: %s", filename.c_str());
		return;
	}

	// Read the header
	// Line 1 is the version
	memset(line, '\0', MAX_LINE_LENGTH);
	ifs.getline(line, MAX_LINE_LENGTH);
	if (ifs.eof())
	{
		ifs.close();
		return;
	}

	// 获取日志文件的版本号
	version = GetLogVersion(line);
	if (version != KFS_LOG_VERSION_V1)
	{
		KFS_LOG_VA_ERROR("Replay log failed: Log version str mismatch: read: %s",
				line);
		ifs.close();
		return;
	}

	/// 注册各个操作所对应的函数地址
	opHandlers["ALLOCATE"] = ParseAllocateChunk;
	opHandlers["DELETE"] = ParseDeleteChunk;
	opHandlers["WRITE"] = ParseWrite;
	opHandlers["TRUNCATE"] = ParseTruncateChunk;
	opHandlers["CHANGE_CHUNK_VERS"] = ParseChangeChunkVers;

	while (!ifs.eof())
	{
		ifs.getline(line, MAX_LINE_LENGTH);
		l = line;
		if (l.empty())
			break;
		ist.str(l);
		ist >> opName;

		iter = opHandlers.find(opName);

		if (iter == opHandlers.end())
		{
			/// 如果没有找到重做标志对应的函数项，则忽略该条记录
			KFS_LOG_VA_ERROR("Unable to replay %s", line);
			ist.clear();
			continue;
		}

		// 执行对应的函数
		iter->second(ist);
		ist.clear();
	}
	ifs.close();
}
