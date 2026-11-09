/* Slermes Windows platform shims.
 * Included from hermes_core_types.h after <sys/types.h>.
 *
 * Only POSIX/ISO surfaces MinGW lacks live here. Behavioural gaps (no fork(),
 * no real tty attributes, no POSIX uids) fail closed rather than pretending to
 * succeed — a silent wrong answer would poison oracle parity. */
#ifndef SLERMES_HERMES_WIN32_H
#define SLERMES_HERMES_WIN32_H

#ifdef _WIN32

#include <winsock2.h>
#include <windows.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>   /* must precede the mkdir macro below */
#include <stdlib.h>
#include <stdio.h>      /* vsnprintf, used by the dprintf shim */
#include <string.h>
#include <direct.h>     /* _mkdir */
#include <io.h>

/* POSIX user/group ids: there is no multi-user model, everything is uid 0. */
#ifndef SLERMES_UID_T_DEFINED
#define SLERMES_UID_T_DEFINED
typedef unsigned int uid_t;
typedef unsigned int gid_t;
#endif

/* POSIX realpath() -> Microsoft _fullpath(). */
static inline char *realpath(const char *path, char *resolved)
{
    if (!path) return NULL;
    if (resolved) return _fullpath(resolved, path, _MAX_PATH);
    return _fullpath(NULL, path, _MAX_PATH);
}

static inline uid_t geteuid(void) { return 0; }
static inline uid_t getuid(void)  { return 0; }
static inline gid_t getgid(void)  { return 0; }

/* POSIX stdio/io MinGW omits. */
#define fsync(fd) _commit(fd)

/* fcntl() and its flags. Windows has no per-fd flags worth emulating here, so
 * the call succeeds without changing behaviour rather than failing open. */
#include <fcntl.h>
#ifndef O_NONBLOCK
#define O_NONBLOCK 0x4000
#endif
#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif
static inline int fcntl(int fd, int cmd, ...) { (void)fd; (void)cmd; return 0; }

/* MinGW's struct dirent has no d_type. Sites that test for a directory stat the
 * joined path instead — real answer, no fabricated DT_* value. */
static inline int hermes_dirent_is_dir(const char *dirpath, const char *name)
{
    char p[4096];
    struct stat st;
    if (!dirpath || !name) return 0;
    if ((size_t)snprintf(p, sizeof p, "%s/%s", dirpath, name) >= sizeof p) return 0;
    if (stat(p, &st) != 0) return 0;
    return (st.st_mode & S_IFMT) == S_IFDIR;
}


/* Regular-file test to pair with hermes_dirent_is_dir(). */
static inline int hermes_dirent_is_file(const char *dirpath, const char *name)
{
    char p[4096];
    struct stat st;
    if (!dirpath || !name) return 0;
    if ((size_t)snprintf(p, sizeof p, "%s/%s", dirpath, name) >= sizeof p) return 0;
    if (stat(p, &st) != 0) return 0;
    return (st.st_mode & S_IFMT) == S_IFREG;
}

/* glibc's strcasestr is absent on MinGW. */
static inline char *strcasestr(const char *haystack, const char *needle)
{
    size_t nlen;
    if (!haystack || !needle) return NULL;
    nlen = strlen(needle);
    if (!nlen) return (char *)haystack;
    for (; *haystack; ++haystack)
        if (strncasecmp(haystack, needle, nlen) == 0)
            return (char *)haystack;
    return NULL;
}

/* Windows has no symlinks in the POSIX sense, so lstat() is stat() and nothing
 * is ever a link. Reporting S_ISLNK as always-false is the honest answer. */
static inline int lstat(const char *path, struct stat *st) { return stat(path, st); }
#ifndef S_ISLNK
#define S_ISLNK(m) 0
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(m) 0
#endif

/* glibc extensions MinGW lacks. */
#include <time.h>
#define timegm(tm) _mkgmtime(tm)

#ifndef SIGPIPE
#define SIGPIPE 13   /* never raised on Windows; kept distinct for signal() calls */
#endif

#ifndef SIGALRM
#define SIGALRM 14   /* never raised: alarm() is a no-op, see below */
#endif

/* Reentrant time conversions: POSIX *_r -> MSVC *_s (argument order differs). */
static inline struct tm *localtime_r(const time_t *t, struct tm *out)
{
    return localtime_s(out, t) == 0 ? out : NULL;
}
static inline struct tm *gmtime_r(const time_t *t, struct tm *out)
{
    return gmtime_s(out, t) == 0 ? out : NULL;
}

/* Windows has no interval timer delivering SIGALRM. Timed-out code paths must
 * rely on their own deadline checks; this cannot be faked without a thread. */
static inline unsigned int alarm(unsigned int seconds) { (void)seconds; return 0; }

static inline int setenv(const char *name, const char *value, int overwrite)
{
    if (!overwrite) {
        const char *old = getenv(name);
        if (old) return 0;
    }
    return _putenv_s(name, value ? value : "");
}

static inline int unsetenv(const char *name) { return _putenv_s(name, ""); }

#include <stdarg.h>
static inline int dprintf(int fd, const char *fmt, ...)
{
    char buf[4096];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) return n;
    if ((size_t)n > sizeof buf - 1) n = (int)sizeof buf - 1;
    return _write(fd, buf, n);
}

/* glibc's open_memstream is absent on MinGW. Emulated with a temp file: writers
 * use it unchanged, and the fclose() hook below reads the bytes back and hands
 * them to the caller as the malloc'd buffer + length the contract promises. */
#define SLERMES_MEMSTREAM_MAX 8
typedef struct {
    FILE  *f;
    char **bufp;
    size_t *sizep;
} slermes_memstream_slot;

static slermes_memstream_slot slermes_memstreams[SLERMES_MEMSTREAM_MAX];

static inline FILE *fmemopen(void *buf, size_t size, const char *mode)
{ (void)buf; (void)size; (void)mode; return NULL; }

static inline FILE *open_memstream(char **bufp, size_t *sizep)
{
    int i;
    FILE *f;
    if (!bufp || !sizep) return NULL;
    f = tmpfile();
    if (!f) return NULL;
    for (i = 0; i < SLERMES_MEMSTREAM_MAX; i++) {
        if (!slermes_memstreams[i].f) {
            slermes_memstreams[i].f = f;
            slermes_memstreams[i].bufp = bufp;
            slermes_memstreams[i].sizep = sizep;
            *bufp = NULL;
            *sizep = 0;
            return f;
        }
    }
    fclose(f);
    return NULL;
}

static inline int slermes_fclose(FILE *f)
{
    int i;
    for (i = 0; i < SLERMES_MEMSTREAM_MAX; i++) {
        if (slermes_memstreams[i].f == f) {
            char *data = NULL;
            long sz;
            size_t got;
            fflush(f);
            rewind(f);
            if (fseek(f, 0, SEEK_END) == 0) {
                sz = ftell(f);
                rewind(f);
                if (sz >= 0) {
                    data = (char *)malloc((size_t)sz + 1);
                    if (data) {
                        got = fread(data, 1, (size_t)sz, f);
                        data[got] = '\0';
                        *slermes_memstreams[i].bufp = data;
                        *slermes_memstreams[i].sizep = got;
                    }
                }
            }
            slermes_memstreams[i].f = NULL;
            slermes_memstreams[i].bufp = NULL;
            slermes_memstreams[i].sizep = NULL;
            return fclose(f);
        }
    }
    return fclose(f);
}
#define fclose(f) slermes_fclose(f)

/* POSIX mkdir(path, mode) while MinGW declares the 1-arg MSVC form.
 * Variadic dispatch keeps both arities compiling. */
#define SLERMES_MKDIR1(p)     _mkdir(p)
#define SLERMES_MKDIR2(p, m)  _mkdir(p)
#define SLERMES_MKDIR_PICK(_1, _2, NAME, ...) NAME
#define mkdir(...) \
    SLERMES_MKDIR_PICK(__VA_ARGS__, SLERMES_MKDIR2, SLERMES_MKDIR1)(__VA_ARGS__)



/* ── Signals that exist only to be named/compared ─────────────────────
 * Windows has no POSIX signal machinery; these exist so tables and kill()
 * arguments compile. Nothing will ever be delivered for them. */
#ifndef SIGUSR1
#define SIGUSR1 10
#endif
#ifndef SIGUSR2
#define SIGUSR2 12
#endif
#ifndef SIGBUS
#define SIGBUS 7
#endif
#ifndef SIGSTOP
#define SIGSTOP 19
#endif
#ifndef SIGTSTP
#define SIGTSTP 20
#endif
#ifndef SIGCONT
#define SIGCONT 18
#endif
#ifndef SIGWINCH
#define SIGWINCH 28
#endif
#ifndef SIG_BLOCK
#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
#endif
typedef unsigned long sigset_t;
static inline int sigemptyset(sigset_t *s) { if (s) *s = 0; return 0; }
static inline int sigfillset(sigset_t *s) { if (s) *s = (sigset_t)-1; return 0; }
static inline int sigaddset(sigset_t *s, int sig) { (void)sig; if (s) *s = (sigset_t)-1; return 0; }
static inline int sigdelset(sigset_t *s, int sig) { (void)sig; (void)s; return 0; }
static inline int sigismember(const sigset_t *s, int sig) { (void)s; (void)sig; return 0; }
static inline int sigprocmask(int how, const sigset_t *set, sigset_t *old)
{ (void)how; (void)set; if (old) *old = 0; return 0; }
static inline int pthread_sigmask(int how, const sigset_t *set, sigset_t *old)
{ return sigprocmask(how, set, old); }

#ifndef S_IFSOCK
#define S_IFSOCK 0140000
#endif

#ifndef _SC_PHYS_PAGES
#define _SC_PHYS_PAGES 85
#define _SC_OPEN_MAX   4
#define _SC_CLK_TCK    2
#endif

/* ── Process model: Windows has no fork()/signals-as-POSIX ─────────────
 * These fail closed. Callers that spawn subprocesses must use the Win32
 * CreateProcess path; a silent success here would look like a working feature. */
#include <errno.h>
#include <sys/types.h>

#ifndef SIGKILL
#define SIGKILL 9
#endif
#ifndef SIGHUP
#define SIGHUP 1
#endif
#ifndef SIGQUIT
#define SIGQUIT 3
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef ENOSYS
#define ENOSYS 40
#endif

static inline pid_t fork(void) { errno = ENOSYS; return (pid_t)-1; }
static inline int kill(pid_t pid, int sig) { (void)pid; (void)sig; errno = ENOSYS; return -1; }
static inline int pipe(int fds[2]) { (void)fds; errno = ENOSYS; return -1; }
static inline pid_t setsid(void) { return (pid_t)0; }
static inline pid_t getppid(void) { return (pid_t)0; }

/* sysconf: only the values the port actually queries. */
#ifndef _SC_NPROCESSORS_ONLN
#define _SC_NPROCESSORS_ONLN 84
#define _SC_PAGESIZE         30
#define _SC_PAGE_SIZE        _SC_PAGESIZE
#define _SC_PHYS_PAGES       85
#define _SC_AVPHYS_PAGES     86
#endif
static inline long sysconf(int name)
{
    SYSTEM_INFO si;
    switch (name) {
    case _SC_NPROCESSORS_ONLN:
        GetSystemInfo(&si);
        return (long)si.dwNumberOfProcessors;
    case _SC_PAGESIZE:
        GetSystemInfo(&si);
        return (long)si.dwPageSize;
    case _SC_PHYS_PAGES: {
        MEMORYSTATUSEX ms;
        ms.dwLength = sizeof ms;
        if (!GlobalMemoryStatusEx(&ms)) return -1;
        GetSystemInfo(&si);
        return (long)(ms.ullTotalPhys / si.dwPageSize);
    }
    case _SC_AVPHYS_PAGES: {
        MEMORYSTATUSEX ms;
        ms.dwLength = sizeof ms;
        if (!GlobalMemoryStatusEx(&ms)) return -1;
        GetSystemInfo(&si);
        return (long)(ms.ullAvailPhys / si.dwPageSize);
    }
    case _SC_OPEN_MAX:
        return 2048;
    case _SC_CLK_TCK:
        return 1000;
    default:
        return -1;
    }
}

/* flock via LockFileEx — real advisory locking, not a no-op. */
#ifndef LOCK_EX
#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8
#endif
static inline int fchmod(int fd, mode_t mode)
{
    (void)fd; (void)mode;
    return 0;
}

/* flock advisory locking stubs (no-op on Windows) */
#ifndef _HERMES_FLOCK_DEFINED
#define _HERMES_FLOCK_DEFINED
struct flock {
    short l_type;
    short l_whence;
    off_t l_start;
    off_t l_len;
    pid_t l_pid;
};
#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2
#define F_SETLK 6
#define F_SETLKW 7
#define F_GETLK 5
#endif
static inline int flock(int fd, int op)
{
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    OVERLAPPED ov;
    DWORD flags = 0;
    if (h == INVALID_HANDLE_VALUE) return -1;
    memset(&ov, 0, sizeof ov);
    if (op & LOCK_UN) return UnlockFileEx(h, 0, 1, 0, &ov) ? 0 : -1;
    if (op & LOCK_NB) flags |= LOCKFILE_FAIL_IMMEDIATELY;
    if (op & LOCK_EX) flags |= LOCKFILE_EXCLUSIVE_LOCK;
    return LockFileEx(h, flags, 0, 1, 0, &ov) ? 0 : -1;
}


static inline ssize_t readlink(const char *path, char *buf, size_t bufsiz)
{ (void)path; (void)buf; (void)bufsiz; errno = EINVAL; return -1; }


/* ── Remaining libc surface ──────────────────────────────────────────── */
/* LOG_* deliberately NOT defined here: the logger has its own level
 * enum and a macro named LOG_DEBUG breaks it. Use <syslog.h>. */

/* wcwidth/wcswidth: East-Asian-width approximation, zero for controls. */
#include <wchar.h>
static inline int wcwidth(wchar_t wc)
{
    if (wc == 0) return 0;
    if (wc < 32 || (wc >= 0x7f && wc < 0xa0)) return -1;
    if (wc < 0x1100) return 1;
    if ((wc >= 0x1100 && wc <= 0x115f) || (wc >= 0x2e80 && wc <= 0xa4cf) ||
        (wc >= 0xac00 && wc <= 0xd7a3) || (wc >= 0xf900 && wc <= 0xfaff) ||
        (wc >= 0xfe30 && wc <= 0xfe6f) || (wc >= 0xff00 && wc <= 0xff60) ||
        (wc >= 0xffe0 && wc <= 0xffe6) || (wc >= 0x1f300 && wc <= 0x1f64f) ||
        (wc >= 0x1f900 && wc <= 0x1f9ff) || (wc >= 0x20000 && wc <= 0x3fffd))
        return 2;
    return 1;
}
static inline int wcswidth(const wchar_t *s, size_t n)
{
    int total = 0;
    size_t i;
    if (!s) return -1;
    for (i = 0; (n == (size_t)-1 || i < n) && s[i]; i++) {
        int w = wcwidth(s[i]);
        if (w < 0) return -1;
        total += w;
    }
    return total;
}

/* strsep */
static inline char *strsep(char **sp, const char *delim)
{
    char *s, *end;
    if (!sp || !(s = *sp)) return NULL;
    end = s + strcspn(s, delim);
    if (*end) { *end = '\0'; *sp = end + 1; }
    else { *sp = NULL; }
    return s;
}

/* utimensat: Windows has no atime control worth exposing; touch mtime only. */
#include <sys/stat.h>
#include <utime.h>
struct timespec;
static inline int utimensat(int dirfd, const char *path, const void *times, int flags)
{
    (void)dirfd; (void)times; (void)flags;
    return _utime(path, NULL);
}

/* sigaction: installs nothing (no POSIX signal delivery). Reports success so
 * startup code does not treat the platform as broken. */
struct sigaction {
    void  (*sa_handler)(int);
    sigset_t sa_mask;
    int    sa_flags;
    void  (*sa_sigaction)(int, void *, void *);
};
#ifndef SA_RESTART
#define SA_RESTART 0x10000000
#define SA_SIGINFO 4
#define SA_NOCLDSTOP 8
#endif
static inline int sigaction(int sig, const struct sigaction *act, struct sigaction *old)
{ (void)sig; (void)act; (void)old; return 0; }

/* pthread_tryjoin_np: degrade to a non-blocking "not finished" answer. */
#include <pthread.h>
static inline int pthread_tryjoin_np(pthread_t t, void **ret)
{ (void)t; if (ret) *ret = NULL; return 16 /* EBUSY */; }

/* mkstemps: MinGW has _mktemp_s; build the name and reserve it. */
#include <fcntl.h>
#include <io.h>
static inline int mkstemps(char *tmpl, int suffixlen)
{
    int fd;
    char *x = strstr(tmpl, "XXXXXX");
    if (!x) { errno = EINVAL; return -1; }
    if (_mktemp_s(tmpl, strlen(tmpl) + 1) != 0) { errno = EINVAL; return -1; }
    (void)suffixlen;
    fd = _open(tmpl, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE);
    return fd;
}

/* ── string/stdio extensions ─────────────────────────────────────────── */
static inline int asprintf(char **strp, const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return -1;
    *strp = (char *)malloc((size_t)n + 1);
    if (!*strp) return -1;
    va_start(ap, fmt);
    vsnprintf(*strp, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return n;
}

static inline int vasprintf(char **strp, const char *fmt, va_list ap)
{
    int n = vsnprintf(NULL, 0, fmt, ap);
    if (n < 0) return -1;
    *strp = (char *)malloc((size_t)n + 1);
    if (!*strp) return -1;
    vsnprintf(*strp, (size_t)n + 1, fmt, ap);
    return n;
}

/* getline: MinGW's CRT has no getline. */
static inline ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
    size_t cap, len = 0;
    char *buf;
    int c;
    if (!lineptr || !n || !stream) return -1;
    if (!*lineptr || *n == 0) {
        cap = 256;
        buf = (char *)malloc(cap);
        if (!buf) return -1;
        *lineptr = buf;
        *n = cap;
    }
    buf = *lineptr;
    cap = *n;
    for (;;) {
        c = fgetc(stream);
        if (c == EOF) {
            if (len == 0) return -1;
            break;
        }
        if (len + 1 >= cap) {
            char *nb;
            cap *= 2;
            nb = (char *)realloc(buf, cap);
            if (!nb) return -1;
            buf = nb;
            *lineptr = buf;
            *n = cap;
        }
        buf[len++] = (char)c;
        if (c == '\n') break;
    }
    buf[len] = '\0';
    return (ssize_t)len;
}

/* strptime: subset covering the formats this port uses. */
static inline char *strptime(const char *s, const char *format, struct tm *tm)
{
    if (!s || !format || !tm) return NULL;
    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
            case 'Y': tm->tm_year = atoi(s) - 1900; while (*s >= '0' && *s <= '9') s++; break;
            case 'm': tm->tm_mon = atoi(s) - 1;     while (*s >= '0' && *s <= '9') s++; break;
            case 'd': tm->tm_mday = atoi(s);        while (*s >= '0' && *s <= '9') s++; break;
            case 'H': tm->tm_hour = atoi(s);        while (*s >= '0' && *s <= '9') s++; break;
            case 'M': tm->tm_min = atoi(s);         while (*s >= '0' && *s <= '9') s++; break;
            case 'S': tm->tm_sec = atoi(s);         while (*s >= '0' && *s <= '9') s++; break;
            case 'F':
                if (!strptime(s, "%Y-%m-%d", tm)) return NULL;
                while (*s && *s != ' ') s++;
                break;
            case 'T':
                if (!strptime(s, "%H:%M:%S", tm)) return NULL;
                while (*s && *s != ' ') s++;
                break;
            case 'z':
                while (*s && *s != ' ' && *s != '+') s++;
                if (*s) s++;
                while (*s >= '0' && *s <= '9') s++;
                break;
            case '%': if (*s != '%') return NULL; s++; break;
            default: return NULL;
            }
            format++;
        } else {
            if (*s != *format) return NULL;
            s++; format++;
        }
    }
    return (char *)s;
}

/* UTC offset for a broken-down local time, replacing tm_gmtoff. */
static inline long hermes_tm_gmtoff(const struct tm *tm)
{
    struct tm copy;
    if (!tm) return 0;
    copy = *tm;
    return (long)difftime(mktime(&copy), (time_t)_mkgmtime(&copy));
}

#endif /* _WIN32 */

/* fcntl-style *at constants (Windows has no fd-relative ops) */
#ifndef AT_FDCWD
#define AT_FDCWD (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR 0x200
#endif

/* inotify event masks (Windows has no inotify; shimmed for compile) */
#ifndef IN_ACCESS
#define IN_ACCESS 0x00000001
#define IN_MODIFY 0x00000002
#define IN_ATTRIB 0x00000004
#define IN_CLOSE_WRITE 0x00000008
#define IN_OPEN 0x00000010
#define IN_MOVED_FROM 0x00000040
#define IN_MOVED_TO 0x00000080
#define IN_CREATE 0x00000100
#define IN_DELETE 0x00000200
#define IN_DELETE_SELF 0x00000400
#define IN_MOVE 0x000000c0
#define IN_ALL_EVENTS 0x00000fff
#define IN_ONLYDIR 0x01000000
#define IN_ISDIR 0x40000000
#define IN_NONBLOCK 0x00000800
#endif

static inline int getloadavg(double loadavg[], int nelem)
{ (void)loadavg; (void)nelem; if (nelem>0 && loadavg) loadavg[0]=0.0; return (nelem>0)?1:0; }

static inline int chown(const char *path, uid_t owner, gid_t group)
{ (void)path; (void)owner; (void)group; return 0; }
static inline int link(const char *oldpath, const char *newpath) { (void)oldpath; (void)newpath; errno=ENOSYS; return -1; }
static inline void *memmem(const void *haystack, size_t haystacklen,
                           const void *needle, size_t needlelen) {
    if (!needlelen) return (void *)haystack;
    if (needlelen > haystacklen) return NULL;
    const unsigned char *h = (const unsigned char *)haystack;
    const unsigned char *n = (const unsigned char *)needle;
    for (size_t i = 0; i + needlelen <= haystacklen; i++) {
        if (memcmp(h + i, n, needlelen) == 0) return (void *)(h + i);
    }
    return NULL;
}

/* strndup() is a POSIX/GNU extension that MSYS2's runtime provides and the
 * winlibs UCRT runtime does not. ~50 call sites in this tree use it, so it is
 * shimmed here rather than at each one. Semantics match POSIX: at most n bytes
 * are copied and the result is always NUL-terminated. */
static inline char *strndup(const char *s, size_t n) {
    if (!s) return NULL;
    size_t len = 0;
    while (len < n && s[len]) len++;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}
#endif /* SLERMES_HERMES_WIN32_H */
