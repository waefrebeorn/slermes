/* gate: blueprint_catalog_resolve_schedule — traced copy of
 * src/hermes_cli/blueprint_catalog_helpers.c::blueprint_catalog_resolve_schedule.
 * LIVE twin: cron/blueprint_catalog.py::_resolve_schedule.
 *
 * Rung order (must match twin.py EXACTLY):
 *   B1  free-text "schedule" value passes through verbatim
 *   B2  template needs {minute}/{hour}
 *   B3    time value missing/empty -> error
 *   B4    time invalid (HH:MM range) -> error
 *   B5  template has {dow}
 *   B6    recurrence preset given
 *   B7      preset known -> else error
 *   B8    else day name given
 *   B9      day known -> else error
 *   B10 {interval_min} present and must be a positive integer
 *   L   per remaining {slot} placeholder found in template (in order)
 *   B     not already filled / present in values as a string
 *   L   substitution pass, one tick per replacement pair
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

static const char *DAYS[] = {"sunday","monday","tuesday","wednesday","thursday","friday","saturday",NULL};
static const char *DOWS[] = {"0","1","2","3","4","5","6"};

static void lower(const char *src, char *dst, size_t cap) {
    size_t n = 0;
    for (const char *p = src; *p && n + 1 < cap; p++)
        dst[n++] = (*p >= 'A' && *p <= 'Z') ? (char)(*p + 32) : *p;
    dst[n] = '\0';
}

/* Minimal flat-JSON-object extractor: fills pairs[][0]=key, pairs[][1]=value
 * (string values only — the same values the real port reads from the object). */
static int wc_extract_values(const char *json, char vals[][2][64], int max) {
    for (int i = 0; i < max; i++) { vals[i][0][0] = '\0'; vals[i][1][0] = '\0'; }
    if (!json) return 0;
    const char *p = json;
    int n = 0;
    while ((p = strchr(p, '"')) != NULL && n < max) {
        const char *k0 = p + 1;
        const char *k1 = strchr(k0, '"');
        if (!k1) break;
        const char *c = strchr(k1 + 1, ':');
        if (!c) break;
        c++;
        while (*c == ' ') c++;
        if (*c != '"') { p = k1 + 1; continue; }
        const char *v0 = c + 1;
        const char *v1 = strchr(v0, '"');
        if (!v1) break;
        size_t kl = (size_t)(k1 - k0), vl = (size_t)(v1 - v0);
        if (kl < 64 && vl < 64) {
            memcpy(vals[n][0], k0, kl); vals[n][0][kl] = '\0';
            memcpy(vals[n][1], v0, vl); vals[n][1][vl] = '\0';
            n++;
        }
        p = v1 + 1;
    }
    return n;
}

static void resolve_schedule(const char *sched, const char *values_json) {
    WCUP_BEGIN("resolve_schedule");
    char result[4096];
    int failed = 0;
    char vals[16][2][64];
    int nvals = wc_extract_values(values_json, vals, 16);

    const char *sched_v = NULL;
    for (int i = 0; i < nvals; i++)
        if (strcmp(vals[i][0], "schedule") == 0) sched_v = vals[i][1];

    if (sched_v && sched_v[0]) {                 /* B1 passthrough */
        WCUP_BRANCH(1);
        snprintf(result, sizeof result, "%s", sched_v);
        goto done;
    }
    WCUP_BRANCH(0);

    {
        char repl[16][2][64];
        int nrepl = 0;
#define ADD_REPL(k, v) do { if (nrepl < 16) { snprintf(repl[nrepl][0], 64, "%s", k); \
        snprintf(repl[nrepl][1], 64, "%s", v); nrepl++; } } while(0)

        if (strstr(sched, "{minute}") || strstr(sched, "{hour}")) {   /* B2 */
            WCUP_BRANCH(1);
            const char *tstr = NULL;
            for (int i = 0; i < nvals; i++)
                if (strcmp(vals[i][0], "time") == 0) tstr = vals[i][1];
            if (!tstr || !tstr[0]) { WCUP_BRANCH(1); failed = 1; goto done; }  /* B3 */
            WCUP_BRANCH(0);
            int hh = -1, mm = -1;
            char extra[8] = "";
            /* strict HH:MM (mirrors sscanf %d:%d + range check on the port) */
            if (sscanf(tstr, "%d:%d%1s", &hh, &mm, extra) != 2 ||
                hh < 0 || hh > 23 || mm < 0 || mm > 59) {
                WCUP_BRANCH(1); failed = 1; goto done;   /* B4 */
            }
            WCUP_BRANCH(0);
            char bh[8], bm[8];
            snprintf(bh, sizeof bh, "%d", hh);
            snprintf(bm, sizeof bm, "%d", mm);
            ADD_REPL("hour", bh); ADD_REPL("minute", bm);
        } else {
            WCUP_BRANCH(0);
        }

        if (strstr(sched, "{dow}")) {                                   /* B5 */
            WCUP_BRANCH(1);
            const char *preset = NULL, *dayname = NULL;
            for (int i = 0; i < nvals; i++) {
                if (strcmp(vals[i][0], "recurrence") == 0) preset = vals[i][1];
                if (strcmp(vals[i][0], "day") == 0) dayname = vals[i][1];
            }
            const char *dow = "*";
            if (preset) {                                               /* B6 */
                WCUP_BRANCH(1);
                char low[32]; lower(preset, low, sizeof low);
                if (strcmp(low, "everyday") == 0) dow = "*";
                else if (strcmp(low, "weekdays") == 0) dow = "1-5";
                else if (strcmp(low, "weekends") == 0) dow = "0,6";
                else { WCUP_BRANCH(0); failed = 1; goto done; }         /* B7 */
                WCUP_BRANCH(1);
            } else {
                WCUP_BRANCH(0);
                if (dayname) {                                          /* B8 */
                    WCUP_BRANCH(1);
                    char low[32]; lower(dayname, low, sizeof low);
                    int found = -1;
                    for (int i = 0; DAYS[i]; i++)
                        if (strcmp(low, DAYS[i]) == 0) { found = i; break; }
                    if (found < 0) { WCUP_BRANCH(0); failed = 1; goto done; }  /* B9 */
                    WCUP_BRANCH(1);
                    dow = DOWS[found];
                } else {
                    WCUP_BRANCH(0);
                }
            }
            ADD_REPL("dow", dow);
        } else {
            WCUP_BRANCH(0);
        }

        if (strstr(sched, "{interval_min}")) {                          /* B10 */
            WCUP_BRANCH(1);
            const char *ivs = "";
            for (int i = 0; i < nvals; i++)
                if (strcmp(vals[i][0], "interval_min") == 0) ivs = vals[i][1];
            char *end = NULL;
            long v = strtol(ivs, &end, 10);
            if (!ivs[0] || *end != '\0' || v <= 0) { WCUP_BRANCH(0); failed = 1; goto done; }
            WCUP_BRANCH(1);
            char bv[16]; snprintf(bv, sizeof bv, "%ld", v);
            ADD_REPL("interval_min", bv);
        } else {
            WCUP_BRANCH(0);
        }

        /* remaining {slot} placeholders, left-to-right */
        const char *p = sched;
        while ((p = strchr(p, '{')) != NULL) {
            const char *q = strchr(p, '}');
            if (!q) break;
            char name[64];
            size_t nl = (size_t)(q - p - 1);
            if (nl >= sizeof name || nl == 0) { p = q + 1; continue; }
            memcpy(name, p + 1, nl); name[nl] = '\0';
            WCUP_LOOP_TICK();
            int found = 0;
            for (int i = 0; i < nrepl; i++)
                if (strcmp(repl[i][0], name) == 0) { found = 1; break; }
            if (!found) {                                              /* B fill-needed */
                WCUP_BRANCH(1);
                for (int i = 0; i < nvals; i++)
                    if (strcmp(vals[i][0], name) == 0) {
                        WCUP_BRANCH(1);
                        ADD_REPL(name, vals[i][1]);
                        break;
                    }
            } else {
                WCUP_BRANCH(0);
            }
            p = q + 1;
        }

        snprintf(result, sizeof result, "%s", sched);
        for (int i = 0; i < nrepl; i++) {
            WCUP_LOOP_TICK();
            char tok[80];
            snprintf(tok, sizeof tok, "{%s}", repl[i][0]);
            char *pos = result;
            while ((pos = strstr(pos, tok)) != NULL) {
                size_t tlen = strlen(tok), vlen = strlen(repl[i][1]);
                memmove(pos + vlen, pos + tlen, strlen(pos + tlen) + 1);
                memcpy(pos, repl[i][1], vlen);
                pos += vlen;
            }
        }
    }

done:
    WCUP_VALSTR(failed ? "ERROR" : result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("resolve_schedule");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s resolve_schedule <tmpl|values>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[4096];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        char *pipe = strchr(buf, '|');
        if (pipe) { *pipe = '\0'; resolve_schedule(buf, pipe + 1); }
        else resolve_schedule(buf, "{}");
    }
    return 0;
}
