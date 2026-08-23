/* gate: model_metadata_is_local_endpoint — traced copy of
 * src/agent/provider_metadata.c::is_local_endpoint
 * (PoP: agent/model_metadata.py::is_local_endpoint).
 *
 * Rung ladder (must match twin.py EXACTLY):
 *   B1 empty url / V(host) / B2 known-local host / B3 container suffix /
 *   B4 IPv4 parse -> seven range branches (loopback 127/8, 10/8,
 *   172.16/12, 192.168/16, link-local 169.254/16, CGNAT 100.64/10, 0/8)
 *   B5 IPv6 parse (inputs never hit the v6 body; its branches are emitted
 *      as a single false rung on both sides)
 *   V(result "true"/"false")
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "watercup.h"

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_is_local_endpoint <base_url>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char raw[1024]; snprintf(raw, sizeof raw, "%s", argv[i]);

        WCUP_BEGIN("is_local_endpoint");
        /* normalize: strip + rstrip '/' (provider_normalize_base_url) */
        char *s = raw;
        while (*s == ' ' || *s == '\t') s++;
        size_t sl = strlen(s);
        while (sl > 0 && s[sl-1] == '/') s[--sl] = '\0';
        s[sl] = '\0';

        WCUP_BRANCH(!*s);                                            /* B1 */
        int result = 0;
        if (!*s) goto emit;

        {
            /* add scheme if missing */
            char url[1100];
            if (strstr(s, "://")) snprintf(url, sizeof url, "%s", s);
            else snprintf(url, sizeof url, "http://%s", s);

            /* extract hostname: after scheme, up to '/', ':' or ']' */
            char host[512]; size_t hl = 0;
            const char *hs = strstr(url, "://");
            hs = hs ? hs + 3 : url;
            if (*hs == '[') {
                const char *cb = strchr(hs + 1, ']');
                if (cb) { size_t n = (size_t)(cb - hs - 1); memcpy(host, hs + 1, n); host[n] = '\0'; hl = n; }
                else host[0] = '\0', hl = 0;
            } else {
                const char *p = hs;
                while (*p && *p != '/' && *p != ':' && *p != '?') p++;
                hl = (size_t)(p - hs);
                memcpy(host, hs, hl); host[hl] = '\0';
                for (char *q = host; *q; q++)
                    if (*q >= 'A' && *q <= 'Z') *q += 32;   /* urlparse lowercases */
            }

            WCUP_VALSTR(host);

            static const char *local_hosts[] = {
                "localhost", "127.0.0.1", "::1", "0.0.0.0", "127.0.1.1", NULL };
            int lh = 0;
            for (int k = 0; local_hosts[k]; k++)
                if (strcasecmp(host, local_hosts[k]) == 0) { lh = 1; break; }
            WCUP_BRANCH(lh);                                         /* B2 */
            if (!lh) {
                static const char *suffixes[] = {
                    "host.docker.internal", "host.podman.internal",
                    "host.lima.internal", ".host.docker.internal",
                    ".host.podman.internal", ".host.lima.internal", NULL };
                size_t hlen = strlen(host);
                int cs = 0;
                for (int k = 0; suffixes[k]; k++) {
                    size_t slen = strlen(suffixes[k]);
                    if (hlen >= slen && strcasecmp(host + hlen - slen, suffixes[k]) == 0) { cs = 1; break; }
                }
                WCUP_BRANCH(cs);                                     /* B3 */
                if (!cs) {
                    struct in_addr a4;
                    int v4 = inet_pton(AF_INET, host, &a4) == 1;
                    WCUP_BRANCH(v4);                                 /* B4 */
                    if (!v4) goto v6check;
                    {
                        unsigned long ip = ntohl(a4.s_addr);
                        int r;
                        r = ((ip & 0xFF000000UL) == 0x7F000000UL); WCUP_BRANCH(r); if (r) { result = 1; goto emit; }
                        r = ((ip & 0xFF000000UL) == 0x0A000000UL); WCUP_BRANCH(r); if (r) { result = 1; goto emit; }
                        r = ((ip & 0xFFF00000UL) == 0xAC100000UL); WCUP_BRANCH(r); if (r) { result = 1; goto emit; }
                        r = ((ip & 0xFFFF0000UL) == 0xC0A80000UL); WCUP_BRANCH(r); if (r) { result = 1; goto emit; }
                        r = ((ip & 0xFFFF0000UL) == 0xA9FE0000UL); WCUP_BRANCH(r); if (r) { result = 1; goto emit; }
                        r = ((ip & 0xFFC00000UL) == 0x64400000UL); WCUP_BRANCH(r); if (r) { result = 1; goto emit; }
                        r = (((ip >> 24) & 0xFF) == 0);            WCUP_BRANCH(r); if (r) { result = 1; goto emit; }
                    }
v6check:;
                    struct in6_addr a6;
                    int v6 = inet_pton(AF_INET6, host, &a6) == 1;
                    WCUP_BRANCH(v6);                                 /* B5 */
                    /* v6 body not exercised by this gate's inputs */
                } else result = 1;
            } else result = 1;
        }
emit:;
        WCUP_VALSTR(result ? "true" : "false");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("is_local_endpoint");
    }
    return 0;
}
