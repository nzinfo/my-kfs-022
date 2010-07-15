
#
# $Id: README.txt 159 2008-09-20 05:44:36Z sriramsrao $
#
# Created on 2007/08/23
#
# Copyright 2008 Quantcast Corp.
# Copyright 2007 Kosmix Corp.
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
# Sriram Rao
# Quantcast Corp.
# (srao at quantcast dot com)

=================

Welcome to the Kosmos File System (KFS)!  The documentation is now on the
project Wiki:

http://kosmosfs.wiki.sourceforge.net/

KFS is being released under the Apache 2.0 license. A copy of the license
is included in the file LICENSE.txt.


DIRECTORY ORGANIZATION
======================
 - kfs (top-level directory)
    |
    |---> conf            (sample config files)
    |---> examples        (Example client code for accessing KFS)
    |
    |---> src
           |
           |----> cc
                  |
                  |---> access          (Java/Python glue code)
                  |---> meta            (meta server code)
                  |---> chunk           (chunk server code)
                  |---> libkfsClient    (client library code)
                  |---> libkfsIO        (IO library used by KFS)
                  |---> common          (common declarations)
                  |---> fuse            (FUSE module for Linux)
                  |---> tools           (KFS tools)
           |
           |----> java
                  |---> org/kosmix/kosmosfs/access: Java wrappers to call KFS-JNI code
           |                  
           |----> python
                  |---> tests           (Python test scripts)

MODIFICATION
================================================================================
wed Feb 18 	libkfsIO/
Thu Feb 19 	libkfsIO/
Fri Feb 20 	libkfsIO/DiskConnection, DiskEvent, DiskManager
>------------------------------------------------------------------------------<
Mon Feb 23 	chunk/ChunkManager.h, chunk.h
Tue Feb 24 	chunk/ChunkManager.h, chunkManager.cc
Web Feb 25 	chunk/ChunkManager.cc
Thu Feb 26 	chunk/chunkscrubber_main.cc, ChunkServer_main.cc, ChunkServer.h, 
			ChunkServer.cc, RemoteSyncSM.h, RemoteSyncSM.cc, Utils.cc; 
			common/log.cc, log.h
Fri Feb 27	chunk/ChunkServer.h, ChunkServer.cc, MetaServerSM.h, 
			MetaServerSM.cc
>------------------------------------------------------------------------------<
Mon Mar 2	chunk/ClientSM.h, ClientSM.cc, ClientManager.h, ClientManager.cc, 
			Logger.cc, Logger.h
Tue Mar 3	chunk/LeaseClerk.cc, LeaseClerk.h, Utils.cc, Utils.h
Wed Mar 4	chunk/Replicator.h, Replicator.cc
Thu Mar 5	Document for chunk
Fri Mar 6	Document for chunk
>------------------------------------------------------------------------------<
Mon Mar 9	meta/ChunkServer.h, ChunkServer.cc
Tue Mar 10	meta/base.h, LayoutManager.h, LayoutManager.cc, kfstree.cc, 
			kfstree.h
Wed Mar 11	meta/kfstree.cc, kfstree.h
Thu Mar 12	meta/kfstree.cc, kfstree.h, LayoutManager.cc, kfsops.cc, meta.h, 
			kfsops.cc
Fri Mar 13	meta/kfstree.h, kfsops.cc, kfstree.cc
>------------------------------------------------------------------------------<
Mon Mar 16	meta/kfsops.cc, LayoutManager.cc
Tue Mar 17	meta/LayoutManager.h, LayoutManager.cc
Wed Mar 18	meta/LayoutManager.cc
Thu Mar 19
Fri Mar 20	meta/LayoutManager.cc
>------------------------------------------------------------------------------<
Mon Mar 23 	meta/LayoutManager.cc, checkpoint.h, checkpoint.cc, util.h, util.cc
Tue Mar 24	学校安排中国气象局参观
Wed Mar 25	meta/util.cc, util.h, ChunkReplicator.h, ChunkReplicator.cc, 
			ChunkServerFactory.h, ChunkServerFactory.cc, ClientManager.cc, 
			ClientSM.cc, ClientSM.h, entry.h, entry.cc, kfstypes.h, 
			LeaseCleaner.cc, LeaseCleaner.h, logcompactor_mail.c(?).
Thu Mar 26	meta/checkpoint.cc, checkpoint.h, logger.cc, logger.h, replay.cc, 
			replay.h, restore.h, restore.cc.
Fri Mar 27	学校安排联想参观
>------------------------------------------------------------------------------<
Mon Mar 30	提取kfs程序入口
Tue Mar 31	曙光实习
Wed Apr 1	metaserver数据流图整理
Thu Apr 2	metaserver类图整理
Fri Apr 3	metaserver的log和checkpoint用例和数据流分析
>------------------------------------------------------------------------------<
Tue Apr	7	
================================================================================