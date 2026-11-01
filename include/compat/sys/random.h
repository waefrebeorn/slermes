/* Slermes Windows compat: <sys/random.h> — getrandom() is not a crypto RNG here.
 * Backed by rand(): fine for jitter/salts, NOT for keys. Callers needing real
 * entropy must use BCryptGenRandom directly. */
#ifndef SLERMES_COMPAT_SYS_RANDOM_H
#define SLERMES_COMPAT_SYS_RANDOM_H
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

#ifndef GRND_NONBLOCK
#define GRND_NONBLOCK 1
#define GRND_RANDOM   2
#endif

static inline ssize_t getrandom(void *buf, size_t buflen, unsigned int flags)
{
    unsigned char *p = (unsigned char *)buf;
    size_t i;
    (void)flags;
    for (i = 0; i < buflen; i++) p[i] = (unsigned char)(rand() & 0xff);
    return (ssize_t)buflen;
}
#endif
