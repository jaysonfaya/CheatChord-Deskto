/* CheatChord desktop: the few platform pieces the engine needs, for Linux, macOS and Windows (MSVC, clang-cl, MinGW).
 * Threads and atomics map to pthread / C11 where they exist, else to Win32. File names are UTF-8 everywhere; on Windows
 * they are converted to UTF-16 so a user folder like "C:\Users\Jürg" works. */
#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#if defined(_MSC_VER) && !defined(__clang__)
#define CC_PRINTF(a, b)
#define CC_UNUSED
#else
#define CC_PRINTF(a, b) __attribute__((format(printf, a, b)))
#define CC_UNUSED __attribute__((unused))
#endif

#if defined(_WIN32) && (defined(_MSC_VER) || defined(CC_FORCE_WINSHIM))
#define CC_WINSHIM 1
#endif

/* ---------------------------------------------------------------- threads */
#ifdef CC_WINSHIM   /* Win32 underneath (compat.c); windows.h stays out of the engine (it defines PATTERN and more) */
typedef void *pthread_t;
typedef struct { void *p; } pthread_mutex_t;   /* an SRWLOCK */
typedef struct { void *p; } pthread_cond_t;    /* a CONDITION_VARIABLE */
typedef struct { size_t stack; } pthread_attr_t;
int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg);
int pthread_join(pthread_t t, void **ret);
static inline int pthread_attr_init(pthread_attr_t *a) { a->stack = 0; return 0; }
static inline int pthread_attr_destroy(pthread_attr_t *a) { (void)a; return 0; }
static inline int pthread_attr_setstacksize(pthread_attr_t *a, size_t n) { a->stack = n; return 0; }
int pthread_mutex_init(pthread_mutex_t *m, const void *a);
int pthread_mutex_destroy(pthread_mutex_t *m);
int pthread_mutex_lock(pthread_mutex_t *m);
int pthread_mutex_trylock(pthread_mutex_t *m);
int pthread_mutex_unlock(pthread_mutex_t *m);
int pthread_cond_init(pthread_cond_t *c, const void *a);
int pthread_cond_destroy(pthread_cond_t *c);
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);
int pthread_cond_signal(pthread_cond_t *c);
#else
#include <pthread.h>
#endif

/* ---------------------------------------------------------------- atomics (only plain int load / store are used) */
#if (defined(_MSC_VER) && !defined(__clang__)) || defined(CC_FORCE_WINSHIM)
typedef volatile long atomic_int;
long cc_atomic_load(volatile long *p);
void cc_atomic_store(volatile long *p, long v);
#define atomic_load(p) cc_atomic_load(p)
#define atomic_store(p, v) cc_atomic_store((p), (long)(v))
#else
#include <stdatomic.h>
#endif

/* ---------------------------------------------------------------- strings */
#if defined(_MSC_VER) && !defined(__clang__)
#include <string.h>
#define strtok_r strtok_s
#define strdup _strdup
#elif defined(_WIN32)
#include <string.h>
#ifndef strtok_r
#define strtok_r strtok_s
#endif
#endif

/* ---------------------------------------------------------------- files (UTF-8 names) */
FILE *cc_fopen(const char *path, const char *mode);
int cc_mkdir(const char *path);                       /* 0 ok or exists */
int cc_mkdirs(const char *path);                      /* every missing level */
int cc_is_dir(const char *path);
int cc_exists(const char *path);
int cc_rename_replace(const char *from, const char *to);   /* replaces an existing file (Windows too) */
void cc_sleep_ms(int ms);
