/* net6_classify.c — IPv6 address-class predicates.
 *
 * ws2tcpip.h DECLARES these as extern functions:
 *
 *     int IN6_IS_ADDR_UNSPECIFIED(const struct in6_addr *);
 *     int IN6_IS_ADDR_LOOPBACK(const struct in6_addr *);
 *     ...
 *
 * but winlibs' libws2_32.a does not export them (nm finds no definition), so
 * any translation unit that classifies an address fails to link. POSIX
 * <netinet/in.h> has them as macros; this toolchain turned them into
 * declarations with no definition.
 *
 * Defining them here as functions matches the header's declaration exactly.
 * Do not re-express them as macros: an object-like macro included before
 * ws2tcpip.h rewrites the prototype into a syntax error.
 *
 * Byte-wise comparisons, so no dependence on s6_addr16 / s6_addr32.
 */
#include <winsock2.h>
#include <ws2tcpip.h>

#define SL_IN6_BYTE(a, i) ((a)->s6_addr[i])

int IN6_IS_ADDR_UNSPECIFIED(const struct in6_addr *a)
{
    for (int i = 0; i < 16; i++) {
        if (SL_IN6_BYTE(a, i) != 0) return 0;
    }
    return 1;
}

int IN6_IS_ADDR_LOOPBACK(const struct in6_addr *a)
{
    for (int i = 0; i < 15; i++) {
        if (SL_IN6_BYTE(a, i) != 0) return 0;
    }
    return SL_IN6_BYTE(a, 15) == 1;
}

int IN6_IS_ADDR_MULTICAST(const struct in6_addr *a)
{
    return SL_IN6_BYTE(a, 0) == 0xff;
}

int IN6_IS_ADDR_LINKLOCAL(const struct in6_addr *a)
{
    return (SL_IN6_BYTE(a, 0) & 0xff) == 0xfe && (SL_IN6_BYTE(a, 1) & 0xc0) == 0x80;
}

int IN6_IS_ADDR_SITELOCAL(const struct in6_addr *a)
{
    return (SL_IN6_BYTE(a, 0) & 0xff) == 0xfe && (SL_IN6_BYTE(a, 1) & 0xc0) == 0xc0;
}

int IN6_IS_ADDR_V4MAPPED(const struct in6_addr *a)
{
    for (int i = 0; i < 10; i++) {
        if (SL_IN6_BYTE(a, i) != 0) return 0;
    }
    return SL_IN6_BYTE(a, 10) == 0xff && SL_IN6_BYTE(a, 11) == 0xff;
}

int IN6_IS_ADDR_V4COMPAT(const struct in6_addr *a)
{
    for (int i = 0; i < 12; i++) {
        if (SL_IN6_BYTE(a, i) != 0) return 0;
    }
    return !(SL_IN6_BYTE(a, 12) == 0 && SL_IN6_BYTE(a, 13) == 0 &&
             SL_IN6_BYTE(a, 14) == 0 && SL_IN6_BYTE(a, 15) == 1);
}

int IN6_IS_ADDR_MC_NODELOCAL(const struct in6_addr *a)
{
    return IN6_IS_ADDR_MULTICAST(a) && (SL_IN6_BYTE(a, 1) & 0x0f) == 0x01;
}

int IN6_IS_ADDR_MC_LINKLOCAL(const struct in6_addr *a)
{
    return IN6_IS_ADDR_MULTICAST(a) && (SL_IN6_BYTE(a, 1) & 0x0f) == 0x02;
}

int IN6_IS_ADDR_MC_SITELOCAL(const struct in6_addr *a)
{
    return IN6_IS_ADDR_MULTICAST(a) && (SL_IN6_BYTE(a, 1) & 0x0f) == 0x05;
}

int IN6_IS_ADDR_MC_ORGLOCAL(const struct in6_addr *a)
{
    return IN6_IS_ADDR_MULTICAST(a) && (SL_IN6_BYTE(a, 1) & 0x0f) == 0x08;
}
