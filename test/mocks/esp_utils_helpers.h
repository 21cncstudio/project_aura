#pragma once

#include <cstddef>

#ifndef __containerof
#define __containerof(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#endif
