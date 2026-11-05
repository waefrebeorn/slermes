/* t_port_turn_lease.c — oracle harness for gateway/turn_lease.py formatting
 * helpers: _holder_desc, TurnLeaseToken.__repr__, TurnLeaseTimeoutError msg. */
#include "port_turn_lease.h"

#include <stdio.h>
#include <stdlib.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* _holder_desc: (owner_key, generation, present). */
    struct { const char *k; long long g; int p; } h[] = {
        {"user;home", 4, 1}, {"", 0, 1}, {"k", 1, 1}, {NULL, 0, 0},
    };
    for (int i = 0; i < 4; i++) {
        char *out = tls_holder_desc(h[i].k, h[i].g, h[i].p);
        printf("{\"family\":\"holder\",\"case\":%d,\"key\":", i);
        jstr(h[i].k);
        printf(",\"gen\":%lld,\"present\":%d,\"out\":%s}\n",
               h[i].g, h[i].p, out ? out : "null");
        free(out);
    }

    /* __repr__: (session_id, owner_key, generation, released). */
    struct { const char *s; const char *k; long long g; int r; } rp[] = {
        {"20261105_abc", "user;home", 7, 0},
        {"s2", "k'x", 0, 1},
        {"x\ny", "k", -3, 0},
    };
    for (int i = 0; i < 3; i++) {
        char *out = tls_token_repr(rp[i].s, rp[i].k, rp[i].g, rp[i].r);
        printf("{\"family\":\"repr\",\"case\":%d,\"sid\":", i);
        jstr(rp[i].s);
        printf(",\"key\":");
        jstr(rp[i].k);
        printf(",\"gen\":%lld,\"rel\":%d,\"out\":", rp[i].g, rp[i].r);
        jstr(out);
        printf("}\n");
        free(out);
    }

    /* timeout message: (session_id, owner_key, generation, wait). */
    struct { const char *s; const char *k; long long g; double w; } tm[] = {
        {"sess1", "rk1", 2, 5.0},
        {"s", "k", 0, 2.5},
        {"s", "k", 9, 3.5},
        {"s", "k", 1, 0.0},
    };
    for (int i = 0; i < 4; i++) {
        char *out = tls_timeout_msg(tm[i].s, tm[i].k, tm[i].g, tm[i].w);
        printf("{\"family\":\"tout\",\"case\":%d,\"sid\":", i);
        jstr(tm[i].s);
        printf(",\"key\":");
        jstr(tm[i].k);
        printf(",\"gen\":%lld,\"wait\":%.1f,\"out\":", tm[i].g, tm[i].w);
        jstr(out);
        printf("}\n");
        free(out);
    }
    return 0;
}
