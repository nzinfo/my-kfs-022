/*!
 * $Id: kfstypes.h 71 2008-07-07 15:49:14Z sriramsrao $
 *
 * \file kfstypes.h
 * \brief simple typedefs and enums for the KFS metadata server
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
 *
 */
#if !defined(KFS_TYPES_H)
#define KFS_TYPES_H

#include <cerrno>
#include <cassert>
#include "common/kfstypes.h"

namespace KFS
{

/*!
 * \brief KFS metadata types.
 */
/// 全局信息的类型
enum MetaType
{
	KFS_UNINIT, //!< uninitialized: 未初始化
	KFS_INTERNAL, //!< internal node: 内部节点
	KFS_FATTR, //!< file attributes: 文件属性
	KFS_DENTRY, //!< directory entry: 文件夹目录
	KFS_CHUNKINFO, //!< chunk information: chunk信息
	KFS_SENTINEL = 99999//!< internal use, must be largest
};

/*!
 * \brief KFS file types
 */
enum FileType
{
	KFS_NONE, //!< uninitialized: 未初始化
	KFS_FILE, //!< plain file: 文件
	KFS_DIR //!< directory: 文件夹
};

/*!
 * \brief KFS lease types
 */
enum LeaseType
{
	READ_LEASE, WRITE_LEASE
};

}
#endif // !defined(KFS_TYPES_H)
