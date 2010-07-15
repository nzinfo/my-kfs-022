/*!
 * $Id: base.h 71 2008-07-07 15:49:14Z sriramsrao $
 *
 * \file base.h
 * \brief Base class for KFS metadata nodes.
 * \author Blake Lewis (Kosmix Corp.)
 *
 * Copyright 2008 Quantcast Corp.
 * Copyright 2006-2008 Kosmix Corp.
 *
 * This file is part of Kosmos File System (KFS).
 *
 * Licensed under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#if !defined(KFS_BASE_H)
#define KFS_BASE_H

#include <string>
#include "kfstypes.h"

using std::string;

namespace KFS
{

typedef long long KeyData; //!< "opaque" key data

/*!
 * @brief search key
 *
 * Key values for tree operations.  Q: does this need to
 * be polymorphic?
 */
class Key
{
	MetaType kind; //!< for what kind of metadata
	KeyData kdata1; //!< associated identification
	KeyData kdata2; //!< and more identification
public:
	static const KeyData MATCH_ANY = -1;
	Key(MetaType k, KeyData d) :
		kind(k), kdata1(d), kdata2(0)
	{
	}
	Key(MetaType k, KeyData d1, KeyData d2) :
		kind(k), kdata1(d1), kdata2(d2)
	{
	}
	Key() :
		kind(KFS_UNINIT), kdata1(0), kdata2(0)
	{
	}
	int compare(const Key &test) const;

	// < 重载，一次比较kind, kdata1, kdata2.
	bool operator <(const Key &test) const
	{
		return compare(test) < 0;
	}

	// 如果三个元素（kind, kdata1, kdata2）都相同，则相等
	bool operator ==(const Key &test) const
	{
		return compare(test) == 0;
	}
	bool operator !=(const Key &test) const
	{
		return compare(test) != 0;
	}
};

// MetaNode flag values
static const int META_CPBIT = 1;//!< CP parity bit
static const int META_NEW = 2; //!< new since start of CP
static const int META_ROOT = 4; //!< root node
static const int META_LEVEL1 = 8; //!< children are leaves
static const int META_SKIP = 16; //!< exclude from current CP

/*!
 * \brief base class for both internal and leaf nodes
 */
class MetaNode
{
	MetaType type;
	int flagbits;
public:
	MetaNode(MetaType t) :
		type(t), flagbits(0)
	{
	}
	MetaNode(MetaType t, int f) :
		type(t), flagbits(f)
	{
	}
	virtual ~MetaNode()
	{
	}
	virtual const Key key() const = 0; //!< cons up key value for node
	virtual const string show() const = 0; //!< print out contents
	int flags() const
	{
		return flagbits;
	}

	/// 设置bit指定的位
	void setflag(int bit)
	{
		flagbits |= bit;
	}

	/// 清除bit指定的位
	void clearflag(int bit)
	{
		flagbits &= ~bit;
	}

	/// 测试bit指定的位是否为1
	bool testflag(int bit) const
	{
		return (flagbits & bit) != 0;
	}
};

}
#endif	// !defined(KFS_BASE_H)
