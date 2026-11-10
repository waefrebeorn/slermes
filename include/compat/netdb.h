/* Slermes Windows compat: <netdb.h> — getaddrinfo family via ws2tcpip. */
#ifndef SLERMES_COMPAT_NETDB_H
#define SLERMES_COMPAT_NETDB_H

#include <sys/socket.h>

/* Winsock names the same codes with distinct values; add the POSIX-only ones
 * the port references so comparisons keep compiling. */
#ifndef EAI_NODATA
#define EAI_NODATA WSANO_DATA
#endif

#endif /* SLERMES_COMPAT_NETDB_H */
