//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: cptokfs_main.cc 108 2008-07-30 06:23:39Z sriramsrao $ 
//
// Created 2006/06/23
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
// \brief Tool that copies a file/directory from a local file system to
// KFS.  This tool is analogous to dump---backup a directory from a
// file system into KFS.
//
//----------------------------------------------------------------------------

#include <iostream>    
#include <fstream>
#include <cerrno>
#include <boost/scoped_array.hpp>
using boost::scoped_array;
using std::ios_base;

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <openssl/md5.h>
}

#include "libkfsClient/KfsClient.h"
#include "common/log.h"

#define MAX_FILE_NAME_LEN 256

using std::cout;
using std::endl;
using std::ifstream;
using std::ofstream;
using namespace KFS;

KfsClientPtr gKfsClient;
bool doMkdirs(const char *path);

//
// For the purpose of the cp -r, take the leaf from sourcePath and
// make that directory in kfsPath. 
//
void MakeKfsLeafDir(const string &sourcePath, string &kfsPath);

//
// Given a file defined by sourcePath, copy it to KFS as defined by
// kfsPath
//
int BackupFile(const string &sourcePath, const string &kfsPath);

// Given a dirname, backit up it to dirname.  Dirname will be created
// if it doesn't exist.  
void BackupDir(const string &dirname, string &kfsdirname);

// Guts of the work
int BackupFile2(string srcfilename, string kfsfilename);

int
main(int argc, char **argv)
{
    DIR *dirp;
    string kfsPath = "";
    string serverHost = "";
    int port = -1;
    char *sourcePath = NULL;
    bool help = false;
    bool verboseLogging = false;
    char optchar;
    struct stat statInfo;

    KFS::MsgLogger::Init(NULL);

    while ((optchar = getopt(argc, argv, "d:hk:p:s:v")) != -1) {
        switch (optchar) {
            case 'd':
                sourcePath = optarg;
                break;
            case 'k':
                kfsPath = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 's':
                serverHost = optarg;
                break;
            case 'h':
                help = true;
                break;
            case 'v':
                verboseLogging = true;
                break;
            default:
                KFS_LOG_VA_ERROR("Unrecognized flag %c", optchar);
                help = true;
                break;
        }
    }

    if (help || (sourcePath == NULL) || (kfsPath == "") || (serverHost == "") || (port < 0)) {
        cout << "Usage: " << argv[0] << " -s <meta server name> -p <port> "
             << " -d <source path> -k <Kfs path> {-v}" << endl;
        exit(-1);
    }

    gKfsClient = getKfsClientFactory()->GetClient(serverHost, port);
    if (!gKfsClient) {
	cout << "kfs client failed to initialize...exiting" << endl;
        exit(-1);
    }

    if (verboseLogging) {
        KFS::MsgLogger::SetLevel(log4cpp::Priority::DEBUG);
    } else {
        KFS::MsgLogger::SetLevel(log4cpp::Priority::INFO);
    } 

    if (stat(sourcePath, &statInfo) < 0) {
	cout << "Source path: " << sourcePath << " is non-existent!" << endl;
	exit(-1);
    }

    if (!S_ISDIR(statInfo.st_mode)) {
	BackupFile(sourcePath, kfsPath);
	exit(0);
    }

    if ((dirp = opendir(sourcePath)) == NULL) {
	perror("opendir: ");
        exit(-1);
    }

    // when doing cp -r a/b kfs://c, we need to create c/b in KFS.
    MakeKfsLeafDir(sourcePath, kfsPath);

    BackupDir(sourcePath, kfsPath);

    closedir(dirp);
}

void
MakeKfsLeafDir(const string &sourcePath, string &kfsPath)
{
    string leaf;
    string::size_type slash = sourcePath.rfind('/');

    // get everything after the last slash
    if (slash != string::npos) {
	leaf.assign(sourcePath, slash+1, string::npos);
    } else {
	leaf = sourcePath;
    }
    if (kfsPath[kfsPath.size()-1] != '/')
        kfsPath += "/";

    kfsPath += leaf;
    doMkdirs(kfsPath.c_str());
}

int
BackupFile(const string &sourcePath, const string &kfsPath)
{
    string filename;
    string::size_type slash = sourcePath.rfind('/');

    // get everything after the last slash
    if (slash != string::npos) {
	filename.assign(sourcePath, slash+1, string::npos);
    } else {
	filename = sourcePath;
    }

    // for the dest side: if kfsPath is a dir, we are copying to
    // kfsPath with srcFilename; otherwise, kfsPath is a file (that
    // potentially exists) and we are ovewriting/creating it
    if (gKfsClient->IsDirectory(kfsPath.c_str())) {
        string dst = kfsPath;

        if (dst[kfsPath.size() - 1] != '/')
            dst += "/";
        
        return BackupFile2(sourcePath, dst + filename);
    }
    
    // kfsPath is the filename that is being specified for the cp
    // target.  try to copy to there...
    return BackupFile2(sourcePath, kfsPath);
}

void
BackupDir(const string &dirname, string &kfsdirname)
{
    string subdir, kfssubdir;
    DIR *dirp;
    struct dirent *fileInfo;

    if ((dirp = opendir(dirname.c_str())) == NULL) {
        perror("opendir: ");
        exit(-1);
    }

    if (!doMkdirs(kfsdirname.c_str())) {
        cout << "Unable to make kfs dir: " << kfsdirname << endl;
        closedir(dirp);
	return;
    }

    while ((fileInfo = readdir(dirp)) != NULL) {
        if (strcmp(fileInfo->d_name, ".") == 0)
            continue;
        if (strcmp(fileInfo->d_name, "..") == 0)
            continue;
        
        string name = dirname + "/" + fileInfo->d_name;
        struct stat buf;
        stat(name.c_str(), &buf);

        if (S_ISDIR(buf.st_mode)) {
	    subdir = dirname + "/" + fileInfo->d_name;
            kfssubdir = kfsdirname + "/" + fileInfo->d_name;
            BackupDir(subdir, kfssubdir);
        } else if (S_ISREG(buf.st_mode)) {
	    BackupFile2(dirname + "/" + fileInfo->d_name, kfsdirname + "/" + fileInfo->d_name);
        }
    }
    closedir(dirp);
}

//
// Guts of the work to copy the file.
//
int
BackupFile2(string srcfilename, string kfsfilename)
{
    // 64MB buffers
    const int bufsize = 1 << 26;
    scoped_array<char> kfsBuf;
    int kfsfd, nRead;
    int srcFd;
    int res;

    srcFd = open(srcfilename.c_str(), O_RDONLY);
    if (srcFd  < 0) {
        cout << "Unable to open: " << srcfilename.c_str() << endl;
        exit(-1);
    }

    kfsfd = gKfsClient->Create((char *) kfsfilename.c_str());
    if (kfsfd < 0) {
        cout << "Create " << kfsfilename << " failed: " << kfsfd << endl;
        exit(-1);
    }
    kfsBuf.reset(new char[bufsize]);

    while (1) {
        nRead = read(srcFd, kfsBuf.get(), bufsize);
        
        if (nRead <= 0)
            break;
        
        res = gKfsClient->Write(kfsfd, kfsBuf.get(), nRead);
        if (res < 0) {
            cout << "Write failed with error code: " << res << endl;
            exit(-1);
        }
    }
    close(srcFd);
    gKfsClient->Close(kfsfd);

    return 0;
}

int
BackupFile3(string srcfilename, string kfsfilename)
{
    // 64MB buffers
    const int bufsize = 1 << 26;
    scoped_array<char> kfsBuf;
    int kfsfd, nRead;
    long long n = 0;
    ifstream ifs;
    int res;

    ifs.open(srcfilename.c_str(), ios_base::in);
    if (!ifs) {
        cout << "Unable to open: " << srcfilename.c_str() << endl;
        exit(-1);
    }

    kfsfd = gKfsClient->Create((char *) kfsfilename.c_str());
    if (kfsfd < 0) {
        cout << "Create " << kfsfilename << " failed: " << kfsfd << endl;
        exit(-1);
    }
    kfsBuf.reset(new char[bufsize]);

    while (!ifs.eof()) {
        ifs.read(kfsBuf.get(), bufsize);

        nRead = ifs.gcount();
        
        if (nRead <= 0)
            break;
        
        res = gKfsClient->Write(kfsfd, kfsBuf.get(), nRead);
        if (res < 0) {
            cout << "Write failed with error code: " << res << endl;
            exit(-1);
        }
        n += nRead;
        // cout << "Wrote: " << n << endl;
    }
    ifs.close();
    gKfsClient->Close(kfsfd);

    return 0;
}

bool
doMkdirs(const char *path)
{
    int res;

    res = gKfsClient->Mkdirs((char *) path);
    if ((res < 0) && (res != -EEXIST)) {
        cout << "Mkdir failed: " << ErrorCodeToStr(res) << endl;
        return false;
    }
    return true;
}

