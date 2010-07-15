//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: LayoutManager.cc 153 2008-09-17 19:08:16Z sriramsrao $
//
// Created 2006/06/06
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
// \file LayoutManager.cc
// \brief Handlers for chunk layout.
//
//----------------------------------------------------------------------------

#include <algorithm>
#include <functional>
#include <sstream>

#include "LayoutManager.h"
#include "kfstree.h"
#include "libkfsIO/Globals.h"

using std::for_each;
using std::find;
using std::ptr_fun;
using std::mem_fun;
using std::mem_fun_ref;
using std::bind2nd;
using std::sort;
using std::random_shuffle;
using std::remove_if;
using std::set;
using std::vector;
using std::map;
using std::min;
using std::endl;
using std::istringstream;

using namespace KFS;
using namespace KFS::libkfsio;

LayoutManager KFS::gLayoutManager;
/// Max # of concurrent read/write replications per node
///  -- write: is the # of chunks that the node can pull in from outside
///  -- read: is the # of chunks that the node is allowed to send out
///
/// 一个chunkserver同时允许的进行写或读复制的操作数
const int MAX_CONCURRENT_WRITE_REPLICATIONS_PER_NODE = 5;
const int MAX_CONCURRENT_READ_REPLICATIONS_PER_NODE = 10;

///
/// When placing chunks, we see the space available on the node as well as
/// we take our estimate of the # of writes on
/// the node as a hint for choosing servers; if a server is "loaded" we should
/// avoid sending traffic to it.  This value defines a watermark after which load
/// begins to be an issue.
///
/// 定义一个chunkserver最多同时进行的写操作数目，目的是为了实现流量均衡，避免单个chunkserver
/// 过载
const uint32_t CONCURRENT_WRITES_PER_NODE_WATERMARK = 10;

///
/// For disk space utilization balancing, we say that a server
/// is "under utilized" if is below 30% full; we say that a server
/// is "over utilized" if it is above 70% full.  For rebalancing, we
/// move data from servers that are over-utilized to servers that are
/// under-utilized.  These #'s are intentionally set conservatively; we
/// don't want the system to constantly move stuff between nodes when
/// there isn't much to be gained by it.
///
/// 定义理论磁盘使用比例理想值的上限和下限，用于实现存储均衡
const float MIN_SERVER_SPACE_UTIL_THRESHOLD = 0.3;
const float MAX_SERVER_SPACE_UTIL_THRESHOLD = 0.9;

#if 0
const float MIN_SERVER_SPACE_UTIL_THRESHOLD = 0.5;
const float MAX_SERVER_SPACE_UTIL_THRESHOLD = 0.6;
#endif

/// Helper functor that can be used to find a chunkid from a vector
/// of meta chunk info's.
/// 两个chunk ID相同，则说明这两个chunk相同
class ChunkIdMatcher
{
	chunkId_t myid;
public:
	ChunkIdMatcher(chunkId_t c) :
		myid(c)
	{
	}
	bool operator()(MetaChunkInfo *c)
	{
		return c->chunkId == myid;
	}
};

/// 初始化：除完成一些基本数据的初始化外，需要建立起各个计数器，并注册到
/// globals().counterManager中
LayoutManager::LayoutManager() :
	mLeaseId(1), mNumOngoingReplications(0), mIsRebalancingEnabled(false),
			mIsExecutingRebalancePlan(false), mLastChunkRebalanced(1),
			mLastChunkReplicated(1), mRecoveryStartTime(0),
			mMinChunkserversToExitRecovery(1)
{
	pthread_mutex_init(&mChunkServersMutex, NULL);

	mReplicationTodoStats = new Counter("Num Replications Todo");
	mOngoingReplicationStats = new Counter("Num Ongoing Replications");
	mTotalReplicationStats = new Counter("Total Num Replications");
	mFailedReplicationStats = new Counter("Num Failed Replications");
	mStaleChunkCount = new Counter("Num Stale Chunks");
	// how much to be done before we are done
	globals().counterManager.AddCounter(mReplicationTodoStats);
	// how much are we doing right now
	globals().counterManager.AddCounter(mOngoingReplicationStats);
	globals().counterManager.AddCounter(mTotalReplicationStats);
	globals().counterManager.AddCounter(mFailedReplicationStats);
	globals().counterManager.AddCounter(mStaleChunkCount);
}

/// 用于比较ServerLocation是否相同：hostname和端口相同
class MatchingServer
{
	ServerLocation loc;
public:
	MatchingServer(const ServerLocation &l) :
		loc(l)
	{
	}
	bool operator()(ChunkServerPtr &s)
	{
		return s->MatchingServer(loc);
	}
};

//
// Try to match servers by hostname: for write allocation, we'd like to place
// one copy of the block on the same host on which the client is running.
//
/// 比较两个Server是否相同：hostname相同
class MatchServerByHost
{
	string host;
public:
	MatchServerByHost(const string &s) :
		host(s)
	{
	}
	bool operator()(ChunkServerPtr &s)
	{
		ServerLocation l = s->GetServerLocation();

		return l.hostname == host;
	}
};

/// Add the newly joined server to the list of servers we have.  Also,
/// update our state to include the chunks hosted on this server.
/*
 * 将r对应的chunkserver加入到metaserver中:
 * 1. 检查该server是否已经存在于内核当中了;
 * 2. 将r中包含的chunk信息导入到内核当中;
 * 3. 将r中的server加入到chunkserver列表当中;
 * 4. 将该server加入到对应机架上.
 */
void LayoutManager::AddNewServer(MetaHello *r)
{
	ChunkServerPtr s;
	vector<chunkId_t> staleChunkIds;
	vector<ChunkInfo>::size_type i;
	vector<ChunkServerPtr>::iterator j;
	uint64_t allocSpace = r->chunks.size() * CHUNKSIZE;

	if (r->server->IsDown())
		return;

	// 根据MetaHello创建一个ChunkServer
	s = r->server;
	s->SetServerLocation(r->location);
	s->SetSpace(r->totalSpace, r->usedSpace, allocSpace);
	s->SetRack(r->rackId);

	// If a previously dead server reconnects, reuse the server's
	// position in the list of chunk servers.  This is because in
	// the chunk->server mapping table, we use the chunkserver's
	// position in the list of connected servers to find it.
	//
	// 如果一个chunkserver是因为当机等原因重新加入，则使用它原来在mChunkServers中占用的目录
	// 是否应该将这一段加到最上边？
	j = find_if(mChunkServers.begin(), mChunkServers.end(), MatchingServer(
			r->location));

	// 说明tree中本来就存在此目录
	if (j != mChunkServers.end())
	{
		/// 说明该chunk是重新加入的
		KFS_LOG_VA_DEBUG("Duplicate server: %s, %d",
				r->location.hostname.c_str(), r->location.port);
		return;
	}

	// 说明这个chunkserver是新加入进来的，需要将其中的chunk信息列表导入到本地
	// r->chunks中存放服务器中所有的chunk信息
	for (i = 0; i < r->chunks.size(); ++i)
	{
		// MetaChunkInfo: chunk简洁信息，包括offset(within the file), chunk ID
		// 和chunk version.
		vector<MetaChunkInfo *> v;
		vector<MetaChunkInfo *>::iterator chunk;
		int res = -1;

		// 将该chunk所属的文件的所有chunk取出, 存放到v中
		metatree.getalloc(r->chunks[i].fileId, v);

		// 再从v中查找对应的chunk...
		chunk = find_if(v.begin(), v.end(),
				ChunkIdMatcher(r->chunks[i].chunkId));
		if (chunk != v.end())
		{
			MetaChunkInfo *mci = *chunk;

			// 如果本地(metaserver中)chunk的版本号 <= r中chunk的版本号, 则执行以下操作
			if (mci->chunkVersion <= r->chunks[i].chunkVersion)
			{
				// This chunk is non-stale.  Verify that there are
				// sufficient copies; if there are too many, nuke some.
				// 将chunk插入到"需要验证的chunk队列"中
				ChangeChunkReplication(r->chunks[i].chunkId);

				// 将chunk的chunkserver加入到chunk的server队列中
				res = UpdateChunkToServerMapping(r->chunks[i].chunkId, s.get());
				assert(res >= 0);

				// get the chunksize for the last chunk of fid
				// stored on this server
				MetaFattr *fa = metatree.getFattr(r->chunks[i].fileId);
				if (fa->filesize < 0)
				{
					// 获取文件的属性失败, 则设置lastChunk为v中的最后一个chunk
					MetaChunkInfo *lastChunk = v.back();
					// 如果当前的chunk就是最后一个chunk, 则发送命令获取该chunk的大小
					if (lastChunk->chunkId == r->chunks[i].chunkId)
						s->GetChunkSize(r->chunks[i].fileId,
								r->chunks[i].chunkId);
				}

				// 如果在内核中的mci的版本号小于r中的版本号, 则需要更新chunkversion
				if (mci->chunkVersion < r->chunks[i].chunkVersion)
				{
					// version #'s differ.  have the chunkserver reset
					// to what the metaserver has.
					// XXX: This is all due to the issue with not logging
					// the version # that the metaserver is issuing.  What is going
					// on here is that,
					//  -- client made a request
					//  -- metaserver bumped the version; notified the chunkservers
					//  -- the chunkservers write out the version bump on disk
					//  -- the metaserver gets ack; writes out the version bump on disk
					//  -- and then notifies the client
					// Now, if the metaserver crashes before it writes out the
					// version bump, it is possible that some chunkservers did the
					// bump, but not the metaserver.  So, fix up.  To avoid other whacky
					// scenarios, we increment the chunk version # by the incarnation stuff
					// to avoid reissuing the same version # multiple times.
					// 如果metaserver正在更新版本号时崩溃, 则chunkversion会有错误
					s->NotifyChunkVersChange(r->chunks[i].fileId,
							r->chunks[i].chunkId, mci->chunkVersion);

				}
			}
			else
			{
				KFS_LOG_VA_INFO("Old version for chunk id = %lld => stale",
						r->chunks[i].chunkId);
			}
		}

		// 如果UpdateChunkToServerMapping()出现错误, 则删除对应的chunk: 先放到
		// staleChunkIds中等待删除
		if (res < 0)
		{
			/// stale chunk
			KFS_LOG_VA_INFO("Non-existent chunk id = %lld => stale",
					r->chunks[i].chunkId);
			staleChunkIds.push_back(r->chunks[i].chunkId);
			mStaleChunkCount->Update(1);
		}
	}

	if (staleChunkIds.size() > 0)
	{
		// 通知chunkserver删除指定的chunk
		s->NotifyStaleChunks(staleChunkIds);
	}

	// prevent the network thread from wandering this list while we change it.
	pthread_mutex_lock(&mChunkServersMutex);

	// 将该chunkserver加入到chunkserver列表中
	mChunkServers.push_back(s);

	pthread_mutex_unlock(&mChunkServersMutex);

	vector<RackInfo>::iterator rackIter;

	// 把该server加入到对应的机架上
	rackIter = find_if(mRacks.begin(), mRacks.end(), RackMatcher(r->rackId));
	if (rackIter != mRacks.end())
	{
		rackIter->addServer(s);
	}
	else
	{
		// 如果这个server不属于任何已经存在的机架, 则创建新的机架, 并加入到机架列表中
		RackInfo ri(r->rackId);
		ri.addServer(s);
		mRacks.push_back(ri);
	}

	// Update the list since a new server is in
	CheckHibernatingServersStatus();
}

/// 清理map中的指定server的chunk数据: 删除p.second.chunkServers中从target开始到结尾的
/// 所有server, 然后把有关数据存放在cmap和crset中
class MapPurger
{
	// std::map<chunkId_t, ChunkPlacementInfo> CSMap
	CSMap &cmap;

	// std::set<chunkId_t> CRCandidateSet;
	CRCandidateSet &crset;
	const ChunkServer *target;	// 将要删除的ChunkServer
public:
	MapPurger(CSMap &m, CRCandidateSet &c, const ChunkServer *t) :
		cmap(m), crset(c), target(t)
	{
	}

	/// 删除p.second中从target开始的所有server
	void operator ()(const map<chunkId_t, ChunkPlacementInfo>::value_type p)
	{
		ChunkPlacementInfo c = p.second;

		// 删除c.chunkServers中从target开始到结尾的所有server
		c.chunkServers.erase(remove_if(c.chunkServers.begin(),
				c.chunkServers.end(), ChunkServerMatcher(target)),
				c.chunkServers.end());
		cmap[p.first] = c;
		// we need to check the replication level of this chunk
		crset.insert(p.first);
	}
};

/// 休眠指定的chunk: 把map中的chunkID存放到指定的chunk列表中和ChunkServer的
/// EvacuateChunk列表中
class MapRetirer
{
	CSMap &cmap;
	CRCandidateSet &crset;
	ChunkServer *retiringServer;
public:
	MapRetirer(CSMap &m, CRCandidateSet &c, ChunkServer *t) :
		cmap(m), crset(c), retiringServer(t)
	{
	}
	void operator ()(const map<chunkId_t, ChunkPlacementInfo>::value_type p)
	{
		ChunkPlacementInfo c = p.second;
		vector<ChunkServerPtr>::iterator i;

		// 查看当前的p是否包含指定的chunkserver(retireingServer)
		i = find_if(c.chunkServers.begin(), c.chunkServers.end(),
				ChunkServerMatcher(retiringServer));

		if (i == c.chunkServers.end())
			return;

		// we need to check the replication level of this chunk
		crset.insert(p.first);
		retiringServer->EvacuateChunk(p.first);
	}
};

/// 将指定ChunkPlacementInfo通过ofstream输出到文件中, 输出的信息包括: id, chunkservers
class MapDumper
{
	ofstream &ofs;
public:
	MapDumper(ofstream &o) :
		ofs(o)
	{
	}
	void operator ()(const map<chunkId_t, ChunkPlacementInfo>::value_type p)
	{
		chunkId_t cid = p.first;
		ChunkPlacementInfo c = p.second;

		ofs << cid << ' ' << c.fid << ' ' << c.chunkServers.size() << ' ';
		for (uint32_t i = 0; i < c.chunkServers.size(); i++)
		{
			ofs << c.chunkServers[i]->ServerID() << ' '
					<< c.chunkServers[i]->GetRack() << ' ';
		}
		ofs << endl;
	}
};

//
// Dump out the chunk block map to a file.  The output can be used in emulation
// modes where we setup the block map and experiment.
//
// 将mChunkToServerMap中的所有chunk的服务器信息输出到文件"chunkmap.txt"中
void LayoutManager::DumpChunkToServerMap()
{
	ofstream ofs;

	ofs.open("chunkmap.txt");

	for_each(mChunkToServerMap.begin(), mChunkToServerMap.end(), MapDumper(ofs));
	ofs.flush();
	ofs.close();
}

/// 关闭一个服务器: 可能是由于系统要求该服务器休眠, 也可能是由于服务器当机
void LayoutManager::ServerDown(ChunkServer *server)
{
	// 1. 在chunkserver的列表中查找给定的server.
	vector<ChunkServerPtr>::iterator i = find_if(mChunkServers.begin(),
			mChunkServers.end(), ChunkServerMatcher(server));

	if (i == mChunkServers.end())
		return;

	vector<RackInfo>::iterator rackIter;

	// 2. 在机架列表中查找该server所在的机架.
	rackIter = find_if(mRacks.begin(), mRacks.end(), RackMatcher(
			server->GetRack()));
	if (rackIter != mRacks.end())
	{
		rackIter->removeServer(server);
	}

	/// Fail all the ops that were sent/waiting for response from
	/// this server.
	// 3. 将该server中所有的操作撤销(状态设置为Error)
	server->FailPendingOps();

	// check if this server was sent to hibernation
	// 查看该server是否处于休眠状态: 从mHibernatingServers列表中查询
	bool isHibernating = false;
	for (uint32_t j = 0; j < mHibernatingServers.size(); j++)
	{
		if (mHibernatingServers[j].location == server->GetServerLocation())
		{
			// 如果这个server是在mHibernatingServers列表中(即, 服务器发送命令让该服务器
			// 休眠):

			// record all the blocks that need to be checked for
			// re-replication later
			// 将处理结果存放在mChunkToServerMap(cmap)和
			// mHibernatingServers.block(crset)中.
			MapPurger purge(mChunkToServerMap, mHibernatingServers[j].blocks,
					server);

			// 搜索其中所有的chunk->server的映像, 删除和server匹配的项. 改动的chunk存放在
			// mHibernatingServers[j].blocks中,
			for_each(mChunkToServerMap.begin(), mChunkToServerMap.end(), purge);
			isHibernating = true;
			break;
		}
	}

	if (!isHibernating)
	{
		// 如果server没有hibernating, 则从mChunkToServerMap中删除对应的server, 然后
		// 添加到mChunkReplicationCandidates中, 以确认chunk的replicator数是否符合要求
		MapPurger purge(mChunkToServerMap, mChunkReplicationCandidates, server);
		for_each(mChunkToServerMap.begin(), mChunkToServerMap.end(), purge);
	}

	// for reporting purposes, record when it went down
	// 关闭服务器完成, 写入日志
	time_t now = time(NULL);
	string downSince = timeToStr(now);
	ServerLocation loc = server->GetServerLocation();

	const char *reason;
	if (isHibernating)
		reason = "Hibernated";
	else if (server->IsRetiring())
		reason = "Retired";
	else
		reason = "Unreachable";

	mDownServers << "s=" << loc.hostname << ", p=" << loc.port << ", down="
			<< downSince << ", reason=" << reason << "\t";

	// 从服务器列表中删除该服务器
	mChunkServers.erase(i);
}

/// 让指定的服务器休眠downtime时间(用于服务器维护或者其他)
int LayoutManager::RetireServer(const ServerLocation &loc, int downtime)
{
	ChunkServerPtr retiringServer;
	vector<ChunkServerPtr>::iterator i;

	// 从服务器列表中找到该服务器
	i = find_if(mChunkServers.begin(), mChunkServers.end(),
					MatchingServer(loc));
	if (i == mChunkServers.end())
		return -1;

	retiringServer = *i;

	// 设置isRetiring为真
	retiringServer->SetRetiring();
	if (downtime > 0)
	{
		HibernatingServerInfo_t hsi;

		hsi.location = retiringServer->GetServerLocation();
		hsi.sleepEndTime = time(0) + downtime;
		mHibernatingServers.push_back(hsi);

		// 发送retire命令到pending队列中
		retiringServer->Retire();

		return 0;
	}

	// 构造一个MapRetirer, 用来终止
	MapRetirer retirer(mChunkToServerMap, mChunkReplicationCandidates,
			retiringServer.get());
	// 把存储在retiringServer中的所有chunk加入到mChunkReplicationCandidates中,
	// 等待检验冗余度是否达到了要求
	for_each(mChunkToServerMap.begin(), mChunkToServerMap.end(), retirer);

	return 0;
}

/*
 * Chunk-placement algorithm is rack-aware. At a high-level, the algorithm tries
 * to keep at most 1 copy of a chunk per rack:
 *  - Sort the racks based on space
 *  - From each rack, find one or more candidate servers
 * This approach will work when we are placing a chunk for the first time.
 * Whenever we need to copy/migrate a chunk, the intent is to keep the chunk
 * migration traffic within the same rack.  Specifically:
 * 1. Need to re-replicate a chunk because a node is dead
 *      -- here, we exclude the 2 racks on which the chunk is already placed and
 *      try to find a new spot.  By the time we get to finding a spot, we have
 *      removed the info about where the chunk was.  So, put it on a unique rack
 * 2. Need to re-replicate a chunk because a node is retiring
 *	-- here, since we know which node is retiring and the rack it is on, we
 *	can keep the new spot to be on the same rack
 * 3. Need to re-replicate a chunk because we are re-balancing amongst nodes
 *	-- we move data between nodes in the same rack
 *	-- if we a new rack got added, we move data between racks; here we need
 *	   to be a bit careful: in one iteration, we move data from one rack to
 *	   a newly added rack; in the next iteration, we could move another copy
 *	   of the chunk to the same rack...fill this in.
 * If we can't place the 3 copies on 3 different racks, we put the moved data
 * whereever we can find a spot.  (At a later time, we'll need the fix code: if
 * a new rack becomes available, we move the 3rd copy to the new rack and get
 * the copies on different racks).
 *
 */

/*
 * Return an ordered list of candidate racks
 */
/// 将mRack随机摆放, 然后提取出mRacks中所有的ID信息
void LayoutManager::FindCandidateRacks(vector<int> &result)
{
	set<int> dummy;

	FindCandidateRacks(result, dummy);
}

/// 将mRack随机摆放, 然后提取出mRacks中除excludes中的rack的ID信息
void LayoutManager::FindCandidateRacks(vector<int> &result,
		const set<int> &excludes)
{
	set<int>::const_iterator iter;

	result.clear();
	// sort(mRacks.begin(), mRacks.end());
	random_shuffle(mRacks.begin(), mRacks.end());
	for (uint32_t i = 0; i < mRacks.size(); i++)
	{
		if (!excludes.empty())
		{
			iter = excludes.find(mRacks[i].id());
			if (iter != excludes.end())
				// rack is in the exclude list
				continue;
		}
		result.push_back(mRacks[i].id());
	}
}

/// server的空间估计, 可进行排序, 排序规则见()函数
struct ServerSpace
{
	uint32_t serverIdx;		// server的ID
	uint32_t loadEstimate;	// 负载估计
	uint64_t availSpace;	// 可用空间
	uint64_t usedSpace;		// 已用空间

	// sort in decreasing order: Prefer the server with more free
	// space, or in the case of a tie, the one with less used space.
	// also, prefer servers that are lightly loaded

	// 用于排序: loadEstimate小, 可用空间大, 已用空间小的排在前面
	bool operator <(const ServerSpace &other) const
	{
		// 如果负载大于并发访问限制, 并且两者的负载不同, 则负载小的排在前面
		if ((loadEstimate > CONCURRENT_WRITES_PER_NODE_WATERMARK)
				&& (loadEstimate != other.loadEstimate))
		{
			// prefer server that is "lightly" loaded
			return loadEstimate < other.loadEstimate;
		}

		// 如果负载没有超过最大允许并发量或者负载相同, 则可用空间大的排在前面
		if (availSpace != other.availSpace)
			return availSpace > other.availSpace;
		// 如果负载没有超过最大允许并发量或者负载相同且可用空间量相同, 则已用空间少的排在前面
		else
			return usedSpace < other.usedSpace;
	}
};

/// 排序的时候, 按utilization大小排序
struct ServerSpaceUtil
{
	uint32_t serverIdx;
	float utilization;

	// sort in increasing order of space utilization
	bool operator <(const ServerSpaceUtil &other) const
	{
		return utilization < other.utilization;
	}
};

/// 从指定机架(如果rackId已指定)或者所有已连接的服务器中(未指定rackId)查找候选服务器
/// 候选规则: 符合指定机架的, 可以作为候选的, 不包含在excludes中的, 空间利用率低于指定
/// 值的服务器
void LayoutManager::FindCandidateServers(vector<ChunkServerPtr> &result,
		const vector<ChunkServerPtr> &excludes, int rackId)
{
	if (mChunkServers.size() < 1)
		return;

	if (rackId > 0)
	{
		vector<RackInfo>::iterator rackIter;

		// 从mRacks查找rackId指定的机架
		rackIter = find_if(mRacks.begin(), mRacks.end(), RackMatcher(rackId));

		// 如果该机架在mRacks列表中, 则从该机架中的服务器中查找
		if (rackIter != mRacks.end())
		{
			FindCandidateServers(result, rackIter->getServers(), excludes,
					rackId);
			return;
		}
	}

	// 如果没有指定rackId或者rackId不存在, 则从所有服务器中查找
	FindCandidateServers(result, mChunkServers, excludes, rackId);
}

/// 判断一个服务器是否可以作为候选服务器: 可用空间小于一个CHUNKSIZE, 或者没有相应(没有心跳的
/// 回复), 或者正要休眠, 均不可作为候选服务器
static bool IsCandidateServer(ChunkServerPtr &c)
{
	if ((c->GetAvailSpace() < ((uint64_t) CHUNKSIZE))
			|| (!c->IsResponsiveServer()) || (c->IsRetiring()))
	{
		// one of: no space, non-responsive, retiring...we leave
		// the server alone
		return false;
	}
	return true;
}

/// 从指定的服务器列表(source)中选择符合指定机架的, 可以作为候选的: 不包含在excludes中的, 空间利用率
/// 低于指定值的服务器作为候选服务器, 通过result返回.
void LayoutManager::FindCandidateServers(vector<ChunkServerPtr> &result,
		const vector<ChunkServerPtr> &sources,
		const vector<ChunkServerPtr> &excludes, int rackId)
{
	// 如果指定的服务器列表为空, 则直接返回退出函数
	if (sources.size() < 1)
		return;

	vector<ChunkServerPtr> candidates;
	vector<ChunkServerPtr>::size_type i;
	vector<ChunkServerPtr>::const_iterator iter;

	for (i = 0; i < sources.size(); i++)
	{
		ChunkServerPtr c = sources[i];

		// 如果已经指定了机架, 而这个服务器机架号不符合该机架号, 则跳过
		if ((rackId >= 0) && (c->GetRack() != rackId))
			continue;

		// 如果该服务器不能作为候选服务器(空间不足等), 则跳过
		if (!IsCandidateServer(c))
			continue;

		// 如果excludes不为空, 则需要判断该服务器是否在例外列表中
		if (excludes.size() > 0)
		{
			iter = find(excludes.begin(), excludes.end(), c);
			if (iter != excludes.end())
			{
				continue;
			}
		}
		// XXX: temporary measure: take only under-utilized servers
		// we need to move a model where we give preference to
		// under-utilized servers
		// 跳过空间使用量大的服务器
		if (c->GetSpaceUtilization() > MAX_SERVER_SPACE_UTIL_THRESHOLD)
			continue;

		// 符合指定机架的, 可以作为候选的, 不包含在excludes中的, 空间利用率低于指定值的服务器
		candidates.push_back(c);
	}
	if (candidates.size() == 0)
		return;
	random_shuffle(candidates.begin(), candidates.end());
	for (i = 0; i < candidates.size(); i++)
	{
		result.push_back(candidates[i]);
	}
}

#if 0
void
LayoutManager::FindCandidateServers(vector<ChunkServerPtr> &result,
		const vector<ChunkServerPtr> &sources,
		const vector<ChunkServerPtr> &excludes,
		int rackId)
{
	if (sources.size() < 1)
	return;

	vector<ServerSpace> ss;
	vector<ChunkServerPtr>::size_type i, j;
	vector<ChunkServerPtr>::const_iterator iter;

	ss.resize(sources.size());

	for (i = 0, j = 0; i < sources.size(); i++)
	{
		ChunkServerPtr c = sources[i];

		if ((rackId >= 0) && (c->GetRack() != rackId))
		continue;
		if ((c->GetAvailSpace() < ((uint64_t) CHUNKSIZE)) || (!c->IsResponsiveServer())
				|| (c->IsRetiring()))
		{
			// one of: no space, non-responsive, retiring...we leave
			// the server alone
			continue;
		}
		if (excludes.size()> 0)
		{
			iter = find(excludes.begin(), excludes.end(), c);
			if (iter != excludes.end())
			{
				continue;
			}
		}
		ss[j].serverIdx = i;
		ss[j].availSpace = c->GetAvailSpace();
		ss[j].usedSpace = c->GetUsedSpace();
		ss[j].loadEstimate = c->GetNumChunkWrites();
		j++;
	}

	if (j == 0)
	return;

	ss.resize(j);

	sort(ss.begin(), ss.end());

	result.reserve(ss.size());
	for (i = 0; i < ss.size(); ++i)
	{
		result.push_back(sources[ss[i].serverIdx]);
	}
}
#endif

/// 将列表中的服务器按空间使用情况排序, 空间使用量小的靠前
void LayoutManager::SortServersByUtilization(vector<ChunkServerPtr> &servers)
{
	vector<ServerSpaceUtil> ss;
	vector<ChunkServerPtr> temp;

	ss.resize(servers.size());
	temp.resize(servers.size());

	for (vector<ChunkServerPtr>::size_type i = 0; i < servers.size(); i++)
	{
		// 标记该server在原来数组中的位置
		ss[i].serverIdx = i;
		// 取出每一个server的使用空间数
		ss[i].utilization = servers[i]->GetSpaceUtilization();
		temp[i] = servers[i];
	}

	sort(ss.begin(), ss.end());
	for (vector<ChunkServerPtr>::size_type i = 0; i < servers.size(); i++)
	{
		servers[i] = temp[ss[i].serverIdx];
	}
}

///
/// The algorithm for picking a set of servers to hold a chunk is: (1) pick
/// the server with the most amount of free space, and (2) to break
/// ties, pick the one with the least amount of used space.  This
/// policy has the effect of doing round-robin(轮换调度) allocations.  The
/// allocated space is something that we track.  Note: We rely on the
/// chunk servers to tell us how much space is used up on the server.
/// Since servers can respond at different rates, doing allocations
/// based on allocated space ensures equitable distribution;
/// otherwise, if we were to do allocations based on the amount of
/// used space, then a slow responding server will get pummelled with
/// lots of chunks (i.e., used space will be updated on the meta
/// server at a slow rate, causing the meta server to think that the
/// chunk server has lot of space available).
///
/*
 * 分配chunk空间:
 * 1. 如果请求chunk分配的主机也是一个chunkserver, 则优先将这个主机加入到要分配的服务器列表中;
 * 2. 从每一个机架上选择合适数量的服务器作为备用服务器;
 * 3. 设置其中一台服务器作为master;
 * 4. 为master分配一个Lease, 并向其发送写命令;
 * 5. 改变chunk的Replicator数目, 自动启动冗余复制操作.
 */
int LayoutManager::AllocateChunk(MetaAllocate *r)
{
	vector<ChunkServerPtr>::size_type i;
	vector<int> racks;

	if (r->numReplicas == 0)
	{
		// huh? allocate a chunk with 0 replicas???
		return -EINVAL;
	}

	// 获取候选机架, 存放到racks中
	FindCandidateRacks(racks);
	if (racks.size() == 0)
		return -ENOSPC;

	// 为r分配服务器存储空间
	r->servers.reserve(r->numReplicas);

	// 每一个机架上要分配的服务器数量
	uint32_t numServersPerRack = r->numReplicas / racks.size();

	// 如果不能整除, 则每一个机架上要分配的服务器数量加1
	if (r->numReplicas % racks.size())
		numServersPerRack++;

	// take the server local to the machine on which the client is to
	// make that the master; this avoids a network transfer
	ChunkServerPtr localserver;
	vector<ChunkServerPtr>::iterator j;

	// 在已经连接的chunkserver列表中查找和r主机名称相同的服务器
	j = find_if(mChunkServers.begin(), mChunkServers.end(), MatchServerByHost(
			r->clientHost));
	// 如果请求分配chunk的clientHost在chunkserver列表中
	if ((j != mChunkServers.end()) && (IsCandidateServer(*j)))
		localserver = *j;

	// 如果请求chunk分配的client也是一个chunkserver, 则首先将自身加入到候选服务器列表中
	if (localserver)
		r->servers.push_back(localserver);

	// 从每一个候选机架上选择出指定数量的候选服务器
	for (uint32_t idx = 0; idx < racks.size(); idx++)
	{
		vector<ChunkServerPtr> candidates, dummy;

		if (r->servers.size() >= (uint32_t) r->numReplicas)
			break;
		// FindCandidateServers(&result, &excludes, rackId = -1);
		// 从指定机架上查找候选服务器
		FindCandidateServers(candidates, dummy, racks[idx]);
		if (candidates.size() == 0)
			continue;
		// take as many as we can from this rack
		uint32_t n = 0;

		// 如果localserver在这个机架上, 则需要少分配一个
		if (localserver && (racks[idx] == localserver->GetRack()))
			n = 1;
		for (uint32_t i = 0; i < candidates.size() && n < numServersPerRack; i++)
		{
			// 如果已经分配够了, 则退出即可
			if (r->servers.size() >= (uint32_t) r->numReplicas)
				break;

			// 如果这个server和localserver相同, 则跳过, 否则, 就可以加入候选服务器列表了
			if (candidates[i] != localserver)
			{
				r->servers.push_back(candidates[i]);
				n++;
			}
		}
	}

	if (r->servers.size() == 0)
		return -ENOSPC;

	// 分配一个租约
	LeaseInfo l(WRITE_LEASE, mLeaseId, r->servers[0]);
	mLeaseId++;

	// 选择第一个服务器作为master, 向它发送分配块命令
	r->master = r->servers[0];
	r->servers[0]->AllocateChunk(r, l.leaseId);

	// 向其他所有的候选服务器发送块分配命令, 此时不需要分配lease了
	for (i = 1; i < r->servers.size(); i++)
	{
		r->servers[i]->AllocateChunk(r, -1);
	}

	ChunkPlacementInfo v;

	// 将该chunk的分配信息存储在v中,
	v.fid = r->fid;
	v.chunkServers = r->servers;
	v.chunkLeases.push_back(l);

	mChunkToServerMap[r->chunkId] = v;

	if (r->servers.size() < (uint32_t) r->numReplicas)
		ChangeChunkReplication(r->chunkId);

	return 0;
}

/// 从chunk到chunkLocation的map中查找r中指定的chunk所对应的写Lease(WRITE_LEASE),
/// 如果该lease不存在, 则创建新lease, 并且置isNewLease为真; 否则, isNewLease为假.
/// @note chunkserver中chunk空间的分配是在创建lease时发送命令的.
int LayoutManager::GetChunkWriteLease(MetaAllocate *r, bool &isNewLease)
{
	ChunkPlacementInfo v;
	vector<ChunkServerPtr>::size_type i;
	vector<LeaseInfo>::iterator l;

	// XXX: This is a little too conservative.  We should
	// check if any server has told us about a lease for this
	// file; if no one we know about has a lease, then deny
	// issuing the lease during recovery---because there could
	// be some server who has a lease and hasn't told us yet.
	// 如果系统正在恢复, 则不为之进行处理, 返回系统忙错误.
	if (InRecovery())
	{
		KFS_LOG_INFO("GetChunkWriteLease: InRecovery() => EBUSY");
		return -EBUSY;
	}

	// if no allocation has been done, can't grab any lease
	// 如果mChunkToServerMap中没有该chunk, 则必然不会存在Lease.
	CSMapIter iter = mChunkToServerMap.find(r->chunkId);
	if (iter == mChunkToServerMap.end())
		return -EINVAL;

	v = iter->second;
	// 如果该chunk所有的服务器都已经当机, 则错误退出!
	if (v.chunkServers.size() == 0)
		// all the associated servers are dead...so, fail
		// the allocation request.
		return -KFS::EDATAUNAVAIL;

	// 从该chunk的lease中查找写Lease(WRITE_LEASE)
	l = find_if(v.chunkLeases.begin(), v.chunkLeases.end(), ptr_fun(
			LeaseInfo::IsValidWriteLease));
	if (l != v.chunkLeases.end())
	{
		// 如果写lease存在
		LeaseInfo lease = *l;
#ifdef DEBUG
		time_t now = time(0);
		assert(now <= lease.expires);
		KFS_LOG_DEBUG("write lease exists...no version bump");
#endif
		// valid write lease; so, tell the client where to go
		isNewLease = false;
		r->servers = v.chunkServers;
		r->master = lease.chunkServer;
		return 0;
	}

	// there is no valid write lease; to issue a new write lease, we
	// need to do a version # bump.  do that only if we haven't yet
	// handed out valid read leases
	// 查找该chunk中是否有可用的lease, 如果有的话, 说明该chunk正在读, 返回系统忙
	l = find_if(v.chunkLeases.begin(), v.chunkLeases.end(), ptr_fun(
			LeaseInfo::IsValidLease));
	if (l != v.chunkLeases.end())
	{
		KFS_LOG_DEBUG("GetChunkWriteLease: read lease => EBUSY");
		return -EBUSY;
	}

	// no one has a valid lease
	// 清除该chunk的所有lease, 因为其中没有有效的lease了.
	LeaseCleanup(r->chunkId, v);

	// Need space on the servers..otherwise, fail it
	// 需要在每一个chunkserver上开辟空间, 如果不能开辟, 则分配失败.
	r->servers = v.chunkServers;
	for (i = 0; i < r->servers.size(); i++)
	{
		if (r->servers[i]->GetAvailSpace() < CHUNKSIZE)
			return -ENOSPC;
	}

	isNewLease = true;

	// 创建关联于master(r->servers[0])的lease
	LeaseInfo lease(WRITE_LEASE, mLeaseId, r->servers[0]);
	mLeaseId++;

	v.chunkLeases.push_back(lease);
	mChunkToServerMap[r->chunkId] = v;

	// when issuing a new lease, bump the version # by the increment
	// 更新chunk的版本号, 向master发送命令分配chunk空间.
	r->chunkVersion += chunkVersionInc;
	r->master = r->servers[0];
	r->master->AllocateChunk(r, lease.leaseId);

	// 向该chunk所对应的所有chunkserver发送chunk分配命令
	for (i = 1; i < r->servers.size(); i++)
	{
		r->servers[i]->AllocateChunk(r, -1);
	}
	return 0;
}

/*
 * \brief Process a reqeuest for a READ lease.
 */
/// 为指定的lease请求一个读操作
int LayoutManager::GetChunkReadLease(MetaLeaseAcquire *req)
{
	ChunkPlacementInfo v;

	// 如果正在执行恢复操作
	if (InRecovery())
	{
		KFS_LOG_INFO("GetChunkReadLease: inRecovery() => EBUSY");
		return -EBUSY;
	}

	// 该chunk在map中应该有一个MetaChunkInfo
	CSMapIter iter = mChunkToServerMap.find(req->chunkId);
	if (iter == mChunkToServerMap.end())
		return -EINVAL;

	// issue a read lease
	// 创建一个读操作
	LeaseInfo lease(READ_LEASE, mLeaseId);
	mLeaseId++;

	v = iter->second;
	v.chunkLeases.push_back(lease);
	mChunkToServerMap[req->chunkId] = v;
	req->leaseId = lease.leaseId;

	return 0;
}

/// 判断c指定的Chunk是否有有效的lease存在.
class ValidLeaseIssued
{
	CSMap &chunkToServerMap;
public:
	ValidLeaseIssued(CSMap &m) :
		chunkToServerMap(m)
	{
	}
	bool operator()(MetaChunkInfo *c)
	{
		ChunkPlacementInfo v;
		vector<LeaseInfo>::iterator l;

		CSMapIter iter = chunkToServerMap.find(c->chunkId);
		if (iter == chunkToServerMap.end())
			return false;
		v = iter->second;
		l = find_if(v.chunkLeases.begin(), v.chunkLeases.end(), ptr_fun(
				LeaseInfo::IsValidLease));
		return (l != v.chunkLeases.end());
	}
};

/// 判断一个MetaChunkInfo中是否有的chunk有可用的lease, 如果都没有, 返回false, 否则true.
bool LayoutManager::IsValidLeaseIssued(const vector<MetaChunkInfo *> &c)
{
	vector<MetaChunkInfo *>::const_iterator i;

	i = find_if(c.begin(), c.end(), ValidLeaseIssued(mChunkToServerMap));
	if (i == c.end())
		return false;
	KFS_LOG_VA_DEBUG("Valid lease issued on chunk: %lld",
			(*i)->chunkId);
	return true;
}

/// 判断两个Lease的ID是否相同
class LeaseIdMatcher
{
	int64_t myid;
public:
	LeaseIdMatcher(int64_t id) :
		myid(id)
	{
	}
	bool operator()(const LeaseInfo &l)
	{
		return l.leaseId == myid;
	}
};

/*
 * 更新req中指定的lease:
 * 1. 如果该chunk目录不存在, 且系统正在处于恢复过程中, 则创建该lease;
 * 2. 如果该lease已经过期, 则清除该lease;
 * 3. 如果不符合以上两种情况, 则更新lease.
 */
int LayoutManager::LeaseRenew(MetaLeaseRenew *req)
{
	ChunkPlacementInfo v;
	vector<LeaseInfo>::iterator l;

	// 查找指定chunk的lease
	CSMapIter iter = mChunkToServerMap.find(req->chunkId);
	if (iter == mChunkToServerMap.end())
	{
		// 如果该chunk没有在chunk列表中.
		// 在chunk列表中没有找到对应项: 如果系统正在进行恢复, 则为之创建lease(相当于更新)
		if (InRecovery())
		{
			// Allow lease renewals during recovery
			LeaseInfo lease(req->leaseType, req->leaseId);
			if (req->leaseId > mLeaseId)
				mLeaseId = req->leaseId + 1;
			v.chunkLeases.push_back(lease);
			mChunkToServerMap[req->chunkId] = v;
			return 0;
		}
		return -EINVAL;
	}

	// chunk中存在对应的项
	v = iter->second;
	l = find_if(v.chunkLeases.begin(), v.chunkLeases.end(), LeaseIdMatcher(
			req->leaseId));
	if (l == v.chunkLeases.end())
		return -EINVAL;
	time_t now = time(0);

	// 如果要更新的lease已经过期, 则清除该lease
	if (now > l->expires)
	{
		// can't renew dead leases; get a new one
		v.chunkLeases.erase(l);
		return -ELEASEEXPIRED;
	}

	// 否则, 更新这个lease, 把更新后的内容写回mChunkToServerMap
	l->expires = now + LEASE_INTERVAL_SECS;
	mChunkToServerMap[req->chunkId] = v;
	return 0;
}

///
/// Handling a corrupted chunk involves removing the mapping
/// from chunk id->chunkserver that we know has it.
///
void LayoutManager::ChunkCorrupt(MetaChunkCorrupt *r)
{
	ChunkPlacementInfo v;

	// 更新统计: 已经挂了的chunk数目+1
	r->server->IncCorruptChunks();

	CSMapIter iter = mChunkToServerMap.find(r->chunkId);
	if (iter == mChunkToServerMap.end())
		return;

	v = iter->second;
	if (v.fid != r->fid)
	{
		KFS_LOG_VA_WARN("Server %s claims invalid chunk: <%lld, %lld> to be corrupt",
				r->server->ServerID().c_str(), r->fid, r->chunkId);
		return;
	}

	KFS_LOG_VA_INFO("Server %s claims file/chunk: <%lld, %lld> to be corrupt",
			r->server->ServerID().c_str(), r->fid, r->chunkId);

	// 删除v中对应的chunkserver服务器
	v.chunkServers.erase(remove_if(v.chunkServers.begin(),
			v.chunkServers.end(), ChunkServerMatcher(r->server.get())),
			v.chunkServers.end());
	mChunkToServerMap[r->chunkId] = v;
	// check the replication state when the replicaiton checker gets to it
	// 更新该chunk的Replicator数目, 确保其有足够的冗余拷贝
	ChangeChunkReplication(r->chunkId);

	// this chunk has to be replicated from elsewhere; since this is no
	// longer hosted on this server, take it out of its list of blocks
	// 因为这个chunk在该服务器上已经没有了该拷贝, 所以需要从它的chunk列表中删除该chunk
	r->server->MovingChunkDone(r->chunkId);
	if (r->server->IsRetiring())
	{
		// 如果该chunkserver刚刚好正要休眠, 则直接从mEvacuatingChunks删除该chunk
		r->server->EvacuateChunkDone(r->chunkId);
	}
}

// 删除指定的chunkId的chunk: 通过构造函数指定chunk, 通过ChunkServerPtr执行删除
class ChunkDeletor
{
	chunkId_t chunkId;
public:
	ChunkDeletor(chunkId_t c) :
		chunkId(c)
	{
	}
	void operator ()(ChunkServerPtr &c)
	{
		c->DeleteChunk(chunkId);
	}
};

///
/// Deleting a chunk involves two things: (1) removing the
/// mapping from chunk id->chunk server that has it; (2) sending
/// an RPC to the associated chunk server to nuke out the chunk.
///
/// 删除一个chunk: 1. 删除map中的chunk->server映射; 2. 发送命令删除在chunkserver的拷贝
void LayoutManager::DeleteChunk(chunkId_t chunkId)
{
	vector<ChunkServerPtr> c;

	// if we know anything about this chunk at all, then we
	// process the delete request.
	if (GetChunkToServerMapping(chunkId, c) != 0)
		return;

	// remove the mapping
	mChunkToServerMap.erase(chunkId);

	// submit an RPC request
	for_each(c.begin(), c.end(), ChunkDeletor(chunkId));
}

/// 发送命令改变指定服务器(通过ChunkServerPtr指定)上指定chunk(通过chunkId指定)的大小(sz).
class Truncator
{
	chunkId_t chunkId;
	off_t sz;
public:
	Truncator(chunkId_t c, off_t s) :
		chunkId(c), sz(s)
	{
	}
	void operator ()(ChunkServerPtr &c)
	{
		c->TruncateChunk(chunkId, sz);
	}
};

///
/// To truncate a chunk, find the server that holds the chunk and
/// submit an RPC request to it.
///
/// 将指定的chunk大小改变为sz大小, 其所有拷贝都需要改变
void LayoutManager::TruncateChunk(chunkId_t chunkId, off_t sz)
{
	vector<ChunkServerPtr> c;

	// if we know anything about this chunk at all, then we
	// process the truncate request.
	if (GetChunkToServerMapping(chunkId, c) != 0)
		return;

	// submit an RPC request
	// 创建一个Truncator, 进行改变chunk大小的操作.
	Truncator doTruncate(chunkId, sz);
	for_each(c.begin(), c.end(), doTruncate);
}

/// 添加指定的chunkserver到chunk中:
/// 1. 如果c是NULL, 则新建(或清空)同chunkserver的映射;
/// 2. 如果更新Map不成功, 则指定chunk的chunkserver为c.
void LayoutManager::AddChunkToServerMapping(chunkId_t chunkId, fid_t fid,
		ChunkServer *c)
{
	ChunkPlacementInfo v;

	// 如果c为空, 则没有指定chunkserver
	if (c == NULL)
	{
		// Store an empty mapping to signify the presence of this
		// particular chunkId.
		v.fid = fid;
		mChunkToServerMap[chunkId] = v;
		return;
	}

	assert(ValidServer(c));

	KFS_LOG_VA_DEBUG("Laying out chunk=%lld on server %s",
			chunkId, c->GetServerName());

	// 将c加入到该chunk已有的chunkserver列表中
	if (UpdateChunkToServerMapping(chunkId, c) == 0)
		return;

	// 如果UpdateChunkToServerMapping()失败, 则指定该chunk的chunkserver为c
	v.fid = fid;
	v.chunkServers.push_back(c->shared_from_this());
	mChunkToServerMap[chunkId] = v;
}

/// 删除chunk在mChunkToServerMap中的记录
void LayoutManager::RemoveChunkToServerMapping(chunkId_t chunkId)
{
	CSMapIter iter = mChunkToServerMap.find(chunkId);
	if (iter == mChunkToServerMap.end())
		return;

	mChunkToServerMap.erase(iter);
}

/// 更新chunk同chunkserver的对应关系: 将c加入到chunkserver列表中
int LayoutManager::UpdateChunkToServerMapping(chunkId_t chunkId, ChunkServer *c)
{
	// If the chunkid isn't present in the mapping table, it could be a
	// stale chunk
	CSMapIter iter = mChunkToServerMap.find(chunkId);
	if (iter == mChunkToServerMap.end())
		return -1;

	/*
	 KFS_LOG_VA_DEBUG("chunk=%lld was laid out on server %s",
	 chunkId, c->GetServerName());
	 */
	// 将chunkserver压入指定的chunk's server队列
	iter->second.chunkServers.push_back(c->shared_from_this());

	return 0;
}

// 从mChunkToServerMap中查找指定chunk所在的chunkserver列表
int LayoutManager::GetChunkToServerMapping(chunkId_t chunkId, vector<
		ChunkServerPtr> &c)
{
	CSMapConstIter iter = mChunkToServerMap.find(chunkId);
	if ((iter == mChunkToServerMap.end()) || (iter->second.chunkServers.size()
			== 0))
		return -1;

	c = iter->second.chunkServers;
	return 0;
}

/// Wrapper class due to silly template/smart-ptr madness
/// 发送mPendingOps中的RPC命令到指定的chunkserver
class Dispatcher
{
public:
	Dispatcher()
	{
	}
	void operator()(ChunkServerPtr &c)
	{
		c->Dispatch();
	}
};

/// 发送所有chunkserver的RPC命令到对应的chunkserver
void LayoutManager::Dispatch()
{
	// this method is called in the context of the network thread.
	// lock out the request processor to prevent changes to the list.

	pthread_mutex_lock(&mChunkServersMutex);

	for_each(mChunkServers.begin(), mChunkServers.end(), Dispatcher());

	pthread_mutex_unlock(&mChunkServersMutex);
}

/// 判断一个chunkserver是否可用: 在mChunkServers(已经连接的chunkserver目录)中有记录的
/// chunkserver是可用的.
bool LayoutManager::ValidServer(ChunkServer *c)
{
	vector<ChunkServerPtr>::const_iterator i;

	i = find_if(mChunkServers.begin(), mChunkServers.end(), ChunkServerMatcher(
			c));
	return (i != mChunkServers.end());
}

/// 用于进行统计: 输出通过该类的所有chunkserver的信息, 统计他们总共可用和已经使用的空间的大小
class Pinger
{
	string &result;
	// return the total/used for all the nodes in the cluster
	uint64_t &totalSpace;
	uint64_t &usedSpace;
public:
	Pinger(string &r, uint64_t &t, uint64_t &u) :
		result(r), totalSpace(t), usedSpace(u)
	{
	}
	void operator ()(ChunkServerPtr &c)
	{
		c->Ping(result);
		totalSpace += c->GetTotalSpace();
		usedSpace += c->GetUsedSpace();
	}
};

/// 获取所有经过该类处理的chunkserver的retire信息, 存放在result中
class RetiringStatus
{
	string &result;
public:
	RetiringStatus(string &r) :
		result(r)
	{
	}
	void operator ()(ChunkServerPtr &c)
	{
		c->GetRetiringStatus(result);
	}
};

/*
 * 获取整个metaserver的所有chunk信息:
 * 1. 系统信息(systemInfo): 恢复开始时间(即启动时间), 总空间, 已用空间;
 * 2. 正常状态的chunkserver信息(upServers);
 * 3. 已经当机的服务器信息(downServers);
 * 4. 正在休眠的服务器信息(retiringServers).
 */
void LayoutManager::Ping(string &systemInfo, string &upServers,
		string &downServers, string &retiringServers)
{
	uint64_t totalSpace = 0, usedSpace = 0;
	Pinger doPing(upServers, totalSpace, usedSpace);
	for_each(mChunkServers.begin(), mChunkServers.end(), doPing);
	downServers = mDownServers.str();
	for_each(mChunkServers.begin(), mChunkServers.end(), RetiringStatus(
			retiringServers));

	ostringstream os;

	os << "Up since= " << timeToStr(mRecoveryStartTime) << '\t';
	os << "Total space= " << totalSpace << '\t';
	os << "Used space= " << usedSpace;
	systemInfo = os.str();
}

/// functor to tell if a lease has expired
/// 判断一个lease是否过期
class LeaseExpired
{
	time_t now;
public:
	LeaseExpired(time_t n) :
		now(n)
	{
	}
	bool operator ()(const LeaseInfo &l)
	{
		return now >= l.expires;
	}

};

/// 用于统计: 减少指定chunkserver正在写chunk的统计数量
class ChunkWriteDecrementor
{
public:
	void operator()(ChunkServerPtr &c)
	{
		c->UpdateNumChunkWrites(-1);
	}
};

/// If the write lease on a chunk is expired, then decrement the # of writes
/// on the servers that are involved in the write.
/// 处理指定chunk的WRITE_LEASE, 如果该lease过期, 则减少对应chunk服务器的写数目
class DecChunkWriteCount
{
	fid_t f;
	chunkId_t c;
public:
	DecChunkWriteCount(fid_t fid, chunkId_t id) :
		f(fid), c(id)
	{
	}
	void operator()(const LeaseInfo &l)
	{
		if (l.leaseType != WRITE_LEASE)
			return;
		vector<ChunkServerPtr> servers;
		// 获取chunk所在的服务器
		gLayoutManager.GetChunkToServerMapping(c, servers);
		// 将该服务器列表中所有的写数目分别-1
		for_each(servers.begin(), servers.end(), ChunkWriteDecrementor());
		// get the chunk's size from one of the servers
		if (servers.size() > 0)
			servers[0]->GetChunkSize(f, c);
	}

};

/// functor to that expires out leases
/// 删除指定的chunk的所有失效的lease.
class LeaseExpirer
{
	// map<chunkId_t, ChunkPlacementInfo>
	CSMap &cmap;
	time_t now;
public:
	LeaseExpirer(CSMap &m, time_t n) :
		cmap(m), now(n)
	{
	}
	void operator ()(const map<chunkId_t, ChunkPlacementInfo>::value_type p)
	{
		ChunkPlacementInfo c = p.second;
		chunkId_t chunkId = p.first;
		vector<LeaseInfo>::iterator i;

		// 查找该chunk中已经失效的lease, 并且删除.
		i = remove_if(c.chunkLeases.begin(), c.chunkLeases.end(), LeaseExpired(
				now));

		// 把每一个失效的lease, 调用DecChunkWriteCount()更新chunk服务器
		for_each(i, c.chunkLeases.end(), DecChunkWriteCount(c.fid, chunkId));
		// trim the list
		c.chunkLeases.erase(i, c.chunkLeases.end());
		cmap[p.first] = c;
	}
};

/// 清除整个系统中所有失效的lease.
void LayoutManager::LeaseCleanup()
{
	time_t now = time(0);

	for_each(mChunkToServerMap.begin(), mChunkToServerMap.end(), LeaseExpirer(
			mChunkToServerMap, now));

}

// Cleanup the leases for a particular chunk
// 删除指定chunkID和其对应的ChunkPlacementInfo的所有lease
void LayoutManager::LeaseCleanup(chunkId_t chunkId, ChunkPlacementInfo &v)
{
	for_each(v.chunkLeases.begin(), v.chunkLeases.end(), DecChunkWriteCount(
			v.fid, chunkId));
	v.chunkLeases.clear();
}

/// 查看指定的server(指定ChunkServerPtr)是否正在休眠中.
class RetiringServerPred
{
public:
	RetiringServerPred()
	{
	}
	bool operator()(const ChunkServerPtr &c)
	{
		return c->IsRetiring();
	}
};

// mEvacuatingChunks.erase(chunkId);
// 清空指定server中的evacuate队列中指定的chunk
// 难道在复制的过程中, 该chunk不可使用?
class ReplicationDoneNotifier
{
	chunkId_t cid;
public:
	ReplicationDoneNotifier(chunkId_t c) :
		cid(c)
	{
	}
	void operator()(ChunkServerPtr &s)
	{
		s->EvacuateChunkDone(cid);
	}
};

/// 将指定chunk服务器的机架号加入到队列rack中
class RackSetter
{
	set<int> &racks;
	bool excludeRetiringServers;
public:
	RackSetter(set<int> &r, bool excludeRetiring = false) :
		racks(r), excludeRetiringServers(excludeRetiring)
	{
	}
	void operator()(const ChunkServerPtr &s)
	{
		if (excludeRetiringServers && s->IsRetiring())
			return;
		racks.insert(s->GetRack());
	}
};

/// 在已有chunk分配的基础上, 为chunkId指定的chunk在多分配几个replicas
int LayoutManager::ReplicateChunk(chunkId_t chunkId,
		const ChunkPlacementInfo &clli, uint32_t extraReplicas)
{
	vector<int> racks;
	set<int> excludeRacks;
	// find a place
	vector<ChunkServerPtr> candidates;

	// two steps here: first, exclude the racks on which chunks are already
	// placed; if we can't find a unique rack, then put it wherever
	// for accounting purposes, ignore the rack(s) which contain a retiring
	// chunkserver; we'd like to move the block within that rack if
	// possible.

	// 将clli中的所有服务器的机架号码提取到excludeRacks中
	for_each(clli.chunkServers.begin(), clli.chunkServers.end(), RackSetter(
			excludeRacks, true));

	// 从剩余的机架中选择一组候选机架, 存放到racks中
	FindCandidateRacks(racks, excludeRacks);
	if (racks.size() == 0)
	{
		// no new rack is available to put the chunk
		// take what we got
		// 如果除了excludeRacks中的服务器外, 已经没有多余的服务器可以使用, 则使用
		// excludeRacks中的服务器.
		FindCandidateRacks(racks);
		if (racks.size() == 0)
			// no rack is available
			return 0;
	}

	// 确定每个机架上候选服务器的数目
	uint32_t numServersPerRack = extraReplicas / racks.size();
	if (extraReplicas % racks.size())
		numServersPerRack++;

	// 从候选的rack中选出指定数量的服务器
	for (uint32_t idx = 0; idx < racks.size(); idx++)
	{
		// 如果已经找到了足够多的候选服务器, 退出即可.
		if (candidates.size() >= extraReplicas)
			break;
		vector<ChunkServerPtr> servers;

		// find candidates other than those that are already hosting the
		// chunk
		// 从第idx个机架中选择出不包含该chunk的机架
		FindCandidateServers(servers, clli.chunkServers, racks[idx]);

		// take as many as we can from this rack
		// 选出候选服务器放在candidates列表中
		for (uint32_t i = 0; i < servers.size() && i < numServersPerRack; i++)
		{
			if (candidates.size() >= extraReplicas)
				break;
			candidates.push_back(servers[i]);
		}
	}

	if (candidates.size() == 0)
		return 0;

	// 分配额外的冗余拷贝
	return ReplicateChunk(chunkId, clli, extraReplicas, candidates);
}

/// 给定指定chunk在现有系统上的分配情况, 额外再分配extraReplicas个冗余拷贝(指定该
/// extraReplicas个chunk所在的候选服务器.
/// @return 成功发送拷贝命令的数目
int LayoutManager::ReplicateChunk(chunkId_t chunkId,
		const ChunkPlacementInfo &clli, uint32_t extraReplicas, const vector<
				ChunkServerPtr> &candidates)
{
	ChunkServerPtr c, dataServer;
	vector<MetaChunkInfo *> v;
	vector<MetaChunkInfo *>::iterator chunk;
	fid_t fid = clli.fid;
	int numDone = 0;

	/*
	 metatree.getalloc(fid, v);
	 chunk = find_if(v.begin(), v.end(), ChunkIdMatcher(chunkId));
	 if (chunk == v.end()) {
	 panic("missing chunk", true);
	 }

	 MetaChunkInfo *mci = *chunk;
	 */

	// 每次选出一个数据源, 对candidates中的候选服务器进行数据拷贝, 选数据源的策略是:
	// 1. 优先从clli中的服务器列表中选择retired服务器;
	// 2. 如果没有, 则从其中选择没有过载的服务器作为数据源.
	for (uint32_t i = 0; i < candidates.size() && i < extraReplicas; i++)
	{
		vector<ChunkServerPtr>::const_iterator iter;

		c = candidates[i];
		// Don't send too many replications to a server
		// 查看当前的候选服务器有多少个正在进行的拷贝, 如果有太多正在进行的拷贝, 则跳过
		if (c->GetNumChunkReplications()
				> MAX_CONCURRENT_WRITE_REPLICATIONS_PER_NODE)
			continue;
#ifdef DEBUG
		// verify that we got good candidates
		iter = find(clli.chunkServers.begin(), clli.chunkServers.end(), c);
		if (iter != clli.chunkServers.end())
		{
			assert(!"Not possible...");
		}
#endif
		// prefer a server that is being retired to the other nodes as
		// the source of the chunk replication
		// 从原拷贝中优先选择处于休眠状态的服务器作为拷贝的数据源???
		iter = find_if(clli.chunkServers.begin(), clli.chunkServers.end(),
				RetiringServerPred());

		const char *reason;

		// 如果在clli的server列表中有retired服务器, 则选择该服务器作为数据源
		if (iter != clli.chunkServers.end())
		{
			reason = " evacuating chunk ";
			if (((*iter)->GetReplicationReadLoad()
					< MAX_CONCURRENT_READ_REPLICATIONS_PER_NODE)
					&& (*iter)->IsResponsiveServer())
				dataServer = *iter;
		}
		else
		{
			reason = " re-replication ";
		}

		// if we can't find a retiring server, pick a server that has read b/w available
		// 如果没有找到retired服务器, 则从clli列表中查找没有读过载的服务器作为数据源
		for (uint32_t j = 0; (!dataServer) && (j < clli.chunkServers.size()); j++)
		{
			if (clli.chunkServers[j]->GetReplicationReadLoad()
					>= MAX_CONCURRENT_READ_REPLICATIONS_PER_NODE)
				continue;
			dataServer = clli.chunkServers[j];
		}
		if (dataServer)
		{
			// 这时, 找到了一个数据源, 则开始进行数据拷贝.
			ServerLocation srcLocation = dataServer->GetServerLocation();
			ServerLocation dstLocation = c->GetServerLocation();

			KFS_LOG_VA_INFO("Starting re-replication for chunk %lld (from: %s to %s) reason = %s",
					chunkId,
					srcLocation.ToString().c_str(),
					dstLocation.ToString().c_str(), reason);
			dataServer->UpdateReplicationReadLoad(1);
			/*
			 c->ReplicateChunk(fid, chunkId, mci->chunkVersion,
			 dataServer->GetServerLocation());
			 */
			// have the chunkserver get the version
			// 发送命令, 要求c向dataServer请求数据
			c->ReplicateChunk(fid, chunkId, -1, dataServer->GetServerLocation());
			numDone++;
		}
		dataServer.reset();
	}

	if (numDone > 0)
	{
		// 更新正在进行的chunk拷贝的chunk数目
		mTotalReplicationStats->Update(1);
		// 更新正在执行chunk拷贝的服务器数目
		mOngoingReplicationStats->Update(numDone);
	}
	return numDone;
}

/*
 * 检测指定的chunk是否可以进行拷贝:
 * 1. 如果有活动的WRITE_LEASE, 则不能进行拷贝, 返回false;
 * 2. 如果c中没有chunk的服务器列表, 也不能进行拷贝, extreReplicas为0;
 * 3. 如果chunk不存在于chunk所属的文件, 也不能进行拷贝, extraReplicas为0;
 * 4. 否则, 即可指定要拷贝的数目了.
 *
 * 要拷贝的数目:
 * 1. extraReplicas > 0: 有额外的chunk需要进行拷贝, 以符合要求;
 * 2. extraReplicas = 0: 不需要进行任何操作;
 * 3. extreReplicas < 0: 拷贝数目太多, 需要删除一些.
 */
bool LayoutManager::CanReplicateChunkNow(chunkId_t chunkId,
		ChunkPlacementInfo &c, int &extraReplicas)
{
	vector<LeaseInfo>::iterator l;

	// Don't replicate chunks for which a write lease
	// has been issued.
	// 如果该chunk有一个WRITE_LEASE, 则说明该lease正在修改, 不可进行拷贝
	l = find_if(c.chunkLeases.begin(), c.chunkLeases.end(), ptr_fun(
			LeaseInfo::IsValidWriteLease));

	if (l != c.chunkLeases.end())
		return false;

	extraReplicas = 0;
	// Can't re-replicate a chunk if we don't have a copy! so,
	// take out this chunk from the candidate set.
	// 如果没有数据源可以使用, 则置可以拷贝的数目为0, 说明不能进行拷贝
	if (c.chunkServers.size() == 0)
		return true;

	// 获取该chunk所属文件的属性, 如果没有该记录, 则同样返回拷贝数为0
	MetaFattr *fa = metatree.getFattr(c.fid);
	if (fa == NULL)
		// No file attr.  So, take out this chunk
		// from the candidate set.
		return true;

	// check if the chunk still exists
	vector<MetaChunkInfo *> v;
	vector<MetaChunkInfo *>::iterator chunk;

	// 获取该文件的所有chunk, 放在v中
	metatree.getalloc(c.fid, v);
	chunk = find_if(v.begin(), v.end(), ChunkIdMatcher(chunkId));
	// 说明该chunk不存在于该文件中, 也不能进行拷贝
	if (chunk == v.end())
	{
		// This chunk doesn't exist in this file anymore.
		// So, take out this chunk from the candidate set.
		return true;
	}

	// if any of the chunkservers are retiring, we need to make copies
	// so, first determine how many copies we need because one of the
	// servers hosting the chunk is going down
	// 如果c有一些服务器正在retiring, 则需要进行拷贝, 使拷贝数达到numReplicas.
	int numRetiringServers = count_if(c.chunkServers.begin(),
			c.chunkServers.end(), RetiringServerPred());
	// now, determine if we have sufficient copies
	if (numRetiringServers > 0)
	{
		if (c.chunkServers.size() - numRetiringServers
				< (uint32_t) fa->numReplicas)
		{
			// we need to make this many copies: # of servers that are
			// retiring plus the # this chunk is under-replicated
			extraReplicas = numRetiringServers + (fa->numReplicas
					- c.chunkServers.size());
		}
		else
		{
			// we got sufficient copies even after accounting for
			// the retiring server.  so, take out this chunk from
			// replication candidates set.
			extraReplicas = 0;
		}
		return true;
	}

	// May need to re-replicate this chunk:
	//    - extraReplicas > 0 means make extra copies;
	//    - extraReplicas == 0, take out this chunkid from the candidate set
	//    - extraReplicas < 0, means we got too many copies; delete some
	// 如果没有retiring的服务器, 则需要拷贝的数目就是:
	// fa->numReplicas - c.chunkServers.size()
	extraReplicas = fa->numReplicas - c.chunkServers.size();

	if (extraReplicas < 0)
	{
		//
		// We need to delete additional copies; however, if
		// there is a valid (read) lease issued on the chunk,
		// then leave the chunk alone for now; we'll look at
		// deleting it when the lease has expired.  This is for
		// safety: if a client was reading from the copy of the
		// chunk that we are trying to delete, the client will
		// see the deletion and will have to failover; avoid
		// unnecessary failovers
		//
		// 需要删除lease, 如果有一个读lease, 则不能进行删除操作.
		l = find_if(c.chunkLeases.begin(), c.chunkLeases.end(), ptr_fun(
				LeaseInfo::IsValidLease));

		if (l != c.chunkLeases.end())
			return false;
	}

	return true;
}

/// 把指定retring服务器中的Evacuating chunks进行检验:
/// 1. 把在chunkToServerMap中不存在的chunk删除;
/// 2. 把其他的加入到candidates中.
class EvacuateChunkChecker
{
	// set<chunkId_t>
	CRCandidateSet &candidates;
	// map<chunkId_t, ChunkPlacementInfo>
	CSMap &chunkToServerMap;
public:
	EvacuateChunkChecker(CRCandidateSet &c, CSMap &m) :
		candidates(c), chunkToServerMap(m)
	{
	}
	void operator()(ChunkServerPtr c)
	{
		// 如果该服务器不是正在retiring, 则不做任何操作.
		if (!c->IsRetiring())
			return;

		// 获取要进行冻结的chunk列表
		CRCandidateSet leftover = c->GetEvacuatingChunks();

		// 逐个该chunkserver中的EvacuatingChunks列表, 并进行处理:
		// 1. 把在chunkToServerMap中不存在的chunk删除;
		// 2. 把其他的加入到candidates中.
		for (CRCandidateSetIter citer = leftover.begin(); citer
				!= leftover.end(); ++citer)
		{
			chunkId_t chunkId = *citer;
			CSMapIter iter = chunkToServerMap.find(chunkId);

			if (iter == chunkToServerMap.end())
			{
				// 清空指定的chunk, 因为这个chunk在chunkToServerMap中已经没有了记录
				c->EvacuateChunkDone(chunkId);
				KFS_LOG_VA_INFO("%s has bogus block %ld",
						c->GetServerLocation().ToString().c_str(), chunkId);
			}
			else
			{
				// XXX
				// if we don't think this chunk is on this
				// server, then we should update the view...

				candidates.insert(chunkId);
				KFS_LOG_VA_INFO("%s has block %ld that wasn't in replication candidates",
						c->GetServerLocation().ToString().c_str(), chunkId);
			}
		}
	}
};

/// 检测正在休眠的服务器的状态:
/// 1. 没有复活, 但是也没有到休眠结束时间;
/// 2. 到规定休眠结束时间了, 但是已经复活;
/// 3. 死去了, 准备对该chunkserver上的数据进行拷贝复制, 以保证足够其中的chunk拥有
///  足够的replicas.
void LayoutManager::CheckHibernatingServersStatus()
{
	time_t now = time(0);

	vector<HibernatingServerInfo_t>::iterator iter =
			mHibernatingServers.begin();
	vector<ChunkServerPtr>::iterator i;

	// 逐个对mHibernatingServers列表中的服务器进行检测.
	while (iter != mHibernatingServers.end())
	{
		i = find_if(mChunkServers.begin(), mChunkServers.end(), MatchingServer(
				iter->location));

		// 1. 如果该服务器不存在于活动的服务器列表中, 并且休眠结束时间还没有到, 则跳过
		if ((i == mChunkServers.end()) && (now < iter->sleepEndTime))
		{
			// within the time window where the server is sleeping
			// so, move on
			iter++;
			continue;
		}
		// 2. 如果存在于活动的服务器列表中, 说明该休眠的服务器已经苏醒
		if (i != mChunkServers.end())
		{
			KFS_LOG_VA_INFO("Hibernated server (%s) is back as promised...",
					iter->location.ToString().c_str());
		}
		// 3. 该服务器既没有出现在已经活动的列表当中, 又已经超时, 则准备对这个服务器上的数据
		// 进行拷贝, 以保证有足够的replicas.
		else
		{
			// server hasn't come back as promised...so, check
			// re-replication for the blocks that were on that node
			KFS_LOG_VA_INFO("Hibernated server (%s) is not back as promised...",
					iter->location.ToString().c_str());
			mChunkReplicationCandidates.insert(iter->blocks.begin(),
					iter->blocks.end());
		}
		mHibernatingServers.erase(iter);
		// 重新开始进行判断.
		iter = mHibernatingServers.begin();
	}
}

/*
 * 检测mChunkReplicationCandidates列表中的chunk复制情况:
 * 1. 检测正在休眠当中的服务器, 判断其中有没有死了的, 把其中的chunk加入到以上列表中;
 * 2. 对列表中的所有chunk发送复制命令(可能不是全部, 见代码中注释);
 * 3. 删除已经完成或非法的chunk;
 * 4. 重新检测所有服务器中的EnvacuatedChunk, 加入到列表中;
 * 5. 执行负载均衡;
 * 6. 更新mReplicationTodoStats.
 */
void LayoutManager::ChunkReplicationChecker()
{
	// 如果正在进行恢复, 则不进行检测.
	if (InRecovery())
	{
		return;
	}

	// 1. 检测正在休眠的服务器的状态. 在这个操作过程中, 由于retire服务器没有启动需要进行复制的
	//  chunk列表都已经存放在mChunkReplicationCandidates中了.
	CheckHibernatingServersStatus();

	// There is a set of chunks that are affected: their server went down
	// or there is a change in their degree of replication.  in either
	// case, walk this set of chunkid's and work on their replication amount.

	chunkId_t chunkId;
	CRCandidateSet delset;	// 要从mChunReplicatingCandidates中删除的chunk列表
	int extraReplicas, numOngoing;	// 循环内使用
	uint32_t numIterations = 0;		// 正在进行的拷贝操作数量
	struct timeval start;

	gettimeofday(&start, NULL);

	// 2. 分别对mChunkReplicationCandidates中的每一个chunk发送复制命令, 注意事项见循环体内
	// 注释.
	for (CRCandidateSetIter citer = mChunkReplicationCandidates.begin(); citer
			!= mChunkReplicationCandidates.end(); ++citer)
	{
		chunkId = *citer;

		struct timeval now;
		gettimeofday(&now, NULL);

		// 1. 如果超过了5秒钟都在执行这个操作, 则停止, 防止系统占用过多.
		if (ComputeTimeDiff(start, now) > 5.0)
			// if we have spent more than 5 seconds here, stop
			// serve other requests
			break;

		// 2. 如果该chunk不在mChunkToServerMap列表中, 则删除并跳过
		CSMapIter iter = mChunkToServerMap.find(chunkId);
		if (iter == mChunkToServerMap.end())
		{
			// 如果mChunkToServerMap中没有该chunk, 则将其加入到要删除的列表中
			delset.insert(chunkId);
			continue;
		}

		// 3. 如果该chunk正在进行复制, 则跳过
		if (iter->second.ongoingReplications > 0)
			// this chunk is being re-replicated; we'll check later
			continue;

		// 4. 如果当前不能进行复制, 则跳过.
		if (!CanReplicateChunkNow(iter->first, iter->second, extraReplicas))
			continue;

		// 5. 如果需要复制的拷贝数目>0, 则进行复制.
		if (extraReplicas > 0)
		{
			// 为该chunk执行复制操作
			numOngoing = ReplicateChunk(iter->first, iter->second,
					extraReplicas);
			// 记录正在进行的chunk复制操作数目.
			iter->second.ongoingReplications += numOngoing;
			if (numOngoing > 0)
			{
				mNumOngoingReplications++;
				mLastChunkReplicated = chunkId;
			}
			// 正在进行的拷贝操作数量
			numIterations++;
		}
		else if (extraReplicas == 0)
		{
			// 6. 如果不需要进行拷贝, 则删除该chunk在候选复制chunk列表中的目录
			delset.insert(chunkId);
		}
		else
		{
			// 7. 如果拷贝数目过多, 则删除一些拷贝.
			DeleteAddlChunkReplicas(iter->first, iter->second, -extraReplicas);
			delset.insert(chunkId);
		}

		// 8. 如果同时进行的拷贝数目过多, 则停止继续发送拷贝命令.
		if (numIterations > mChunkServers.size()
				* MAX_CONCURRENT_WRITE_REPLICATIONS_PER_NODE)
			// throttle...we are handing out
			break;
	}

	// 删除delset列表中的chunk
	if (delset.size() > 0)
	{
		for (CRCandidateSetIter citer = delset.begin(); citer != delset.end(); ++citer)
		{
			// Notify the retiring servers of any of their chunks have
			// been evacuated---such as, if there were too many copies of those
			// chunks, we are done evacuating them
			chunkId = *citer;
			CSMapIter iter = mChunkToServerMap.find(chunkId);

			// 如果存在于mChunkToServerMap中, 则说明该Replication已经完成,
			// 通知该chunk的所有server, 完成Replication.
			// @note 只能来自上边的6和7两种情况.
			if (iter != mChunkToServerMap.end())
			{
				for_each(iter->second.chunkServers.begin(),
						iter->second.chunkServers.end(),
						ReplicationDoneNotifier(chunkId));
			}

			// 删除其在mChunkReplicationCandidates中的目录.
			mChunkReplicationCandidates.erase(*citer);
		}
	}

	// 如果mChunkReplicationCandidates已经空了, 则检测所有的chunkserver, 有没有正在
	// retiring的服务器, 如果有, 将其中EvacuatingChunks列表中的chunk拷贝到
	// mChunkReplicationCandidates中.
	if (mChunkReplicationCandidates.size() == 0)
	{
		// if there are any retiring servers, we need to make sure that
		// the servers don't think there is a block to be replicated
		// if there is any such, let us get them into the set of
		// candidates...need to know why this happens
		for_each(mChunkServers.begin(), mChunkServers.end(),
				EvacuateChunkChecker(mChunkReplicationCandidates,
						mChunkToServerMap));
	}

	// 重新进行负载均衡控制.
	RebalanceServers();

	mReplicationTodoStats->Set(mChunkReplicationCandidates.size());
}

/// 为指定的server查找新的工作:
/// @note chunk拷贝复制操作优先考虑chunkID较低的chunk.
void LayoutManager::FindReplicationWorkForServer(ChunkServerPtr &server,
		chunkId_t chunkReplicated)
{
	chunkId_t chunkId;
	int extraReplicas = 0, numOngoing;
	vector<ChunkServerPtr> c;

	if (server->IsRetiring())
		return;

	c.push_back(server);

	//
	// 在调用该函数时, chunkReplicated指定的chunk的复制已经完成, 此时考察是否有它的下一个
	// 序号的chunk存在.
	CRCandidateSetIter citer = mChunkReplicationCandidates.find(chunkReplicated
			+ 1);

	// chunkReplicated + 1所指向的chunk不存在, 此时从mLastChunkReplicated开始向后查找
	if (citer == mChunkReplicationCandidates.end())
	{
		// try to start where we left off last time; if that chunk has
		// disappeared, find something "closeby"
		citer = mChunkReplicationCandidates.upper_bound(mLastChunkReplicated);
	}

	// 如果mLastChunkReplicated已经是当前具有最大ID的chunk, 则从含最小chunkID的chunk
	// 开始复制.
	if (citer == mChunkReplicationCandidates.end())
	{
		mLastChunkReplicated = 1;
		citer = mChunkReplicationCandidates.begin();
	}

	struct timeval start, now;

	gettimeofday(&start, NULL);

	// 利用0.2秒钟的时间, 尽可能多的完成mChunkReplicationCandidates中的复制.
	for (; citer != mChunkReplicationCandidates.end(); ++citer)
	{
		gettimeofday(&now, NULL);

		if (ComputeTimeDiff(start, now) > 0.2)
			// if we have spent more than 200 m-seconds here, stop to
			// serve other requests
			break;

		chunkId = *citer;

		CSMapIter iter = mChunkToServerMap.find(chunkId);

		// 如果该chunkID不合法, 则跳过;
		if (iter == mChunkToServerMap.end())
		{
			continue;
		}
		// 如果该chunk正在进行复制, 则跳过;
		if (iter->second.ongoingReplications > 0)
			continue;

		// if the chunk is already hosted on this server, the chunk isn't a candidate for
		// work to be sent to this server.
		// 如果该chunk已经在这个服务器上有拷贝副本, 或者当前不能进行复制
		if (IsChunkHostedOnServer(iter->second.chunkServers, server)
				|| (!CanReplicateChunkNow(iter->first, iter->second,
						extraReplicas)))
			continue;

		// 如果当前可以进行拷贝, 并且extraReplicas > 0, 则进行复制
		if (extraReplicas > 0)
		{
			if (mRacks.size() > 1)
			{
				// when there is more than one rack, since we
				// are re-replicating a chunk, we don't want to put two
				// copies of a chunk on the same rack.  if one
				// of the nodes is retiring, we don't want to
				// count the node in this rack set---we want to
				// put a block on to the same rack.
				set<int> excludeRacks;

				// 去掉该chunk的服务器列表中正在retiring的服务器.
				for_each(iter->second.chunkServers.begin(),
						iter->second.chunkServers.end(), RackSetter(
								excludeRacks, true));

				// 如果当前的服务器所在的rack在excludeRacks中, 则不进行操作.
				if (excludeRacks.find(server->GetRack()) != excludeRacks.end())
					continue;
			}

			// 调用ReplicateChunk()对该chunk进行1个复制(c中只有一个server, 就是参数中
			// 传进来的server).
			numOngoing = ReplicateChunk(iter->first, iter->second, 1, c);
			iter->second.ongoingReplications += numOngoing;
			if (numOngoing > 0)
			{
				// 因为原来该chunk没有在复制, 所以启动一个复制会使
				// mNumOngoingReplications + 1.
				mNumOngoingReplications++;
				mLastChunkReplicated = chunkId;
			}
		}

		// 如果当前LayoutManager中的并发写操作过多, 则停止进行复制, 防止系统过载.
		if (server->GetNumChunkReplications()
				> MAX_CONCURRENT_WRITE_REPLICATIONS_PER_NODE)
			break;
	}
	// if there is any room left to do more work...
	// 执行该server的负载均衡操作, 如果该server的负载仍然充足
	ExecuteRebalancePlan(server);
}

/*
 * 完成一个服务器上一个chunk的拷贝:
 * 1. 更新LayoutManager中的mOngoingReplicationStats;
 * 2. 更新该chunk在tree中的记录;
 * 3. 向req中的服务器发送拷贝完成命令;
 * 4. 更新该拷贝源数据服务器的读负载;
 * 5. 通知正要进行休眠的服务器写拷贝完成, 命令其冻结该chunk;
 * 6. 如果完成拷贝操作的服务器已经空闲, 则为之查找更多的操作, 避免系统资源空闲, 造成浪费.
 */
void LayoutManager::ChunkReplicationDone(MetaChunkReplicate *req)
{
	vector<ChunkServerPtr>::iterator source;

	// 正在进行的复制数-1
	mOngoingReplicationStats->Update(-1);

	// Book-keeping....
	CSMapIter iter = mChunkToServerMap.find(req->chunkId);

	// 如果在tree中找到了该chunk所对应的map项目, 则更新其中的ongoingReplications数目
	if (iter != mChunkToServerMap.end())
	{
		iter->second.ongoingReplications--;

		// 如果该chunk所有的复制都已经完成, 则mNumOngoingReplications - 1.
		if (iter->second.ongoingReplications == 0)
			// if all the replications for this chunk are done,
			// then update the global counter.
			mNumOngoingReplications--;

		if (iter->second.ongoingReplications < 0)
			// sanity...
			iter->second.ongoingReplications = 0;
	}

	// 向req中指定的server发出完成复制命令
	req->server->ReplicateChunkDone(req->chunkId);

	// 查找req指定的复制操作的源数据服务器
	source = find_if(mChunkServers.begin(), mChunkServers.end(),
			MatchingServer(req->srcLocation));
	if (source != mChunkServers.end())
	{
		// 更新数据源的读操作数
		(*source)->UpdateReplicationReadLoad(-1);
	}

	if (req->status != 0)
	{
		// Replication failed...we will try again later
		KFS_LOG_VA_DEBUG("%s: re-replication for chunk %lld failed, code = %d",
				req->server->GetServerLocation().ToString().c_str(),
				req->chunkId, req->status);
		mFailedReplicationStats->Update(1);
		return;
	}

	// replication succeeded: book-keeping

	// if any of the hosting servers were being "retired", notify them that
	// re-replication of any chunks hosted on them is finished
	// 清除chunkServers列表中, 所有正在retiring的服务器中的evacuating chunks列表中的
	// 该chunk(req->chunkId指定).
	if (iter != mChunkToServerMap.end())
	{
		for_each(iter->second.chunkServers.begin(),
				iter->second.chunkServers.end(), ReplicationDoneNotifier(
						req->chunkId));
	}

	// validate that the server got the latest copy of the chunk
	vector<MetaChunkInfo *> v;
	vector<MetaChunkInfo *>::iterator chunk;

	// 从tree中查找req对应的文件中所包含的所有chunk
	metatree.getalloc(req->fid, v);

	// chunk是从tree中查找到的该chunk的记录
	chunk = find_if(v.begin(), v.end(), ChunkIdMatcher(req->chunkId));
	if (chunk == v.end())
	{
		// Chunk disappeared -> stale; this chunk will get nuked
		KFS_LOG_VA_INFO("Re-replicate: chunk (%lld) disappeared => so, stale",
				req->chunkId);
		mFailedReplicationStats->Update(1);
		req->server->NotifyStaleChunk(req->chunkId);
		return;
	}
	MetaChunkInfo *mci = *chunk;
	// 如果req同当前在tree中存储的chunk的信息不匹配, 则错误退出: 错误的拷贝数+1, 通知
	// 该req对应的server删除req所对应的chunk.
	if (mci->chunkVersion != req->chunkVersion)
	{
		// Version that we replicated has changed...so, stale
		KFS_LOG_VA_INFO("Re-replicate: chunk (%lld) version changed (was=%lld, now=%lld) => so, stale",
				req->chunkId, req->chunkVersion, mci->chunkVersion);
		mFailedReplicationStats->Update(1);
		req->server->NotifyStaleChunk(req->chunkId);
		return;
	}

	// Yaeee...all good...
	KFS_LOG_VA_DEBUG("%s reports that re-replication for chunk %lld is all done",
			req->server->GetServerLocation().ToString().c_str(), req->chunkId);
	// 将完成复制的chunkserver加入到chunk映射中.
	UpdateChunkToServerMapping(req->chunkId, req->server.get());

	// since this server is now free, send more work its way...
	// 如果当前的服务器已经空闲(正在进行的写复制操作数<=1时), 为它找额外的工作去做.
	if (req->server->GetNumChunkReplications() <= 1)
	{
		FindReplicationWorkForServer(req->server, req->chunkId);
	}
}

//
// To delete additional copies of a chunk, find the servers that have the least
// amount of space and delete the chunk from there.  In addition, also pay
// attention to rack-awareness: if two copies are on the same rack, then we pick
// the server that is the most loaded and delete it there
//
/// 删除系统中过多的拷贝: 如果机架数和最终达到的拷贝数相同, 则删除在同一机架上的服务器; 否则,
/// 删除列表中最后几个服务器.
void LayoutManager::DeleteAddlChunkReplicas(chunkId_t chunkId,
		ChunkPlacementInfo &clli, uint32_t extraReplicas)
{
	vector<ChunkServerPtr> servers = clli.chunkServers, copiesToDiscard;
	uint32_t numReplicas = servers.size() - extraReplicas;
	set<int> chosenRacks;

	// if any of the copies are on nodes that are retiring, leave the copies
	// alone; we will reclaim space later if needed
	// 如果该chunk的chunkserver中有正在休眠的服务器, 则退出不执行操作.
	int numRetiringServers = count_if(servers.begin(), servers.end(),
			RetiringServerPred());
	if (numRetiringServers > 0)
		return;

	// We get servers sorted by increasing amount of space utilization; so the candidates
	// we want to delete are at the end
	// 按磁盘空间利用率进行排序, 空间利用率低的排在前面.
	SortServersByUtilization(servers);

	// 提取出来chunkserver列表中的server所在的机架集合
	for_each(servers.begin(), servers.end(), RackSetter(chosenRacks));
	if (chosenRacks.size() == numReplicas)
	{
		// 如果该chunk最终的拷贝数和当前在系统中占用的机架数是相同的, 则删除一部分机器, 使得
		// 每一个机架上只有一个该chunk的拷贝.
		// we need to keep as many copies as racks.  so, find the extra
		// copies on a given rack and delete them
		clli.chunkServers.clear();
		chosenRacks.clear();

		// 按以上规则, 从server列表中选出要保留的chunkserver和要删除的chunkserver.
		for (uint32_t i = 0; i < servers.size(); i++)
		{
			if (chosenRacks.find(servers[i]->GetRack()) == chosenRacks.end())
			{
				chosenRacks.insert(servers[i]->GetRack());
				clli.chunkServers.push_back(servers[i]);
			}
			else
			{
				// second copy on the same rack
				copiesToDiscard.push_back(servers[i]);
			}
		}
	}
	else
	{
		// 如果不符合以上的要求, 则从原chunkserver列表中删除最后的几个server, 使得达到
		// 要求的拷贝数
		clli.chunkServers = servers;
		// Get rid of the extra stuff from the end
		clli.chunkServers.resize(numReplicas);

		// The first N are what we want to keep; the rest should go.
		// 将结尾的几个server中的chunk删除
		copiesToDiscard.insert(copiesToDiscard.end(), servers.begin()
				+ numReplicas, servers.end());
	}
	// 更新chunk信息
	mChunkToServerMap[chunkId] = clli;

	ostringstream msg;
	msg << "Chunk " << chunkId << " lives on: \n";
	for (uint32_t i = 0; i < clli.chunkServers.size(); i++)
	{
		msg << clli.chunkServers[i]->GetServerLocation().ToString() << ' '
				<< clli.chunkServers[i]->GetRack() << "; ";
	}
	msg << "\n";
	msg << "Discarding chunk on: ";
	for (uint32_t i = 0; i < copiesToDiscard.size(); i++)
	{
		msg << copiesToDiscard[i]->GetServerLocation().ToString() << ' '
				<< copiesToDiscard[i]->GetRack() << " ";
	}
	msg << "\n";

	KFS_LOG_VA_INFO("%s", msg.str().c_str());

	// 执行删除操作.
	for_each(copiesToDiscard.begin(), copiesToDiscard.end(), ChunkDeletor(
			chunkId));
}

// 把指定的chunk插入到"需要验证的chunk队列"中
void LayoutManager::ChangeChunkReplication(chunkId_t chunkId)
{
	mChunkReplicationCandidates.insert(chunkId);
}

//
// Check if the server is part of the set of the servers hosting the chunk
//
// 检查server是否在hosters指定的server列表中
bool LayoutManager::IsChunkHostedOnServer(
		const vector<ChunkServerPtr> &hosters, const ChunkServerPtr &server)
{
	vector<ChunkServerPtr>::const_iterator iter;
	iter = find(hosters.begin(), hosters.end(), server);
	return iter != hosters.end();
}

/// 查看指定的服务器的空间利用率是否超过系统规定的上限
class LoadedServerPred
{
public:
	LoadedServerPred()
	{
	}
	bool operator()(const ChunkServerPtr &s) const
	{
		return s->GetSpaceUtilization() > MAX_SERVER_SPACE_UTIL_THRESHOLD;
	}
};

//
// We are trying to move a chunk between two servers on the same rack.  For a
// given chunk, we try to find as many "migration pairs" (source/destination
// nodes) within the respective racks.
//
/// 从nonloadedServer中选择尽可能多的候选服务器存放到candidates中, 选择规则见代码中注释
/// 保证就近复制原则, 如果某一个chunkserver不过载, 则不为该服务器选择
void LayoutManager::FindIntraRackRebalanceCandidates(
		vector<ChunkServerPtr> &candidates,
		const vector<ChunkServerPtr> &nonloadedServers,
		const ChunkPlacementInfo &clli)
{
	vector<ChunkServerPtr>::const_iterator iter;

	// 对于每一个服务器, 都从nonloadedServers中选择符合条件(与clli指定服务器在同一个机架上)的
	// 服务器, 存放到candidates中.
	for (uint32_t i = 0; i < clli.chunkServers.size(); i++)
	{
		vector<ChunkServerPtr> servers;

		// 如果超过规定的磁盘利用率, 则跳过.
		if (clli.chunkServers[i]->GetSpaceUtilization()
				< MAX_SERVER_SPACE_UTIL_THRESHOLD)
		{
			continue;
		}

		//we have a loaded server; find another non-loaded
		//server within the same rack (case 1 from above)
		// 从nonloadedServers中选择在该机架上的, 不包含在clli.chunkServers列表中的
		// 候选服务器, 存放在servers中
		FindCandidateServers(servers, nonloadedServers, clli.chunkServers,
				clli.chunkServers[i]->GetRack());
		if (servers.size() == 0)
		{
			// nothing available within the rack to do the move
			continue;
		}
		// make sure that we are not putting 2 copies of a chunk on the
		// same server
		// 把选出来的服务器不重复的存放在candidates中
		for (uint32_t j = 0; j < servers.size(); j++)
		{
			iter = find(candidates.begin(), candidates.end(), servers[j]);
			if (iter == candidates.end())
			{
				candidates.push_back(servers[j]);
				break;
			}
		}
	}
}

//
// For rebalancing, for a chunk, we could not find a candidate server on the same rack as a
// loaded server.  Hence, we are trying to move the chunk between two servers on two
// different racks.  So, find a migration pair: source/destination on two different racks.
//
/// 机架间的负载均衡: 从nonloadedServers中选择一个与原来的服务器都不再同一机架上的一个服务器,
/// 作为candidate返回.
/// @note 如果机架的数量小于1, 则不能进行机架间均衡负载.
void LayoutManager::FindInterRackRebalanceCandidate(ChunkServerPtr &candidate,
		const vector<ChunkServerPtr> &nonloadedServers,
		const ChunkPlacementInfo &clli)
{
	vector<ChunkServerPtr> servers;

	// result, source, exclude, rackId = -1.
	// 从nonloadedServers列表中查找一组可用的(见FindCandidateServers注释)服务器列表,
	// 存放到servers中
	FindCandidateServers(servers, nonloadedServers, clli.chunkServers);
	if (servers.size() == 0)
	{
		return;
	}
	// XXX: in emulation mode, we have 0 racks due to compile issues
	// 如果机架的数量小于1, 则不能找到除该机架以外的服务器了.
	if (mRacks.size() <= 1)
		return;

	// if we had only one rack then the intra-rack move should have found a
	// candidate.
	assert(mRacks.size()> 1);
	if (mRacks.size() <= 1)
		return;

	// For the candidate we pick, we want to enforce the property that all
	// the copies of the chunks are on different racks.
	// 我们强迫所有的候选服务器都在不同的机架上.
	set<int> excludeRacks;
	for_each(clli.chunkServers.begin(), clli.chunkServers.end(), RackSetter(
			excludeRacks));

	// 从中查找一个不与原来的服务器在同一机架上的一个服务器, 作为candidate返回.
	for (uint32_t i = 0; i < servers.size(); i++)
	{
		set<int>::iterator iter = excludeRacks.find(servers[i]->GetRack());
		if (iter == excludeRacks.end())
		{
			candidate = servers[i];
			return;
		}
	}
}

//
// Periodically, if we find that some chunkservers have LOT (> 80% free) of space
// and if others are loaded (i.e., < 30% free space), move chunks around.  This
// helps with keeping better disk space utilization (and maybe load).
//
int LayoutManager::RebalanceServers()
{
	// 如果系统正在进行恢复, 或者已经连接的chunkserver的数量为0, 则不能进行操作
	if ((InRecovery()) || (mChunkServers.size() == 0))
	{
		return 0;
	}

	// if we are doing rebalancing based on a plan, execute as
	// much of the plan as there is room.

	// 1. 对于所有的chunkserver执行负载均衡操作(移动chunk操作)
	ExecuteRebalancePlan();

	// 2. 更新每一个机架上的总空间数和已经使用的空间数量.
	for_each(mRacks.begin(), mRacks.end(), mem_fun_ref(&RackInfo::computeSpace));

	if (!mIsRebalancingEnabled)
		return 0;

	vector<ChunkServerPtr> servers = mChunkServers;
	vector<ChunkServerPtr> loadedServers, nonloadedServers;
	int extraReplicas, numBlocksMoved = 0;

	// 3. 检查每一个chunkservre的状态和空间利用率, 提取出nonloadedServers(小于空间利用率
	// 下限的)和loadedServer(大于空间利用率上限的)服务器
	for (uint32_t i = 0; i < servers.size(); i++)
	{
		if (servers[i]->IsRetiring())
			continue;
		if (servers[i]->GetSpaceUtilization() < MIN_SERVER_SPACE_UTIL_THRESHOLD)
			nonloadedServers.push_back(servers[i]);
		else if (servers[i]->GetSpaceUtilization()
				> MAX_SERVER_SPACE_UTIL_THRESHOLD)
			loadedServers.push_back(servers[i]);
	}

	// 如果没有空载的服务器(不能执行均衡负载)或者没有过载的服务器(没有均衡必要), 则直接退出
	if ((nonloadedServers.size() == 0) || (loadedServers.size() == 0))
		return 0;

	bool allbusy = false;

	// 4. 查找我们上一次最后一个进行负载的chunkID, 从这个ID开始向后进行负载均衡
	// try to start where we left off last time; if that chunk has
	// disappeared, find something "closeby"
	CSMapIter iter = mChunkToServerMap.find(mLastChunkRebalanced);
	if (iter == mChunkToServerMap.end())
		iter = mChunkToServerMap.upper_bound(mLastChunkRebalanced);

	// 5. 从chunk列表中开始, 逐个查看chunk元素, 对于含有过载chunkserver的服务器进行负载均衡
	for (; iter != mChunkToServerMap.end(); iter++)
	{

		allbusy = true;
		// 在nonloadedServers列表中查找一个正在进行的chunk拷贝低于最高值的一个服务器
		for (uint32_t i = 0; i < nonloadedServers.size(); i++)
		{
			if (nonloadedServers[i]->GetNumChunkReplications()
					<= MAX_CONCURRENT_WRITE_REPLICATIONS_PER_NODE)
			{
				allbusy = false;
				break;
			}
		}

		// 如果nonloadedServers中所有的服务器都在忙于chunk复制, 则退出复制过程.
		if (allbusy)
			break;

		chunkId_t chunkId = iter->first;
		ChunkPlacementInfo &clli = iter->second;
		vector<ChunkServerPtr> candidates;

		// we have seen this chunkId; next time, we'll start time from
		// around here
		mLastChunkRebalanced = chunkId;

		// If this chunk is already being replicated or it is busy, skip
		if ((clli.ongoingReplications > 0) || (!CanReplicateChunkNow(chunkId,
				clli, extraReplicas)))
			continue;

		// 说明此时该chunk已经拥有了过多的拷贝数量, 则不必进行拷贝了
		if (extraReplicas < 0)
			continue;

		// chunk could be moved around if it is hosted on a loaded server
		vector<ChunkServerPtr>::const_iterator csp;
		// 从chunk的chunkserver列表中查找过载的服务器, 如果没有过载服务器, 则直接退出
		csp = find_if(clli.chunkServers.begin(), clli.chunkServers.end(),
				LoadedServerPred());
		if (csp == clli.chunkServers.end())
			continue;

		// there are two ways nodes can be added:
		//  1. new nodes to an existing rack: in this case, we want to
		//  migrate chunk within a rack
		//  2. new rack of nodes gets added: in this case, we want to
		//  migrate chunks to the new rack as long as we don't get more
		//  than one copy onto the same rack

		// 选候选服务器: 使用就近移动原则, 从nonloadedServers中查找在同一rack中的服务器列表
		FindIntraRackRebalanceCandidates(candidates, nonloadedServers, clli);
		if (candidates.size() == 0)
		{
			// 如果机架内没有候选服务器, 则从机架间选择
			ChunkServerPtr cand;

			// 从机架间选取候选服务器
			FindInterRackRebalanceCandidate(cand, nonloadedServers, clli);
			if (cand)
				candidates.push_back(cand);
		}

		if (candidates.size() == 0)
			// no candidates :-(
			continue;
		// get the chunk version
		vector<MetaChunkInfo *> v;
		vector<MetaChunkInfo *>::iterator chunk;

		// 获取clli所在的文件拥有的所有chunk的列表
		metatree.getalloc(clli.fid, v);
		chunk = find_if(v.begin(), v.end(), ChunkIdMatcher(chunkId));
		// 如果该chunk不在该文件中
		if (chunk == v.end())
			continue;

		// 可以进行复制的数为候选服务器的数量(不过载的chunkserver不分配候选服务器)和
		// chunkserver数量中的较小值
		uint32_t numCopies = min(candidates.size(), clli.chunkServers.size());
		uint32_t numOngoing = 0;

		// 所有条件符合要求, 为该chunk进行均衡负载
		numOngoing = ReplicateChunkToServers(chunkId, clli, numCopies,
				candidates);
		if (numOngoing > 0)
		{
			numBlocksMoved++;
		}
	}

	// 如果所有的服务器都没有空闲空间, 则重新从1号chunk开始做起
	if (!allbusy)
		// reset
		mLastChunkRebalanced = 1;

	// 返回移动了的chunk数目
	return numBlocksMoved;
}

/// 对相应的块复制到指定的候选服务器中: 用于均衡负载, 通过进行块复制, 然后再进行块删除来实现
int LayoutManager::ReplicateChunkToServers(chunkId_t chunkId,
		ChunkPlacementInfo &clli, uint32_t numCopies,
		vector<ChunkServerPtr> &candidates)
{
	// 所有的候选服务器都不应该含有该chunk, 否则将不能移动
	for (uint32_t i = 0; i < candidates.size(); i++)
	{
		assert(!IsChunkHostedOnServer(clli.chunkServers, candidates[i]));
	}

	// 建立chunkId的numCopies个拷贝副本到candidates中.
	int numOngoing = ReplicateChunk(chunkId, clli, numCopies, candidates);
	if (numOngoing > 0)
	{
		// add this chunk to the target set of chunkIds that we are tracking
		// for replication status change
		// 把指定的chunk插入到"需要验证的chunk队列"中, 用于删除完成复制之后多余的拷贝
		ChangeChunkReplication(chunkId);

		clli.ongoingReplications += numOngoing;
		mNumOngoingReplications++;
	}
	return numOngoing;
}

/// 通过LayoutManager对指定的chunkserver进行均衡负载: 将mChunkToMove列表中的chunk移动
/// 到这些chunkserver中.
class RebalancePlanExecutor
{
	LayoutManager *mgr;
public:
	RebalancePlanExecutor(LayoutManager *l) :
		mgr(l)
	{
	}
	void operator()(ChunkServerPtr &c)
	{
		// 如果该服务器正在进行休眠, 或者没有相应, 则不执行操作;
		if ((c->IsRetiring()) || (!c->IsResponsiveServer()))
			return;
		// 移动mgr中的mChunkToMove列表中的chunk到c
		mgr->ExecuteRebalancePlan(c);
	}
};

/// 从指定的文件中读入均衡负载计划(删除指定服务器上的指定chunk): planFn指定文件名称.
int LayoutManager::LoadRebalancePlan(const string &planFn)
{
	// load the plan from the specified file
	int fd = open(planFn.c_str(), O_RDONLY);

	if (fd < 0)
	{
		KFS_LOG_VA_INFO("Unable to open: %s", planFn.c_str());
		return -1;
	}

	RebalancePlanInfo_t rpi;
	int rval;

	while (1)
	{
		// 从文件中读入一个RebalancePlanInfo_t到rpi中
		rval = read(fd, &rpi, sizeof(RebalancePlanInfo_t));
		if (rval != sizeof(RebalancePlanInfo_t))
			break;
		ServerLocation loc;
		istringstream ist(rpi.dst);
		vector<ChunkServerPtr>::iterator j;

		// 读入目的服务器的主机名和端口
		ist >> loc.hostname;
		ist >> loc.port;
		// 该服务器应该在chunkserver列表中
		j = find_if(mChunkServers.begin(), mChunkServers.end(), MatchingServer(
				loc));
		if (j == mChunkServers.end())
			continue;
		ChunkServerPtr c = *j;
		// 将读入的chunk加入到指定chunkserver中要删除的chunk列表中
		c->AddToChunksToMove(rpi.chunkId);
	}
	close(fd);

	mIsExecutingRebalancePlan = true;
	KFS_LOG_VA_INFO("Setup for rebalance plan execution from %s is done", planFn.c_str());

	return 0;
}

// 对于所有和该metaserver连接的chunkserver执行负载均衡操作
void LayoutManager::ExecuteRebalancePlan()
{
	// 如果系统没有在执行负载均衡操作, 则不执行该操作直接退出
	if (!mIsExecutingRebalancePlan)
		return;

	// 对所有的chunkserver进行均衡负载, 如果这些chunkserver有空余空间, 则将mChunkToMove
	// 列表中的一部分chunk移动到该服务器中.
	for_each(mChunkServers.begin(), mChunkServers.end(), RebalancePlanExecutor(
			this));

	bool alldone = true;

	// 检查在已经连接的chunkserver中还有没有需要移动的dchunk, 如果有, 则alldone为false.
	for (vector<ChunkServerPtr>::iterator iter = mChunkServers.begin(); iter
			!= mChunkServers.end(); iter++)
	{
		ChunkServerPtr c = *iter;
		set<chunkId_t> chunksToMove = c->GetChunksToMove();

		if (!chunksToMove.empty())
		{
			alldone = false;
			break;
		}
	}

	// 如果所有chunk都已经完成均衡负载, 则mIsExecutingRebalancePlan置为false.
	if (alldone)
	{
		KFS_LOG_INFO("Execution of rebalance plan is complete...");
		mIsExecutingRebalancePlan = false;
	}
}

/// 由于负载均衡设置, 需要从其他地方移动一些chunk到该chunserver中, 此函数负责移动
/// mChunkToMove列表中的chunk到本地, 如果不会超出该server的负载.
void LayoutManager::ExecuteRebalancePlan(ChunkServerPtr &c)
{
	// 获取要移动到该server的chunk列表
	set<chunkId_t> chunksToMove = c->GetChunksToMove();
	vector<ChunkServerPtr> candidates;

	// 如果空间使用率大于最大允许空间使用率, 则清空要移动到该server的列表
	if (c->GetSpaceUtilization() > MAX_SERVER_SPACE_UTIL_THRESHOLD)
	{
		KFS_LOG_VA_INFO("Terminating rebalance plan execution for overloaded server %s", c->ServerID().c_str());
		c->ClearChunksToMove();
		return;
	}

	// 该server的空间利用率没有超过最大允许值
	candidates.push_back(c);

	for (set<chunkId_t>::iterator citer = chunksToMove.begin(); citer
			!= chunksToMove.end(); citer++)
	{
		// 获取该server正在进行的写操作数, 如果超出最大允许并发量, 则退出
		if (c->GetNumChunkReplications()
				> MAX_CONCURRENT_WRITE_REPLICATIONS_PER_NODE)
			return;

		chunkId_t cid = *citer;

		CSMapIter iter = mChunkToServerMap.find(cid);
		int extraReplicas;

		// 如果该chunk正在进行复制, 或者该chunk当前不能进行复制, 则跳过.
		if ((iter->second.ongoingReplications > 0) || (!CanReplicateChunkNow(
				cid, iter->second, extraReplicas)))
			continue;
		// Paranoia...
		// 如果c是这个chunk所在的服务器之一(说明该服务器上已经有了一个该chunk的副本), 则跳过
		if (IsChunkHostedOnServer(iter->second.chunkServers, c))
			continue;

		// 发送该chunk的复制命令, 复制到参数中传过来的chunkserver中
		ReplicateChunkToServers(cid, iter->second, 1, candidates);
	}
}

/// 判断某一个chunk所属的文件是否正在进行写操作和读操作.
class OpenFileChecker
{
	set<fid_t> &readFd, &writeFd;
public:
	OpenFileChecker(set<fid_t> &r, set<fid_t> &w) :
		readFd(r), writeFd(w)
	{
	}
	void operator()(const map<chunkId_t, ChunkPlacementInfo>::value_type p)
	{
		ChunkPlacementInfo c = p.second;
		vector<LeaseInfo>::iterator l;

		l = find_if(c.chunkLeases.begin(), c.chunkLeases.end(), ptr_fun(
				LeaseInfo::IsValidWriteLease));
		if (l != c.chunkLeases.end())
		{
			writeFd.insert(c.fid);
			return;
		}
		l = find_if(c.chunkLeases.begin(), c.chunkLeases.end(), ptr_fun(
				LeaseInfo::IsValidLease));
		if (l != c.chunkLeases.end())
		{
			readFd.insert(c.fid);
			return;
		}
	}
};

/// 获取系统中打开的文件的列表: 包括读和写.
void LayoutManager::GetOpenFiles(string &openForRead, string &openForWrite)
{
	set<fid_t> readFd, writeFd;
	for_each(mChunkToServerMap.begin(), mChunkToServerMap.end(),
			OpenFileChecker(readFd, writeFd));
	// XXX: fill me in..map from fd->path name

}
