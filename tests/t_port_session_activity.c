/* t_port_session_activity.c — Oracle harness for the activity-stamp core. */
#include "port_session_activity.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        default:   fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    static const char *const provs[] = {
        "agent.compression_timeout", "agent.compression_cooldown",
        "agent.compression_turnhold", "agent.compression",
        "unknown", "", "  agent.compression_cooldown  ", "BOGUS", NULL,
        "agent.compression.turnhold", "AGENT.COMPRESSION_COOLDOWN",
    };
    for (size_t i = 0; i < sizeof(provs) / sizeof(provs[0]); i++) {
        char n[64];
        snprintf(n, sizeof(n), "is_term_%zu", i);
        printf("{\"family\":\"is_terminal\",\"case\":\"%s\",\"ret\":%d}\n", n,
               sa_is_terminal_compression_provenance(provs[i]));
    }
    for (size_t i = 0; i < sizeof(provs) / sizeof(provs[0]); i++) {
        printf("{\"family\":\"normalize\",\"case\":\"norm_%zu\",\"out\":", i);
        char *o = sa_normalize_activity_provenance(provs[i]);
        jstr(o);
        printf("}\n");
        free(o);
    }

    static const char *const descs[] = {
        NULL, "", "   ", "hello world",
        "a very long activity description that comfortably exceeds the one hundred and twenty character budget that the module reserves for the persistent session viewer column so it must be clamped and ellipsized exactly",
    };
    for (size_t i = 0; i < sizeof(descs) / sizeof(descs[0]); i++) {
        printf("{\"family\":\"bound\",\"case\":\"b_%zu\",\"desc\":", i);
        jstr(descs[i]);
        printf(",\"out\":");
        char *o = sa_bound_activity_description(descs[i]);
        jstr(o);
        printf("}\n");
        free(o);
    }

    static const struct { const char *n, *max; } iter[] = {
        { "3", "15" }, { "0", "15" }, { "3", NULL }, { "3", "" },
        { "3", "9223372036854775807" }, { "3", "3.5" }, { "3", "015" },
        { "3", "abc" }, { "-1", "5" }, { "12", "0" },
    };
    for (size_t i = 0; i < sizeof(iter) / sizeof(iter[0]); i++) {
        printf("{\"family\":\"iter\",\"case\":\"it_%zu\",\"out\":", i);
        char *o = sa_format_iteration_progress(iter[i].n, iter[i].max);
        jstr(o);
        printf("}\n");
        free(o);
    }

    static const struct { const char *at, *desc, *prov; double now; const char *extra; } snaps[] = {
        { NULL, "hello", "agent.compression", 100.0, NULL },
        { "50.0", "work", "agent.compression_timeout", 100.3, NULL },
        { NULL, NULL, NULL, 0.0, NULL },
        { "12.5", "  padded  ", "bogus", 22.2, "{\"note\":\"hi\",\"n\":7,\"ok\":true}" },
        { "9.1", "", "", 19.0, "{}" },
    };
    for (size_t i = 0; i < sizeof(snaps) / sizeof(snaps[0]); i++) {
        json_t *snap = sa_build_activity_snapshot(snaps[i].at, snaps[i].desc,
                                                  snaps[i].prov, snaps[i].now,
                                                  snaps[i].extra,
                                                  snaps[i].extra != NULL);
        printf("{\"family\":\"snapshot\",\"case\":\"sn_%zu\",\"out\":", i);
        if (!snap) {
            fputs("null", stdout);
        } else {
            char *s = json_serialize(snap);
            fputs(s ? s : "null", stdout);
            free(s);
            json_free(snap);
        }
        printf("}\n");
    }

    /* AwakeIdleMeter measure: anchors at wall=1000 mono=2000. */
    {
        sa_idle_meter_t m;
        sa_idle_meter_init(&m, 1000.0, 2000.0);
        static const double script[][3] = { /* idle, wall, mono */
            { 50.0, 1005.0, 2001.0 },
            { 30.0, 1010.0, 2010.0 },
            { 2.0,  1020.0, 2015.0 },
            { 60.0, 1030.0, 2020.0 },
            { 5.0,  1040.0, 2021.0 },
        };
        for (size_t i = 0; i < sizeof(script) / sizeof(script[0]); i++) {
            double r = sa_measure_idle(&m, script[i][0], script[i][1], script[i][2]);
            printf("{\"family\":\"measure\",\"case\":\"ms_%zu\",\"ret\":%.9g}\n", i, r);
        }
    }
    return 0;
}
