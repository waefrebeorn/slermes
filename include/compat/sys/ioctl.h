/* Slermes Windows compat: <sys/ioctl.h> — ioctl() over ioctlsocket(), winsize stub. */
#ifndef SLERMES_COMPAT_SYS_IOCTL_H
#define SLERMES_COMPAT_SYS_IOCTL_H

#include <sys/socket.h>
#include <stdio.h>

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

#ifndef TIOCGWINSZ
#define TIOCGWINSZ 0x5413
#endif

/* Socket-oriented ioctl. Console-size queries return failure rather than a
 * fabricated 80x24 so callers take their "not a tty" path. */
static inline int ioctl(int fd, unsigned long request, void *arg)
{
    if (request == TIOCGWINSZ)
        return -1;
    return ioctlsocket((SOCKET)(uintptr_t)fd, (long)request, (u_long *)arg);
}

#endif /* SLERMES_COMPAT_SYS_IOCTL_H */
