#pragma once
#include <stdlib.h>
#define BIT64(n) (1ULL << (n))
#define MALLOC_CAP_DEFAULT 0
static inline void *heap_caps_calloc(size_t n, size_t s, unsigned caps) {
    (void)caps; return calloc(n, s);
}
