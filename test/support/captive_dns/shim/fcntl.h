#pragma once
#define F_SETFL 4
#define O_NONBLOCK 2048
#define fcntl dns_fcntl
int dns_fcntl(int,int,int);
