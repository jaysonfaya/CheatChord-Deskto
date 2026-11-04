#include "compat.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <direct.h>
#include <process.h>
#include <sys/stat.h>
#include <wchar.h>

static wchar_t *widen(const char *s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) return NULL;
    wchar_t *w = (wchar_t *)malloc(sizeof(wchar_t) * (size_t)n);
    if (w) MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

FILE *cc_fopen(const char *path, const char *mode) {
    wchar_t *wp = widen(path), *wm = widen(mode);
    FILE *f = (wp && wm) ? _wfopen(wp, wm) : NULL;
    free(wp);
    free(wm);
    return f;
}
int cc_mkdir(const char *path) {
    wchar_t *wp = widen(path);
    int r = wp ? _wmkdir(wp) : -1;
    free(wp);
    return r == 0 || errno == EEXIST ? 0 : -1;
}
int cc_is_dir(const char *path) {
    wchar_t *wp = widen(path);
    DWORD a = wp ? GetFileAttributesW(wp) : INVALID_FILE_ATTRIBUTES;
    free(wp);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
int cc_exists(const char *path) {
    wchar_t *wp = widen(path);
    DWORD a = wp ? GetFileAttributesW(wp) : INVALID_FILE_ATTRIBUTES;
    free(wp);
    return a != INVALID_FILE_ATTRIBUTES;
}
int cc_rename_replace(const char *from, const char *to) {
    wchar_t *a = widen(from), *b = widen(to);
    int ok = a && b && MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    free(a);
    free(b);
    return ok ? 0 : -1;
}
void cc_sleep_ms(int ms) { Sleep((DWORD)ms); }

#ifdef CC_WINSHIM
int pthread_mutex_init(pthread_mutex_t *m, const void *a) { (void)a; InitializeSRWLock((PSRWLOCK)m); return 0; }
int pthread_mutex_destroy(pthread_mutex_t *m) { (void)m; return 0; }
int pthread_mutex_lock(pthread_mutex_t *m) { AcquireSRWLockExclusive((PSRWLOCK)m); return 0; }
int pthread_mutex_trylock(pthread_mutex_t *m) { return TryAcquireSRWLockExclusive((PSRWLOCK)m) ? 0 : 16; }
int pthread_mutex_unlock(pthread_mutex_t *m) { ReleaseSRWLockExclusive((PSRWLOCK)m); return 0; }
int pthread_cond_init(pthread_cond_t *c, const void *a) { (void)a; InitializeConditionVariable((PCONDITION_VARIABLE)c); return 0; }
int pthread_cond_destroy(pthread_cond_t *c) { (void)c; return 0; }
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) {
    SleepConditionVariableSRW((PCONDITION_VARIABLE)c, (PSRWLOCK)m, INFINITE, 0);
    return 0;
}
int pthread_cond_signal(pthread_cond_t *c) { WakeConditionVariable((PCONDITION_VARIABLE)c); return 0; }
long cc_atomic_load(volatile long *p) { return InterlockedCompareExchange(p, 0, 0); }
void cc_atomic_store(volatile long *p, long v) { InterlockedExchange(p, v); }
typedef struct { void *(*fn)(void *); void *arg; } tstart_t;
static unsigned __stdcall tmain(void *p) {
    tstart_t t = *(tstart_t *)p;
    free(p);
    t.fn(t.arg);
    return 0;
}
int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg) {
    tstart_t *st = (tstart_t *)malloc(sizeof *st);
    if (!st) return ENOMEM;
    st->fn = fn;
    st->arg = arg;
    uintptr_t h = _beginthreadex(NULL, a ? (unsigned)a->stack : 0, tmain, st, 0, NULL);
    if (!h) { free(st); return EAGAIN; }
    *t = (HANDLE)h;
    return 0;
}
int pthread_join(pthread_t t, void **ret) {
    if (ret) *ret = NULL;
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
    return 0;
}
#endif

#else   /* POSIX */
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

FILE *cc_fopen(const char *path, const char *mode) { return fopen(path, mode); }
int cc_mkdir(const char *path) { return mkdir(path, 0775) == 0 || errno == EEXIST ? 0 : -1; }
int cc_is_dir(const char *path) { struct stat st; return stat(path, &st) == 0 && S_ISDIR(st.st_mode); }
int cc_exists(const char *path) { struct stat st; return stat(path, &st) == 0; }
int cc_rename_replace(const char *from, const char *to) { return rename(from, to); }
void cc_sleep_ms(int ms) {
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}
#endif

int cc_mkdirs(const char *path) {
    char buf[1024];
    size_t n = strlen(path);
    if (n >= sizeof buf) return -1;
    memcpy(buf, path, n + 1);
    for (size_t i = 1; i < n; i++) {
        if (buf[i] == '/' || buf[i] == '\\') {
            if (i == 2 && buf[1] == ':') continue;   /* "C:\" */
            char c = buf[i];
            buf[i] = 0;
            if (!cc_is_dir(buf)) cc_mkdir(buf);
            buf[i] = c;
        }
    }
    return cc_is_dir(buf) ? 0 : cc_mkdir(buf);
}

#ifdef CC_WINSHIM
_Static_assert(sizeof(pthread_mutex_t) == sizeof(SRWLOCK), "SRWLOCK size");
_Static_assert(sizeof(pthread_cond_t) == sizeof(CONDITION_VARIABLE), "CONDITION_VARIABLE size");
#endif
