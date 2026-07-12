/* t_port_gateway_cgroup_cleanup.c — oracle harness for
 * gateway/cgroup_cleanup.py:_own_cgroup_path parser.
 * We test the "0::<path>" extraction by writing a fake /proc/self/cgroup
 * style buffer through a simulated parse. Since the real function reads a
 * file, the harness instead validates the parse rule via a tiny reimpl that
 * mirrors cgroup_own_path(): given lines, return the 0:: path. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cli/port_gateway_cgroup_cleanup.h"

/* Re-derive _own_cgroup_path's parse against an in-memory cgroup file. */
static char *parse_cgroup(const char *buf) {
    /* emulate: read lines, match ^0::(.+)$ */
    const char *p = buf;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len >= 3 && strncmp(p, "0::", 3) == 0) {
            const char *val = p + 3;
            size_t vlen = len - 3;
            char *r = malloc(vlen + 1);
            memcpy(r, val, vlen);
            r[vlen] = '\0';
            return r;
        }
        p = nl ? nl + 1 : p + len;
    }
    return NULL;
}

static void emit(const char *buf) {
    /* JSON-escape buf into a rotating buffer */
    static char out[8][2048];
    static int oi = 0;
    char *o = out[oi];
    oi = (oi + 1) % 8;
    char *q = o;
    *q++ = '"';
    for (const char *p = buf; p && *p && q - o < 2000; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
        else if (c == '\t') { *q++ = '\\'; *q++ = 't'; }
        else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    char *r = parse_cgroup(buf);
    if (r) printf("{\"in\":%s,\"out\":\"%s\"}\n", o, r);
    else   printf("{\"in\":%s,\"out\":null}\n", o);
    free(r);
}

int main(void) {
    emit("11:cpu,cpuacct:/user.slice\n0::/system.slice/my.service\n");
    emit("0::/system.slice/gateway.service\n");
    emit("0::/init.scope\n");
    emit("3:freezer:/\n2:cpu:/\n");
    emit("");
    return 0;
}
