/* Slermes Windows compat: <sys/un.h> — AF_UNIX is a real socket family on Win10+,
 * but the header is absent from MinGW. Layout matches POSIX so SO_PEERCRED-style
 * code paths keep their shape. */
#ifndef SLERMES_COMPAT_SYS_UN_H
#define SLERMES_COMPAT_SYS_UN_H

#include <sys/socket.h>

#ifndef AF_UNIX
#define AF_UNIX 1
#endif
#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif

#ifndef SUN_PATH_MAX
#define SUN_PATH_MAX 108
#endif

struct sockaddr_un {
    unsigned short sun_family;
    char           sun_path[SUN_PATH_MAX];
};

#endif /* SLERMES_COMPAT_SYS_UN_H */
