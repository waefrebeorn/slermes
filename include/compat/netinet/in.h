/* Slermes Windows compat: <netinet/in.h> — struct sockaddr_in etc. come from Winsock. */
#ifndef SLERMES_COMPAT_NETINET_IN_H
#define SLERMES_COMPAT_NETINET_IN_H

#include <sys/socket.h>

#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001UL
#endif
#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffffUL
#endif

#endif /* SLERMES_COMPAT_NETINET_IN_H */
