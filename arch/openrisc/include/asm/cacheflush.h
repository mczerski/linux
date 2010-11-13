/*
 * The cache doesn't need to be flushed when TLB entries change when
 * the cache is mapped to physical memory, not virtual memory...
 * that's what the generic implementation gets us.
 */

#include <asm-generic/cacheflush.h>

