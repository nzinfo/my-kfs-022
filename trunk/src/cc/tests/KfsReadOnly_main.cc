/*
 * KfsReadOnly_main.cc
 *
 *  Created on: Dec 30, 2009
 *      Author: fify
 */

#include <iostream>
#include <sstream>
#include <fcntl.h>

#include "libkfsClient/KfsClient.h"

using std::cout;
using std::endl;

using std::ostringstream;

KFS::KfsClientPtr gClient;
char *file = "/home/fify/test.txt";

void writeFile(int fd);
void readFile(int fd);
void startTesting();

int main(int argc, char **argv)
{
	gClient = KFS::getKfsClientFactory()->GetClient("client.prop");

	if(gClient == NULL)
	{
		cout << "Error occures while connecting to KFS." << endl;
	}

	startTesting();

	return 0;
}

void startTesting()
{
	int fd = -1;

	if(!gClient->Exists(file))
	{
		cout << "File not exist!: " << file << endl;
		//fd = gClient->Create(file);
		return;
	}
	else
	{
		cout << "File already exist, opening: " << file << endl;
		fd = gClient->Open(file, O_RDWR | O_EXCL);
	}

	if(fd < 0)
	{
		cout << "Open file failed!" << endl;
	}

	readFile(fd);

	gClient->Close(fd);
}

void writeFile(int fd)
{
	printf("\033[31m\033[1m" "Writing file.." "\033[0m\n");
	static int count = 0;
	count ++;
	ostringstream oss;
	oss << "Writing file to kfs from test program: KfsWriteAfterRead" << endl;
	oss << "Write for the " << count << " time." << endl;
	//gClient->Seek(fd, SEEK_END);
	int res = gClient->Write(fd, oss.str().c_str(), oss.str().length());
	gClient->Sync(fd);
	printf("\033[33m\033[1m" "Write file done, %d bytes written." "\033[0m\n", res);
}

void readFile(int fd)
{
	printf("\033[31m\033[1m" "Reading file.." "\033[0m\n");
	char buf[1024];

	//gClient->Seek(fd, SEEK_SET);
	int res = gClient->Read(fd, buf, 1024);
	buf[res] = '\0';
	cout << buf << endl;
	printf("\033[33m\033[1m" "Read file done, %d bytes read." "\033[0m\n", res);
}
