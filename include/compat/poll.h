/* Slermes Windows compat: <poll.h> — POSIX poll() backed by WSAPoll(). */
#ifndef SLERMES_COMPAT_POLL_H
#define SLERMES_COMPAT_POLL_H

#include <sys/socket.h>
#ifndef nfds_t
#ifndef _NFDS_T_DEFINED
typedef unsigned long nfds_t;
#endif
#endif

#ifndef POLLIN
#define POLLIN   POLLRDNORM
#endif
#ifndef POLLOUT
#define POLLOUT  POLLWRNORM
#endif
#ifndef POLLPRI
#define POLLPRI  POLLRDBAND
#endif

/* WSAPoll has identical semantics for our usage. */
#define poll(fds, nfds, timeout) WSAPoll((fds), (nfds), (timeout))

#endif /* SLERMES_COMPAT_POLL_H */
