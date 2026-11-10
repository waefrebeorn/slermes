/*
 * include/compat/unistd.h -- Windows shim.
 *
 * hermes_win32.h (force-included by the build) already supplies fork(), kill()
 * and pipe() as fail-closed static inlines. The one thing still missing from
 * MinGW-w64 is memrchr(), a GNU extension.
 */
#ifndef HERMES_COMPAT_UNISTD_H
#define HERMES_COMPAT_UNISTD_H

#include_next <unistd.h>

#ifdef _WIN32
#ifndef HERMES_HAVE_MEMRCHR
#define HERMES_HAVE_MEMRCHR
#include <stddef.h>
static inline void *memrchr(const void *s, int c, size_t n)
{
    const unsigned char *p = (const unsigned char *)s;
    for (size_t i = n; i > 0; i--)
        if (p[i - 1] == (unsigned char)c) return (void *)(p + i - 1);
    return NULL;
}
#endif
#endif /* _WIN32 */

#endif /* HERMES_COMPAT_UNISTD_H */
