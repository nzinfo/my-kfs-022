//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: kfsdecls.h 71 2008-07-07 15:49:14Z sriramsrao $
//
// \brief Common declarations of KFS structures
//
// Created 2006/10/20
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
//----------------------------------------------------------------------------

#ifndef COMMON_KFSDECLS_H
#define COMMON_KFSDECLS_H

#include <cassert>
#include <cerrno>
#include <string>
#include <sstream>
#include <stdlib.h>

namespace KFS
{
///
/// Define a server process' location: hostname and the port at which
/// it is listening for incoming connections
///
/// 用hostname和port唯一标记一个server
struct ServerLocation
{
	ServerLocation() :
		hostname(""), port(-1)
	{
	}
	ServerLocation(const ServerLocation &other) :
		hostname(other.hostname), port(other.port)
	{
	}
	ServerLocation(const std::string &h, int p) :
		hostname(h), port(p)
	{
	}
	ServerLocation & operator =(const ServerLocation &other)
	{
		hostname = other.hostname;
		port = other.port;
		return *this;
	}
	void Reset(const char *h, int p)
	{
		hostname = h;
		port = p;
	}
	bool operator ==(const ServerLocation &other) const
	{
		return hostname == other.hostname && port == other.port;
	}
	bool operator !=(const ServerLocation &other) const
	{
		return hostname != other.hostname || port != other.port;
	}
	bool IsValid() const
	{
		// Hostname better be non-null and port better
		// be a positive number
		return hostname.compare("") != 0 && port > 0;
	}

	// a proxy for distance between two hosts: take the difference
	// between their hostnames.  this will mostly work as long as all
	// machines in the cluster are named as nodeXXX, where XXX is a number
	// 通过主机号判别距离，只能应用于命名方法为：nodeXXX的地方。
	int Distance(const std::string &otherhost)
	{
		int len = (int) std::min(hostname.size(), otherhost.size());
		int hosta = 0, hostb = 0;
		int scalefactor = 1;

		for (int i = len - 1; i >= 0; --i)
		{
			if (isdigit(hostname[i]))
				hosta += (hostname[i] - '0') * scalefactor;
			if (isdigit(otherhost[i]))
				hostb += (otherhost[i] - '0') * scalefactor;
			scalefactor *= 10;
		}
		return abs(hosta - hostb);
	}

	// 将主机转换为字符串输出
	std::string ToString() const
	{
		std::ostringstream os;

		os << hostname << ' ' << port;
		return os.str();
	}

	// 从字符串读入主机名，命名规则“hostname port_number"
	void FromString(const std::string &s)
	{
		std::istringstream is(s);

		is >> hostname;
		is >> port;
	}

	std::string hostname; //!< Location of the server: machine name
	int port; //!< Location of the server: port to connect to
};

}

#endif // COMMON_KFSDECLS_H
