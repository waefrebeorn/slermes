/* t_port_readiness.c — oracle harness for gateway/readiness.py pure probes. */
#include "port_readiness.h"

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

    /* state_db: (exists, corrupt, error_name) */
    struct { int e, c; const char *err; } sdb[] = {
        {0,0,NULL},{1,0,NULL},{1,1,NULL},{1,0,"corrupt"},{1,0,"OperationalError"},
        {1,0,""}, {0,1,"corrupt"},
    };
    for (size_t i = 0; i < sizeof(sdb)/sizeof(sdb[0]); i++) {
        char *out = rd_probe_state_db(sdb[i].e, sdb[i].c, sdb[i].err);
        printf("{\"family\":\"sdb\",\"case\":%zu,\"exists\":%d,\"corrupt\":%d,"
               "\"error\":", i, sdb[i].e, sdb[i].c);
        jstr(sdb[i].err);
        printf(",\"out\":%s}\n", out);
        free(out);
    }

    /* config: (exists, backend_unavail, exc_name, raw_is_mapping) */
    struct { int e, b; const char *exc; int m; } cfg[] = {
        {0,0,NULL,1},{1,0,NULL,1},{1,1,NULL,1},{1,0,"TypeError",1},
        {1,0,"ConfigBackendUnavailable",0},{1,0,NULL,0},
    };
    for (size_t i = 0; i < sizeof(cfg)/sizeof(cfg[0]); i++) {
        char *out = rd_probe_config(cfg[i].e, cfg[i].b, cfg[i].exc, cfg[i].m);
        printf("{\"family\":\"cfg\",\"case\":%zu,\"exists\":%d,\"backend\":%d,"
               "\"exc\":", i, cfg[i].e, cfg[i].b);
        jstr(cfg[i].exc);
        printf(",\"mapping\":%d,\"out\":%s}\n", cfg[i].m, out);
        free(out);
    }

    /* disk: (used_percent, free_bytes, error_name) */
    struct { double u; long long f; const char *err; } dsk[] = {
        {90.0, 1, NULL},{89.3, 2048,NULL},{0.0, 1024*1024,NULL},{42.5, 8, NULL},
        {12.0, 64, "PermissionError"},
    };
    for (size_t i = 0; i < sizeof(dsk)/sizeof(dsk[0]); i++) {
        char *out = rd_probe_disk(dsk[i].u, dsk[i].f, dsk[i].err);
        printf("{\"family\":\"dsk\",\"case\":%zu,\"used\":%.1f,\"free\":%lld,"
               "\"error\":", i, dsk[i].u, (long long)dsk[i].f);
        jstr(dsk[i].err);
        printf(",\"out\":%s}\n", out);
        free(out);
    }

    /* gateway: (gateway_state, platforms_json) */
    struct { const char *st; const char *pj; } gw[] = {
        {"running","{\"telegram\":{\"state\":\"connected\"},\"discord\":{\"state\":\"OK\"}}"},
        {"draining","{\"telegram\":{\"state\":\"disconnected\"}}"},
        {"stopped","{\"slack\":{\"status\":\"running\"},\"webhook\":{}}"},
        {"unknown",NULL},
        {"running","[]"},
        {"ok","{\"a\":{\"state\":\"connected\"},\"b\":{\"state\":\"unknown\"},\"c\":42}"},
    };
    for (size_t i = 0; i < sizeof(gw)/sizeof(gw[0]); i++) {
        char *out = rd_probe_gateway(gw[i].st, gw[i].pj);
        printf("{\"family\":\"gw\",\"case\":%zu,\"state\":", i);
        jstr(gw[i].st);
        printf(",\"platforms\":");
        jstr(gw[i].pj);
        printf(",\"out\":%s}\n", out);
        free(out);
    }

    /* session_store: (db_detail, db_status, store_state) */
    struct { const char *d, *s, *st; } ss[] = {
        {"corrupt","degraded","ok"},{"", "ok","ok"},{"","ok","unavailable"},
        {"", "ok","retrying"},{"","ok","unknown"},{"","degraded","unknown"},
        {"","",""},{"corrupt","","running"},
    };
    for (size_t i = 0; i < sizeof(ss)/sizeof(ss[0]); i++) {
        char *out = rd_probe_session_store(ss[i].d, ss[i].s, ss[i].st);
        printf("{\"family\":\"ss\",\"case\":%zu,\"db_detail\":", i);
        jstr(ss[i].d);
        printf(",\"db_status\":");
        jstr(ss[i].s);
        printf(",\"store\":");
        jstr(ss[i].st);
        printf(",\"out\":%s}\n", out);
        free(out);
    }

    /* overall: (statuses array) */
    static const char *const ov[] = {
        "[]",
        "[{\"status\":\"ok\"},{\"status\":\"ok\"}]",
        "[{\"status\":\"ok\"},{\"status\":\"degraded\"}]",
        "[{\"status\":\"degraded\"}]",
        "[{\"status\":\"ok\"},\"junk\"]",
    };
    for (size_t i = 0; i < sizeof(ov)/sizeof(ov[0]); i++) {
        char *out = rd_overall(ov[i]);
        printf("{\"family\":\"ov\",\"case\":%zu,\"arr\":%s,\"out\":", i, ov[i]);
        jstr(out);
        printf("}\n");
        free(out);
    }
    return 0;
}
