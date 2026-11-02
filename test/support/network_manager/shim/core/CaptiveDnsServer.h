#pragma once
#include <stdint.h>
namespace LabDns {
inline int starts=0,polls=0,live=0;
inline bool fail_start=false;
inline void reset() { starts=polls=live=0;fail_start=false; }
}
class CaptiveDnsServer {
 bool active_=false;
public:
 ~CaptiveDnsServer(){stop();}
 bool start(uint32_t){++LabDns::starts;if(LabDns::fail_start)return false;if(!active_)++LabDns::live;active_=true;return true;}
 bool running()const{return active_;}
 void stop(){if(active_)--LabDns::live;active_=false;}
 void poll(){if(active_)++LabDns::polls;}
};
