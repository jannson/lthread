#ifndef __LTHR_SETTINGS_H_
#define __LTHR_SETTINGS_H_

#if defined(USE_JEMALLOC)
#include <jemalloc/jemalloc.h>
#endif

#ifdef JEMALLOC_PARCIAL
#define lthr_malloc je_malloc
#define lthr_calloc je_calloc
#define lthr_posix_memalign je_posix_memalign
#define lthr_aligned_alloc je_aligned_alloc
#define lthr_realloc je_realloc
#define lthr_free je_free
#else
#define lthr_malloc malloc
#define lthr_calloc calloc
#define lthr_posix_memalign posix_memalign
#define lthr_aligned_alloc aligned_alloc
#define lthr_realloc realloc
#define lthr_free free
#endif

#endif

