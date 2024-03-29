//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: KfsRm.cc 104 2008-07-28 06:11:57Z sriramsrao $ 
//
// Created 2006/09/21
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
// \brief Tool that implements rm -rf <path>
//----------------------------------------------------------------------------

#include <iostream>    
#include <fstream>
#include <cerrno>
#include <vector>
#include <string>

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
}

#include "libkfsClient/KfsClient.h"
#include "tools/KfsShell.h"

using std::cout;
using std::endl;
using std::ifstream;
using std::vector;
using std::string;

using namespace KFS;
using namespace KFS::tools;

int doRecrRemove(const char *dirname);
int doRecrRmdir(const char *dirname);
int doRemoveFile(const char *pathname);

int
KFS::tools::handleRm(const vector<string> &args)
{
    if ((args.size() < 1) || (args[0] == "--help") || (args[0] == "")) {
        cout << "Usage: rm <path> " << endl;
        return 0;
    }

    return doRecrRemove(args[0].c_str());
}

int
doRecrRemove(const char *pathname)
{
    int res;
    struct stat statInfo;
    KfsClientPtr kfsClient = getKfsClientFactory()->GetClient();

    res = kfsClient->Stat(pathname, statInfo);
    if (res < 0) {
        cout << "Unable to remove: " <<  pathname << " : " 
             << ErrorCodeToStr(res) << endl;
        return res;
    }
    if (S_ISDIR(statInfo.st_mode)) {
        return kfsClient->Rmdirs(pathname);
    } else {
        return doRemoveFile(pathname);
    }
}

int
doRemoveFile(const char *pathname)
{
    int res;
    KfsClientPtr kfsClient = getKfsClientFactory()->GetClient();

    res = kfsClient->Remove(pathname);
    if (res < 0) {
        cout << "unable to remove: " << pathname <<  ' ' << "error = " << res << endl;
    }

    return res;
}


