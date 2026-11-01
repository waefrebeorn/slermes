/* Slermes Windows compat: <pty.h> — no POSIX pty on Windows. Fails closed. */
#ifndef SLERMES_COMPAT_PTY_H
#define SLERMES_COMPAT_PTY_H
#include <errno.h>
#include <sys/types.h>
static inline int openpty(int *am, int *as, char *name, const void *tio, const void *ws)
{ (void)am; (void)as; (void)name; (void)tio; (void)ws; errno = ENOSYS; return -1; }
static inline pid_t forkpty(int *am, char *name, const void *tio, const void *ws)
{ (void)am; (void)name; (void)tio; (void)ws; errno = ENOSYS; return (pid_t)-1; }
#endif
