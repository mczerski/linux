#ifdef __KERNEL__
#ifndef _OR32_CACHE_H
#define _OR32_CACHE_H

#include <asm/processor.h>

#if CONFIG_OR32_DC_LINE!=CONFIG_OR32_IC_LINE
#  warning "insn cachline is different than data cacheline" 
#endif

#define L1_CACHE_BYTES CONFIG_OR32_DC_LINE

#if L1_CACHE_BYTES==16
#  define L1_CACHE_SHIFT     4
#elif L1_CACHE_BYTES==32
#  define L1_CACHE_SHIFT     5
#else
#  error "please define L1_CACHE_SHIFT, and if neccessery correct L1_CACHE_SHIFT_MAX"
#endif

/* Maximum cache line this arch supports */
#  define L1_CACHE_SHIFT_MAX 5

#endif
#endif
