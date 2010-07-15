//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: kfs_fuse_main.cc 136 2008-08-25 02:18:42Z sriramsrao $
//
// Created 2006/11/01
// Author: Blake Lewis (Kosmix Corp.)
//
// Copyright 2006 Kosmix Corp.
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

#include "../libkfsClient/KfsClient.h"

extern "C"
{
#define FUSE_USE_VERSION	25
#define _FILE_OFFSET_BITS	64
#include <fuse.h>
#include <sys/stat.h>
#include <string.h>
}

using std::vector;
using namespace KFS;

static string FUSE_KFS_PROPERTIES = "./client.prp";
static KFS::KfsClientPtr client;

void *fuse_init()
{
	client = getKfsClientFactory()->GetClient(FUSE_KFS_PROPERTIES.c_str());
	return client->IsInitialized() ? client.get() : NULL;
}

void fuse_destroy(void *cookie)
{
	client.reset();
}

static int fuse_getattr(const char *path, struct stat *s)
{
	return client->Stat(path, *s);
}

static int fuse_mkdir(const char *path, mode_t mode)
{
	return client->Mkdir(path);
}

static int fuse_unlink(const char *path)
{
	return client->Remove(path);
}

static int fuse_rmdir(const char *path)
{
	return client->Rmdir(path);
}

static int fuse_rename(const char *src, const char *dst)
{
	return client->Rename(src, dst, false);
}

static int fuse_truncate(const char *path, off_t size)
{
	int fd = client->Open(path, O_WRONLY);
	if (fd < 0)
		return fd;
	int status = client->Truncate(fd, size);
	client->Close(fd);
	return status;
}

static int fuse_open(const char *path, struct fuse_file_info *finfo)
{
	int res = client->Open(path, finfo->flags);
	if (res >= 0)
		return 0;
	return res;
}

static int fuse_create(const char *path, mode_t mode,
		struct fuse_file_info *finfo)
{
	int res = client->Create(path);
	if (res >= 0)
		return 0;
	return res;
}

static int fuse_read(const char *path, char *buf, size_t nread, off_t off,
		struct fuse_file_info *finfo)
{
	int fd = client->Open(path, O_RDONLY);
	if (fd < 0)
		return fd;
	int status = client->Seek(fd, off, SEEK_SET);
	if (status == 0)
		status = client->Read(fd, buf, nread);
	client->Close(fd);
	return status;
}

static int fuse_write(const char *path, const char *buf, size_t nwrite,
		off_t off, struct fuse_file_info *finfo)
{
	int fd = client->Open(path, O_WRONLY);
	if (fd < 0)
		return fd;
	int status = client->Seek(fd, off, SEEK_SET);
	if (status == 0)
		status = client->Write(fd, buf, nwrite);
	client->Close(fd);
	return status;
}

static int fuse_flush(const char *path, struct fuse_file_info *finfo)
{
	int fd = client->Fileno(path);
	if (fd < 0)
		return fd;
	return client->Sync(fd);
}

static int fuse_fsync(const char *path, int flags, struct fuse_file_info *finfo)
{
	int fd = client->Open(path, O_RDONLY);
	if (fd < 0)
		return fd;
	return client->Sync(fd);
}

static int fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *finfo)
{
	vector<KfsFileAttr> contents;
	int status = client->ReaddirPlus(path, contents);
	if (status < 0)
		return status;
	int n = contents.size();
	for (int i = 0; i != n; i++)
	{
		struct stat s;
		memset(&s, 0, sizeof s);
		s.st_ino = contents[i].fileId;
		s.st_mode = contents[i].isDirectory ? S_IFDIR : S_IFREG;
		if (filler(buf, contents[i].filename.c_str(), &s, 0) != 0)
			break;
	}
	return 0;
}

// 定义fuse文件系统的操作:
/*
struct fuse_operations
{
	// 1.
	int (*getattr) (const char *, struct stat *);
	int (*readlink) (const char *, char *, size_t);
	int (*getdir) (const char *, fuse_dirh_t, fuse_dirfil_t);
	int (*mknod) (const char *, mode_t, dev_t);
	int (*mkdir) (const char *, mode_t);
	// 2.
	int (*unlink) (const char *);
	int (*rmdir) (const char *);
	int (*symlink) (const char *, const char *);
	int (*rename) (const char *, const char *);
	int (*link) (const char *, const char *);
	// 3.
	int (*chmod) (const char *, mode_t);
	int (*chown) (const char *, uid_t, gid_t);
	int (*truncate) (const char *, off_t);
	int (*utime) (const char *, struct utimbuf *);
	int (*open) (const char *, struct fuse_file_info *);
	// 4.
	int (*read) (const char *, char *, size_t, off_t, struct fuse_file_info *);
	int (*write) (const char *, const char *, size_t, off_t,struct fuse_file_info *);
	int (*statfs) (const char *, struct statvfs *);
	int (*flush) (const char *, struct fuse_file_info *);
	int (*release) (const char *, struct fuse_file_info *);
	// 5.
	int (*fsync) (const char *, int, struct fuse_file_info *);
	int (*setxattr) (const char *, const char *, const char *, size_t, int);
	int (*getxattr) (const char *, const char *, char *, size_t);
	int (*listxattr) (const char *, char *, size_t);
	int (*removexattr) (const char *, const char *);
	// 6.
	int (*opendir) (const char *, struct fuse_file_info *);
	int (*readdir) (const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
	int (*releasedir) (const char *, struct fuse_file_info *);
	int (*fsyncdir) (const char *, int, struct fuse_file_info *);
	void *(*init) (struct fuse_conn_info *conn);
	// 7.
	void (*destroy) (void *);
	int (*access) (const char *, int);
	int (*create) (const char *, mode_t, struct fuse_file_info *);
	int (*ftruncate) (const char *, off_t, struct fuse_file_info *);
	int (*fgetattr) (const char *, struct stat *, struct fuse_file_info *);
	// 8.
	int (*lock) (const char *, struct fuse_file_info *, int cmd, struct flock *);
	int (*utimens) (const char *, const struct timespec tv[2]);
	int (*bmap) (const char *, size_t blocksize, uint64_t *idx);
};
 */
/// 定一fuse文件系统的操作, 然后整个系统通过fuse来实现.
struct fuse_operations ops =
{
	// 1.
	fuse_getattr,
	NULL, /* readlink */
	NULL, /* getdir */
	NULL, /* mknod */
	fuse_mkdir,
	// 2.
	fuse_unlink,
	fuse_rmdir,
	NULL, /* symlink */
	fuse_rename,
	NULL, /* link */
	// 3.
	NULL, /* chmod */
	NULL, /* chown */
	fuse_truncate,
	NULL, /* utime */
	fuse_open,
	// 4.
	fuse_read,
	fuse_write,
	NULL, /* statfs */
	fuse_flush, /* flush */
	NULL, /* release */
	// 5.
	fuse_fsync, /* fsync */
	NULL, /* setxattr */
	NULL, /* getxattr */
	NULL, /* listxattr */
	NULL, /* removexattr */
	// 6.
	NULL, /* opendir */
	fuse_readdir,
	NULL, /* releasedir */
	NULL, /* fsyncdir */
	fuse_init,
	// 7.
	fuse_destroy,
	NULL, /* access */
	fuse_create, /* create */
	NULL, /* ftruncate */
	NULL /* fgetattr */
};

// 实现客户端的fuse文件系统
int main(int argc, char **argv)
{
	// 初始化fuse文件系统, 该文件系统通过ops定义
	return fuse_main(argc, argv, &ops);
}
