//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: ChunkServer_main.cc 77 2008-07-12 01:23:54Z sriramsrao $
//
// Created 2006/03/22
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

extern "C"
{
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
}

#include <string>
#include <vector>

#include "common/properties.h"
#include "libkfsIO/DiskManager.h"
#include "libkfsIO/NetManager.h"
#include "libkfsIO/Globals.h"

#include "ChunkServer.h"
#include "ChunkManager.h"

using namespace KFS;
using namespace KFS::libkfsio;
using std::string;
using std::vector;
using std::cout;
using std::endl;

string gLogDir;
vector<string> gChunkDirs;

ServerLocation gMetaServerLoc;
int64_t gTotalSpace; // max. storage space to use
int gChunkServerClientPort; // Port at which kfs clients connect to us

/// 全局属性
Properties gProp;
const char *gClusterKey;
int gChunkServerRackId;
int gChunkServerCleanupOnStart;

int ReadChunkServerProperties(char *fileName);

/// 命令行：<progm name> <Properties file> [log location]
/// 进行一系列的初始化活动后，进入常驻程序。初始化的内容见代码中注释：
int main(int argc, char **argv)
{
	// 如果输入的命令参数少于2，则提示用法
	if (argc < 2)
	{
		cout << "Usage: " << argv[0] << " <properties file> {<msg log file>}"
				<< endl;
		exit(0);
	}

	if (argc > 2)
	{
		KFS::MsgLogger::Init(argv[2]);
	}
	else
	{
		KFS::MsgLogger::Init(NULL);
	}

	// 1. chunkServer初始化
	// 读入chunkserver有关的属性，详细内容见该函数注释
	if (ReadChunkServerProperties(argv[1]) != 0)
	{
		cout << "Bad properties file: " << argv[1] << " aborting...\n";
		exit(-1);
	}

	// 2. 全局变量初始化
	// 初始化系统的全局变量
	libkfsio::InitGlobals();

	// for writes, the client is sending WRITE_PREPARE with 64K bytes;
	// to enable the data to fit into a single buffer (and thereby get
	// the data to the underlying FS via a single aio_write()), allocate a bit
	// of extra space. 64K + 4k
	// 3. 设置IOBuffer的大小
	/// 设置IOBuffer的缓存为64k + 4k
	libkfsio::SetIOBufferSize(69632);

	// 4. diskManager初始化
	/// 初始化AIO，用于磁盘异步IO
	globals().diskManager.InitForAIO();

	// would like to limit to 200MB outstanding
	// globals().netManager.SetBacklogLimit(200 * 1024 * 1024);

	// 5. chunkServer初始化
	gChunkServer.Init();

	// 6. chunkManager初始化
	// 初始化chunkManager
	gChunkManager.Init(gChunkDirs, gTotalSpace);

	// 7. Logger初始化
	// 初始化日志记录器：Logger
	gLogger.Init(gLogDir);

	// 8. 设置MetaServerSM
	gMetaServerSM.SetMetaInfo(gMetaServerLoc, gClusterKey, gChunkServerRackId);

	signal(SIGPIPE, SIG_IGN);

	// gChunkServerCleanupOnStart is a debugging option---it provides
	// "silent" cleanup
	if (gChunkServerCleanupOnStart == 0)
	{
		gChunkManager.Restart();
	}

	// 9. 初始化完成，进入常驻程序
	gChunkServer.MainLoop(gChunkServerClientPort);

	return 0;
}

/// 如果指定目录不存在，则创建该目录
static bool make_if_needed(const char *dirname)
{
	struct stat s;

	// stat查询成功返回0
	// 如果当前查询的dirname是目录，则直接返回即可
	if (stat(dirname, &s) == 0 && S_ISDIR(s.st_mode))
		return true;

	return mkdir(dirname, 0755) == 0;
}

///
/// Read and validate the configuration settings for the chunk
/// server. The configuration file is assumed to contain lines of the
/// form: xxx.yyy.zzz = <value>
/// @result 0 on success; -1 on failure
/// @param[in] fileName File that contains configuration information
/// for the chunk server.
///
/// 从属性文件中读入chunk的属性，包括：metaserver的主机名和端口, 用户连接本chunk使用的端口,
/// chunk文件的存储路径, 日志文件所在的目录, 总共可用空间数, gChunkServerCleanupOnStart,
/// gChunkServerRackId, gClusterKey, 日志权限等
int ReadChunkServerProperties(char *fileName)
{
	string::size_type curr = 0, next;
	string chunkDirPaths;
	string logLevel;
#ifdef NDEBUG
	const char *defLogLevel = "INFO";
#else
	const char *defLogLevel = "DEBUG";
#endif

	// 从属性文件中读入chunk属性
	if (gProp.loadProperties(fileName, '=', true) != 0)
		return -1;

	// 从属性值中提取出metaserver的主机名和端口
	gMetaServerLoc.hostname = gProp.getValue("chunkServer.metaServer.hostname",
			"");
	gMetaServerLoc.port = gProp.getValue("chunkServer.metaServer.port", -1);
	if (!gMetaServerLoc.IsValid())
	{
		cout << "Aborting...bad meta-server host or port: ";
		cout << gMetaServerLoc.hostname << ':' << gMetaServerLoc.port << '\n';
		return -1;
	}

	// 提取用户连接本chunk使用的端口
	gChunkServerClientPort = gProp.getValue("chunkServer.clientPort", -1);
	if (gChunkServerClientPort < 0)
	{
		cout << "Aborting...bad client port: " << gChunkServerClientPort
				<< '\n';
		return -1;
	}
	cout << "Using chunk server client port: " << gChunkServerClientPort
			<< '\n';

	// Paths are space separated directories for storing chunks
	// chunk文件的存储路径
	chunkDirPaths = gProp.getValue("chunkServer.chunkDir", "chunks");

	// 从chunkDirPaths中逐个提取出chunk文件所在的路径；如果指定路径不存在，则创建目录
	while (curr < chunkDirPaths.size())
	{
		string component;

		// 提取配置文件中多个chunk目录（中间用空格隔开）
		next = chunkDirPaths.find(' ', curr);
		if (next == string::npos)
			next = chunkDirPaths.size();

		// 提取一个路径
		component.assign(chunkDirPaths, curr, next - curr);

		curr = next + 1;

		// 忽略空路径
		if ((component == " ") || (component == ""))
		{
			continue;
		}

		// 如果当前chunk目录不存在，则创建该目录
		if (!make_if_needed(component.c_str()))
		{
			cout << "Aborting...failed to create " << component << '\n';
			return -1;
		}

		// also, make the directory for holding stale chunks in each "partition"
		// 同时创建（如果不存在）对应的回收站目录
		string staleChunkDir = GetStaleChunkPath(component);
		make_if_needed(staleChunkDir.c_str());

		cout << "Using chunk dir = " << component << '\n';

		gChunkDirs.push_back(component);
	}

	// 获取日志文件所在的目录，创建如果不存在（create if not exists）
	gLogDir = gProp.getValue("chunkServer.logDir", "logs");
	if (!make_if_needed(gLogDir.c_str()))
	{
		cout << "Aborting...failed to create " << gLogDir << '\n';
		return -1;
	}
	cout << "Using log dir = " << gLogDir << '\n';

	// 获取总共可用空间数
	gTotalSpace = gProp.getValue("chunkServer.totalSpace", (long long) 0);
	cout << "Total space = " << gTotalSpace << '\n';

	/// 该全局变量功能未知
	gChunkServerCleanupOnStart
			= gProp.getValue("chunkServer.cleanupOnStart", 0);
	cout << "cleanup on start = " << gChunkServerCleanupOnStart << endl;

	/// 该全局变量功能未知
	gChunkServerRackId = gProp.getValue("chunkServer.rackId", (int) -1);
	cout << "Chunk server rack: " << gChunkServerRackId << endl;

	/// 建立同metaserver的联系时， 需要发送的信息
	gClusterKey = gProp.getValue("chunkServer.clusterKey", "");
	cout << "using cluster key = " << gClusterKey << endl;

	// 设置日志权限（@see Logger)
	logLevel = gProp.getValue("chunkServer.loglevel", defLogLevel);
	if (logLevel == "INFO")
		KFS::MsgLogger::SetLevel(log4cpp::Priority::INFO);
	else
		KFS::MsgLogger::SetLevel(log4cpp::Priority::DEBUG);

	return 0;
}
