#pragma once
#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif
struct ifreq { char ifr_name[IFNAMSIZ]; };
