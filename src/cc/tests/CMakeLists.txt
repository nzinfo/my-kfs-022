#
# $Id: CMakeLists.txt 119 2008-08-10 18:30:24Z sriramsrao $
#
# Created 2006/10/20
# Author: Sriram Rao (Kosmix Corp)
#
# Copyright 2006 Kosmix Corp.
#
# This file is part of Kosmos File System (KFS).
#
# Licensed under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied. See the License for the specific language governing
# permissions and limitations under the License.
#
#

set (exe_files
KfsDataGen
KfsDirFileTester
KfsPerfReader
KfsPerfWriter
KfsReader
KfsSeekWrite
KfsTrunc
KfsWriter
KfsDirScanTest
mkfstree
KfsRW
KfsWriteAfterRead
KfsWriteOnly
KfsReadOnly
)

#
# Every executable depends on its namesake source with _main.cc
#
foreach (exe_file ${exe_files})
        add_executable (${exe_file} ${exe_file}_main.cc)
        if (USE_STATIC_LIB_LINKAGE)
                add_dependencies (${exe_file} kfsClient)
                target_link_libraries (${exe_file} tools kfsClient)
        else (USE_STATIC_LIB_LINKAGE)
                add_dependencies (${exe_file} kfsClient-shared)
                target_link_libraries (${exe_file} tools-shared kfsClient-shared)
        endif (USE_STATIC_LIB_LINKAGE)

endforeach (exe_file)

#
install (TARGETS ${exe_files}
        RUNTIME DESTINATION bin/tests)


