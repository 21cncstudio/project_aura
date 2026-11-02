#include <vector>
#include <deque>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <lwip/sockets.h>
#include <fcntl.h>
#include "core/CaptiveDnsServer.h"
struct Mock {
 int sockets=0,closes=0,binds=0,receives=0,sends=0;
 bool socket_fail=false,flags_fail=false,bind_fail=false,send_fail=false;
 int recv_error=EAGAIN,flags=0;uint32_t address=0;uint16_t port=0;
 std::deque<std::vector<uint8_t>> input;std::vector<uint8_t> reply;
} g;
int passed=0,failed=0;
void check(bool ok,const char *s){printf("[%s] %s\n",ok?"PASS":"FAIL",s);ok?++passed:++failed;}
int dns_socket(int family,int type,int protocol){check(family==AF_INET&&type==SOCK_DGRAM&&protocol==IPPROTO_UDP,"IPv4 UDP socket");++g.sockets;return g.socket_fail?-1:100;}
int dns_fcntl(int,int operation,int flags){check(operation==F_SETFL,"sets socket flags");g.flags=flags;return g.flags_fail?-1:0;}
int dns_bind(int,const sockaddr *addr,socklen_t size){const auto *v=reinterpret_cast<const sockaddr_in*>(addr);++g.binds;g.address=v->sin_addr.s_addr;g.port=v->sin_port;check(size==sizeof(*v),"bind address length");return g.bind_fail?-1:0;}
int dns_close(int){++g.closes;return 0;}
int dns_recvfrom(int,void *data,size_t size,int,sockaddr *addr,socklen_t *alen){++g.receives;if(g.input.empty()){errno=g.recv_error;return -1;}auto row=g.input.front();g.input.pop_front();auto *v=reinterpret_cast<sockaddr_in*>(addr);v->sin_family=AF_INET;v->sin_port=htons(50000);v->sin_addr.s_addr=0x0204a8c0;*alen=sizeof(*v);size=std::min(size,row.size());memcpy(data,row.data(),size);return int(size);}
int dns_sendto(int,const void *data,size_t size,int,const sockaddr *addr,socklen_t){++g.sends;auto *v=reinterpret_cast<const sockaddr_in*>(addr);check(v->sin_port==htons(50000)&&v->sin_addr.s_addr==0x0204a8c0,"reply targets query sender");g.reply.assign((const uint8_t*)data,(const uint8_t*)data+size);return g.send_fail?-1:int(size);}
std::vector<uint8_t> query(){return {1,2,1,0,0,1,0,0,0,0,0,0,1,'a',0,0,1,0,1};}
int main(){
 {g={};CaptiveDnsServer s;check(s.start(0x0104a8c0),"starts on AP");check(g.address==0x0104a8c0&&g.port==htons(53),"binds only AP IP and DNS port");check(g.flags==O_NONBLOCK,"nonblocking socket");check(s.start(0x0104a8c0)&&g.sockets==1,"same AP start is idempotent");s.stop();s.stop();check(g.closes==1&&!s.running(),"stop closes once");}
 for(int mode=0;mode<3;++mode){g={};g.socket_fail=mode==0;g.flags_fail=mode==1;g.bind_fail=mode==2;CaptiveDnsServer s;check(!s.start(0x0104a8c0)&&!s.running(),"failed setup stays stopped");check(g.closes==(mode?1:0),"setup failure releases acquired socket");g.socket_fail=g.flags_fail=g.bind_fail=false;check(s.start(0x0104a8c0),"setup can retry");}
 {g={};CaptiveDnsServer s;s.start(0x0104a8c0);g.input={query(),query(),query()};s.poll();check(g.receives==2&&g.sends==2&&g.input.size()==1,"one poll processes at most two queries");const uint8_t ip[]={192,168,4,1};check(g.reply.size()==35&&!memcmp(g.reply.data()+31,ip,4),"wire response contains correct AP bytes");s.poll();check(g.sends==3&&s.running(),"next poll handles remaining packet and EAGAIN");}
 {g={};CaptiveDnsServer s;s.start(0x0104a8c0);auto big=query();big.resize(1500);g.input={big,{1,2,3}};s.poll();check(g.sends==0&&s.running(),"oversized and truncated datagrams are discarded");}
 {g={};CaptiveDnsServer s;s.start(0x0104a8c0);g.recv_error=EINTR;s.poll();check(s.running()&&g.receives==1,"EINTR yields this poll");g.recv_error=EIO;s.poll();check(!s.running()&&g.closes==1,"permanent recv failure releases socket for retry");}
 {g={};CaptiveDnsServer s;s.start(0x0104a8c0);g.send_fail=true;g.input={query()};s.poll();check(s.running()&&g.sends==1,"failed nonblocking send is not retried in a loop");}
 {g={};{CaptiveDnsServer s;s.start(0x0104a8c0);check(s.start(0x0105a8c0)&&g.closes==1&&g.address==0x0105a8c0,"address change replaces the socket");}check(g.closes==2,"destruction closes live socket");}
 {g={};CaptiveDnsServer s;check(!s.start(0)&&g.sockets==0,"zero AP address never opens a socket");s.poll();check(g.receives==0,"stopped service performs no I/O");}
 printf("passed=%d failed=%d\n",passed,failed);return failed?1:0;
}
