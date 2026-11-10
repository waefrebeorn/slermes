/*
 * include/compat/signal.h -- Windows shim.
 *
 * MinGW-w64 (both the MSYS2 and winlibs flavours) declares _kill() in
 * <process.h> but not the POSIX kill() that the rest of the code uses.
 * Shadow the real <signal.h> and add the missing prototype.
 */
#ifndef HERMES_COMPAT_SIGNAL_H
#define HERMES_COMPAT_SIGNAL_H

#include_next <signal.h>

#include <sys/types.h>

/* MinGW-w64 gates the POSIX signal numbers behind #ifdef _POSIX, which
 * -D_GNU_SOURCE does not define. Supply the ones the code uses. */
#ifndef SIGHUP
#define SIGHUP 1
#endif
#ifndef SIGQUIT
#define SIGQUIT 3
#endif
#ifndef SIGTRAP
#define SIGTRAP 5
#endif
#ifndef SIGKILL
#define SIGKILL 9
#endif
#ifndef SIGPIPE
#define SIGPIPE 13
#endif
#ifndef SIGALRM
#define SIGALRM 14
#endif

#ifdef _WIN32
/* POSIX: int kill(pid_t pid, int sig);
 * MinGW-w64 provides _kill(int, int) in <process.h>. */
#include <process.h>
#ifdef kill
#undef kill
#endif
#define kill(pid, sig) _kill((int)(pid), (int)(sig))
#endif /* _WIN32 */

#endif /* HERMES_COMPAT_SIGNAL_H */
