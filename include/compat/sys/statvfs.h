/* Slermes Windows compat: <sys/statvfs.h> over GetDiskFreeSpaceEx. */
#ifndef SLERMES_COMPAT_SYS_STATVFS_H
#define SLERMES_COMPAT_SYS_STATVFS_H
#include <windows.h>
#include <string.h>
#include <stdint.h>

typedef unsigned long fsblkcnt_t;
typedef unsigned long fsfilcnt_t;

struct statvfs {
    unsigned long f_bsize;
    unsigned long f_frsize;
    fsblkcnt_t    f_blocks;
    fsblkcnt_t    f_bfree;
    fsblkcnt_t    f_bavail;
    fsfilcnt_t    f_files;
    fsfilcnt_t    f_ffree;
    fsfilcnt_t    f_favail;
    unsigned long f_fsid;
    unsigned long f_flag;
    unsigned long f_namemax;
};

static inline int statvfs(const char *path, struct statvfs *out)
{
    ULARGE_INTEGER avail, total, freeb;
    char root[4];
    if (!out) return -1;
    if (!path || !*path) return -1;
    memset(out, 0, sizeof *out);
    root[0] = (char)((path[0] && path[1] == ':') ? path[0] : 'C');
    root[1] = ':'; root[2] = '\\'; root[3] = '\0';
    if (!GetDiskFreeSpaceExA(root, &avail, &total, &freeb)) return -1;
    out->f_bsize = 4096;
    out->f_frsize = 4096;
    out->f_blocks = (fsblkcnt_t)(total.QuadPart / 4096);
    out->f_bfree = (fsblkcnt_t)(freeb.QuadPart / 4096);
    out->f_bavail = (fsblkcnt_t)(avail.QuadPart / 4096);
    out->f_namemax = 255;
    return 0;
}
static inline int fstatvfs(int fd, struct statvfs *out) { (void)fd; return statvfs(".", out); }
#endif
