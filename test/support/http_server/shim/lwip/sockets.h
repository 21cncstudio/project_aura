#pragma once
// Mirror lwIP's BSD close mapping for the C++ transport close callback too.
#define close(fd) sh_close(fd)
