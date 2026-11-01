/* Slermes Windows compat: <sys/select.h> — Winsock select(); nfds is ignored. */
#ifndef SLERMES_COMPAT_SYS_SELECT_H
#define SLERMES_COMPAT_SYS_SELECT_H
#include <sys/socket.h>
#include <sys/time.h>
#ifndef nfds_t
typedef unsigned long nfds_t;
#endif
#define select(n, r, w, e, t) select(0, (r), (w), (e), (t))
#endif
