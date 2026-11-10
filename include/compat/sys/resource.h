/*
 * include/compat/sys/resource.h -- Windows shim.
 *
 * Windows has no POSIX rlimit interface. getrlimit() reports a generous
 * RLIMIT_NOFILE (the Win32 handle limit is far higher than the POSIX
 * default) and setrlimit() is a no-op, so callers that size a fd budget
 * from it keep working instead of failing to compile.
 */
#ifndef HERMES_COMPAT_SYS_RESOURCE_H
#define HERMES_COMPAT_SYS_RESOURCE_H

#include <string.h>
#include <sys/types.h>
#include <time.h>

#define RLIMIT_CPU      0
#define RLIMIT_FSIZE    1
#define RLIMIT_DATA     2
#define RLIMIT_STACK    3
#define RLIMIT_CORE     4
#define RLIMIT_RSS      5
#define RLIMIT_NPROC    6
#define RLIMIT_NOFILE   7
#define RLIMIT_MEMLOCK  8
#define RLIMIT_AS       9

#define RLIM_INFINITY   (~0UL)
#define RLIM_SAVED_MAX  RLIM_INFINITY
#define RLIM_SAVED_CUR  RLIM_INFINITY

struct rlimit {
    unsigned long rlim_cur;
    unsigned long rlim_max;
};

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
    long ru_maxrss, ru_ixrss, ru_idrss, ru_isrss, ru_minflt, ru_majflt;
    long ru_nswap, ru_inblock, ru_oublock, ru_msgsnd, ru_msgrcv;
    long ru_nsignals, ru_nvcsw, ru_nivcsw;
};

#define RUSAGE_SELF      0
#define RUSAGE_CHILDREN (-1)

static inline int getrlimit(int resource, struct rlimit *rlim)
{
    (void)resource;
    if (!rlim) return -1;
    /* 512 is the MSVCRT default; UCRT raises it substantially. Report a
     * generous ceiling so fd-budget callers do not under-allocate. */
    rlim->rlim_cur = 4096UL;
    rlim->rlim_max = RLIM_INFINITY;
    return 0;
}

static inline int setrlimit(int resource, const struct rlimit *rlim)
{
    (void)resource; (void)rlim;
    return 0;   /* no-op: Windows has no per-process rlimit */
}

static inline int getrusage(int who, struct rusage *usage)
{
    (void)who;
    if (!usage) return -1;
    memset(usage, 0, sizeof(*usage));
    return 0;
}

#endif /* HERMES_COMPAT_SYS_RESOURCE_H */
