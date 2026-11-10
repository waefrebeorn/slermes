/* Slermes Windows compat: <sys/socket.h>
 * Faithful POSIX surface over Winsock2 so sources keep their original includes.
 * Doctrine: no #ifdef sprinkling in port_*.c — the shim lives here. */
#ifndef SLERMES_COMPAT_SYS_SOCKET_H
#define SLERMES_COMPAT_SYS_SOCKET_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef SSIZE_T ssize_t;
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif

#ifndef SHUT_RD
#define SHUT_RD   SD_RECEIVE
#define SHUT_WR   SD_SEND
#define SHUT_RDWR SD_BOTH
#endif

/* WinSock error numbers are not errno values; map the ones the port tests for. */
#ifndef EINPROGRESS
#define EINPROGRESS WSAEINPROGRESS
#endif
#ifndef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif
#ifndef ECONNRESET
#define ECONNRESET WSAECONNRESET
#endif
#ifndef ENOTCONN
#define ENOTCONN WSAENOTCONN
#endif

/* Winsock takes `const char *` for optval and `int *` for optlen, where POSIX
 * uses a void pointer and socklen_t. These wrappers keep call sites faithful. */
#define setsockopt(s, lvl, opt, val, len) \
    setsockopt((s), (lvl), (opt), (const char *)(val), (int)(len))
#define getsockopt(s, lvl, opt, val, len) \
    getsockopt((s), (lvl), (opt), (char *)(val), (int *)(len))
#define accept(s, addr, len) \
    accept((s), (addr), (int *)(len))
#define bind(s, addr, len) \
    bind((s), (addr), (int)(len))
#define connect(s, addr, len) \
    connect((s), (addr), (int)(len))

#endif /* SLERMES_COMPAT_SYS_SOCKET_H */
