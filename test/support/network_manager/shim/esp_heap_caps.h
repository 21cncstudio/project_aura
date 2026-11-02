#pragma once
#include <stdint.h>
#include <stddef.h>
#ifndef MALLOC_CAP_INTERNAL
#define MALLOC_CAP_INTERNAL 0
#define MALLOC_CAP_8BIT 0
#endif
uint32_t heap_caps_get_free_size(uint32_t);
uint32_t heap_caps_get_largest_free_block(uint32_t);
