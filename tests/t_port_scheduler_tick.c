/* t_port_scheduler_tick.c — oracle harness for scheduler_tick pure core:
 * is_recurring, _advance_or_drop_recurring, _log_tick_yield_once. */
#include "port_scheduler_tick.h"

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
        case '\n': fputs("\\n", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* --- is_recurring --- */
    {
        static const char *const jobs[] = {
            "{\"id\":\"j1\",\"schedule\":{\"kind\":\"cron\",\"expr\":\"* * * * *\"}}",
            "{\"id\":\"j2\",\"schedule\":{\"kind\":\"interval\"}}",
            "{\"id\":\"j3\",\"schedule\":{}}",
            "{\"id\":\"j4\"}",
            "{\"id\":\"j5\",\"schedule\":{\"kind\":\"once\"}}",
            "{\"id\":\"j6\",\"schedule\":null}",
        };
        for (int i = 0; i < 6; i++) {
            int r = js_is_recurring(jobs[i]);
            printf("{\"family\":\"recur\",\"case\":%d,\"job\":%s,\"out\":%d}\n",
                   i, jobs[i], r);
        }
    }

    /* --- _advance_or_drop_recurring --- */
    {
        const char *due = "[{\"id\":\"a\",\"schedule\":{\"kind\":\"cron\"}},"
                          "{\"id\":\"b\"},"
                          "{\"id\":\"c\",\"schedule\":{\"kind\":\"interval\"}},"
                          "{\"id\":\"d\",\"schedule\":{\"kind\":\"once\"}}]";
        char *o1 = js_advance_or_drop(due, 1);
        char *o0 = js_advance_or_drop(due, 0);
        printf("{\"family\":\"aod\",\"case\":0,\"due\":%s,\"advance_ok\":1,\"out\":%s}\n", due, o1);
        printf("{\"family\":\"aod\",\"case\":1,\"due\":%s,\"advance_ok\":0,\"out\":%s}\n", due, o0);
        free(o1);
        free(o0);
    }

    /* --- _log_tick_yield_once decision --- */
    {
        struct { const char *lr; double la; const char *r; double now; } cases[] = {
            {"boot=1", 100.0, "boot=1", 200.0},   /* same, <3600 -> false */
            {"boot=1", 100.0, "boot=2", 150.0},   /* diff reason -> true */
            {"boot=1", 100.0, "boot=1", 3700.0},  /* same, exactly 3600 -> true */
            {NULL, 0.0, "boot=3", 50.0},          /* prev empty -> true */
            {"boot=9", 5000.0, "boot=9", 7599.0}, /* 2599 -> false */
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            int out = js_tick_yield_should_log(cases[i].lr, cases[i].la,
                                               cases[i].r, cases[i].now);
            printf("{\"family\":\"gate\",\"case\":%zu,\"lr\":", i);
            jstr(cases[i].lr);
            printf(",\"la\":%.17g,\"r\":", cases[i].la);
            jstr(cases[i].r);
            printf(",\"now\":%.17g,\"out\":%s}\n", cases[i].now, out ? "true" : "false");
        }
    }

    /* --- _log_tick_yield_once message --- */
    {
        static const char *const reasons[] = {
            "boot=a/b", "boot=2", "", "boot=1 disk=4",
        };
        for (size_t i = 0; i < sizeof(reasons) / sizeof(reasons[0]); i++) {
            char *out = js_tick_yield_message(reasons[i]);
            printf("{\"family\":\"msg\",\"case\":%zu,\"r\":", i);
            jstr(reasons[i]);
            printf(",\"out\":");
            jstr(out);
            printf("}\n");
            free(out);
        }
    }
    return 0;
}
