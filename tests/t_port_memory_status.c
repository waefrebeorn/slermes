/* t_port_memory_status.c — Oracle harness for the /api/status memory rollup. */
#include "port_memory_status.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOW 1793880000.0 /* 2026-11-05T12:00:00+00:00 */

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

    static const char *const vals[] = { "0", "1", "65536", "1048576",
                                         "131072", "-1", "1.5", "null",
                                         "true", "\"5\"" };
    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
        json_t *v = json_parse(vals[i], NULL);
        printf("{\"family\":\"nonneg\",\"case\":\"n_%zu\",\"in\":%s,\"ret\":%lld,\"mb\":%lld}\n",
               i, vals[i], (long long)ms_nonneg_int(v), (long long)ms_mb(v));
        json_free(v);
    }

    static const struct { const char *a, *t; } pre[] = {
        { "null", "null" }, { "null", "8000000" }, { "0", "8000000" },
        { "50000", "8000000" }, { "70000", "8000000" }, { "70000", "200000" },
        { "131072", "100000" }, { "200000", "4000000" }, { "-5", "4000000" },
        { "100000", "null" }, { "500", "1000000" }, { "100000", "0" },
    };
    for (size_t i = 0; i < sizeof(pre) / sizeof(pre[0]); i++) {
        json_t *a = json_parse(pre[i].a, NULL), *t = json_parse(pre[i].t, NULL);
        printf("{\"family\":\"classify\",\"case\":\"c_%zu\",\"a\":%s,\"t\":%s,\"out\":",
               i, pre[i].a, pre[i].t);
        jstr(ms_classify_pressure(a, t));
        printf("}\n");
        json_free(a); json_free(t);
    }

    static const char *const tss[] = {
        "2026-11-05T12:00:00+00:00", "2026-11-05T11:59:50", 
        "2026-11-05T12:00:00Z", "2026-11-05T10:30:15.500000+00:00",
        "2026-11-05T12:00:00", "2026-11-05", "2026-13-05T12:00:00",
        "bogus", "", "2026-11-05T99:00:00+00:00",
        "1999-01-01T00:00:00+00:00",
    };
    for (size_t i = 0; i < sizeof(tss) / sizeof(tss[0]); i++) {
        printf("{\"family\":\"parse\",\"case\":\"p_%zu\",\"in\":", i);
        jstr(tss[i]);
        printf(",\"epoch\":%.17g}\n", ms_parse_iso_epoch(tss[i]));
    }

    static const struct { const char *hb, *sn; } colls[] = {
        /* fresh, low avail -> critical */
        { "{\"updated_at\":\"2026-11-05T11:59:50\",\"mem\":{\"rss_kib\":102400,\"mem_total_kib\":16777216,\"mem_available_kib\":50000,\"swap_used_kib\":2048}}", NULL },
        /* fresh, high avail -> ok */
        { "{\"updated_at\":\"2026-11-05T12:00:00+00:00\",\"mem\":{\"rss_kib\":4096,\"mem_total_kib\":16777216,\"mem_available_kib\":8388608,\"swap_used_kib\":0}}", NULL },
        /* stale -> sampled_at kept, pressure unknown */
        { "{\"updated_at\":\"2026-11-05T10:00:00+00:00\",\"mem\":{\"rss_kib\":102400,\"mem_total_kib\":16777216,\"mem_available_kib\":0,\"swap_used_kib\":100}}", NULL },
        /* unreadable heartbeat */
        { NULL, NULL },
        /* mem missing entirely */
        { "{\"updated_at\":\"2026-11-05T12:00:00+00:00\"}", NULL },
        /* empty heartbeat str mapped to no file */
        { "{}", NULL },
        /* with sentinel */
        { "{\"updated_at\":\"2026-11-05T12:00:00Z\",\"mem\":{\"rss_kib\":4096,\"mem_total_kib\":16777216,\"mem_available_kib\":8388608,\"swap_used_kib\":0}}",
          "{\"prior_unclean_exit\":true,\"prior_suspected_oom\":\"x\",\"started_at\":\"2026-11-01T00:00:00\"}" },
        /* sentinel with non-string started_at + falsy flags */
        { NULL, "{\"prior_unclean_exit\":0,\"prior_suspected_oom\":false,\"started_at\":7}" },
        /* empty sentinel */
        { NULL, "{}" },
        /* sentinel truthy via numeric 1 */
        { NULL, "{\"prior_unclean_exit\":1,\"prior_suspected_oom\":0,\"started_at\":\"\"}" },
    };
    for (size_t i = 0; i < sizeof(colls) / sizeof(colls[0]); i++) {
        json_t *st = ms_collect_memory_status(colls[i].hb, colls[i].sn, NOW);
        printf("{\"family\":\"collect\",\"case\":\"col_%zu\",\"out\":", i);
        if (!st) {
            fputs("null", stdout);
        } else {
            char *s = json_serialize(st);
            fputs(s ? s : "null", stdout);
            free(s);
            json_free(st);
        }
        printf("}\n");
    }
    return 0;
}
