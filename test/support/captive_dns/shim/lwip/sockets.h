#pragma once
#include <stddef.h>
#include <stdint.h>
typedef unsigned socklen_t;
struct in_addr {uint32_t s_addr;};
struct sockaddr {uint16_t sa_family;char sa_data[14];};
struct sockaddr_in {uint16_t sin_family;uint16_t sin_port;in_addr sin_addr;char pad[8];};
#define AF_INET 2
#define SOCK_DGRAM 2
#define IPPROTO_UDP 17
inline uint16_t htons(uint16_t n){return uint16_t((n<<8)|(n>>8));}
#define socket dns_socket
#define bind dns_bind
#define recvfrom dns_recvfrom
#define sendto dns_sendto
int dns_socket(int,int,int);
int dns_bind(int,const sockaddr*,socklen_t);
int dns_recvfrom(int,void*,size_t,int,sockaddr*,socklen_t*);
int dns_sendto(int,const void*,size_t,int,const sockaddr*,socklen_t);
