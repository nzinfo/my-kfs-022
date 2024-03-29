//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id:$
//
// Created 2008/06/20
// Author: Sriram Rao
//
// Copyright 2008 Quantcast Corp.
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
// \brief Tool that tells the metaserver to mark a node "down" for
// planned downtime.  Nodes can either be hibernated or retired:
// hibernation is a promise that the server will be back after N secs;
// retire => node is going down and don't know when it will be back.
// When a node is "retired" in this manner, the metaserver uses the
// retiring node to proactively replicate the blocks from that server
// to other nodes.
//
//----------------------------------------------------------------------------

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
};

#include <iostream>
#include <string>
#include "libkfsClient/KfsClient.h"

using std::string;
using std::cout;
using std::endl;
using namespace KFS;

KfsClientPtr gKfsClient;

int main()
{
	string host = "10.61.1.92";
	int port = 10625;

	gKfsClient = KFS::getKfsClientFactory()->GetClient(host, port);

	int fd;
	if(!gKfsClient->Exists("/home/fify/test.txt"))
	{
		gKfsClient->Mkdirs("/home/fify");
	}
	if(!gKfsClient->Exists("/home/fify/test.txt"))
	{
		fd = gKfsClient->Open("/home/fify/test.txt", O_CREAT | O_RDWR | O_EXCL);
	}
	else
	{
		fd = gKfsClient->Open("/home/fify/test.txt", O_RDWR | O_EXCL);
	}

	char buf[1024];

	printf("\033[31m\033[1mStart to read file.\033[32m" "\033[m\n");
	int res = gKfsClient->Read(fd, buf, 1024);
	buf[res] = '\0';
	cout << buf << endl;

	//gKfsClient->Close(fd);

	//fd = gKfsClient->Open("/home/fify/test.txt", O_APPEND | O_WRONLY | O_EXCL);

	sleep(5);
	string str = "Append from linux client.\n";
	printf("\033[31m\033[1mStart to write file.\033[32m" "\033[m\n");
	gKfsClient->Write(fd, str.c_str(), str.size());

	//gKfsClient->Close(fd);

	sleep(5);
	//fd = gKfsClient->Open("/home/fify/test.txt", O_RDONLY | O_EXCL);
	printf("\033[31m\033[1mStart to read file again.\033[32m" "\033[m\n");
	gKfsClient->Read(fd, buf, 1024);
	gKfsClient->Close(fd);
	cout << buf << endl;

	return 0;
}
/*
// # of secs for which the node is being hibernated
static void
RetireChunkserver(const ServerLocation &metaLoc, const ServerLocation &chunkLoc,
                  int sleepTime)
{
    TcpSocket metaServerSock;

    if (metaServerSock.Connect(metaLoc) < 0) {
        KFS_LOG_VA_ERROR("Unable to connect to %s",
                         metaLoc.ToString().c_str());
        exit(-1);
    }
    RetireChunkserverOp op(1, chunkLoc, sleepTime);
    int numIO = DoOpCommon(&op, &metaServerSock);
    if (numIO < 0) {
        KFS_LOG_VA_ERROR("Server (%s) isn't responding to retire",
                         metaLoc.ToString().c_str());
        exit(-1);
    }
    metaServerSock.Close();
    if (op.status < 0) {
        cout << "Unable to retire node: " << chunkLoc.ToString() << " status: " << op.status << endl;
        exit(-1);
    }

}

int main(int argc, char **argv)
{
    char optchar;
    bool help = false;
    const char *metaserver = NULL, *chunkserver = NULL;
    int metaport = -1, chunkport = -1, sleepTime = -1;
    bool verboseLogging = false;

    KFS::MsgLogger::Init(NULL);

    while ((optchar = getopt(argc, argv, "hm:p:c:d:s:v")) != -1) {
        switch (optchar) {
            case 'm':
                metaserver = optarg;
                break;
            case 'c':
                chunkserver = optarg;
                break;
            case 'p':
                metaport = atoi(optarg);
                break;
            case 'd':
                chunkport = atoi(optarg);
                break;
            case 's':
                sleepTime = atoi(optarg);
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

    help = help || !metaserver || !chunkserver;

    if (help) {
        cout << "Usage: " << argv[0] << " [-m <metaserver> -p <port>] [-c <chunkserver> -d <port>] "
             << " {-s <sleeptime in seconds>} {-v}" << endl;
        exit(-1);
    }

    if (verboseLogging) {
        KFS::MsgLogger::SetLevel(log4cpp::Priority::DEBUG);
    } else {
        KFS::MsgLogger::SetLevel(log4cpp::Priority::INFO);
    }

    ServerLocation metaLoc(metaserver, metaport);
    ServerLocation chunkLoc(chunkserver, chunkport);

    RetireChunkserver(metaLoc, chunkLoc, sleepTime);
    exit(0);
}

*/
