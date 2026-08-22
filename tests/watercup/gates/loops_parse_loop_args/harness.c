/* gate: loops_parse_loop_args — traced C copy of
 * src/hermes_cli/loops_pure.c::loops_parse_loop_args (LIVE twin:
 * hermes_cli/loops.py::parse_loop_args).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1:  raw empty after strip            -> {"error":"empty"}
 *   B2:  --times match present
 *   B3:    times value invalid (<1 / non-int) -> --times error return
 *   B4:  --until match present
 *   B5:  leading "every" sugar present
 *   B6:  leading interval token parses
 *   B7:  prompt empty after extraction    -> missing-prompt error
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <regex.h>
#include "watercup.h"

/* Faithful port of LIVE parse_interval_token:
 * ^(?!\d*$)(?=(\d|\s))(?:(\d+)h)?(?:(\d+)m)?(?:(\d+)s)?$ (case-insensitive)
 * — at least one unit suffix must be present; total>0 else None. */
static long wc_parse_interval_token(const char *token, int *ok) {
    *ok = 0;
    if (!token || !*token) return -1;
    long h = 0, mn = 0, sc = 0;
    int has_h = 0, has_m = 0, has_s = 0;
    const char *p = token;
    if (!isdigit((unsigned char)*p)) return -1;   /* lookahead \d */
    /* (?:(\d+)h)? */
    if (isdigit((unsigned char)*p)) {
        long v = 0;
        const char *q = p;
        while (isdigit((unsigned char)*q)) { v = v * 10 + (*q - '0'); q++; }
        if (*q == 'h' || *q == 'H') { h = v; has_h = 1; p = q + 1; }
        else {
            /* could be the minutes or seconds group instead */
            long v2 = 0;
            const char *r = p;
            while (isdigit((unsigned char)*r)) { v2 = v2 * 10 + (*r - '0'); r++; }
            if ((*r == 'm' || *r == 'M')) { mn = v2; has_m = 1; p = r + 1; }
            else if ((*r == 's' || *r == 'S')) { sc = v2; has_s = 1; p = r + 1; }
            else return -1;  /* trailing junk */
        }
    }
    /* (?:(\d+)m)? */
    if (!has_m && isdigit((unsigned char)*p)) {
        long v = 0;
        const char *q = p;
        while (isdigit((unsigned char)*q)) { v = v * 10 + (*q - '0'); q++; }
        if (*q == 'm' || *q == 'M') { mn = v; has_m = 1; p = q + 1; }
    }
    /* (?:(\d+)s)? */
    if (!has_s && isdigit((unsigned char)*p)) {
        long v = 0;
        const char *q = p;
        while (isdigit((unsigned char)*q)) { v = v * 10 + (*q - '0'); q++; }
        if (*q == 's' || *q == 'S') { sc = v; has_s = 1; p = q + 1; }
    }
    if (*p != '\0') return -1;                    /* $ anchor */
    if (!has_h && !has_m && !has_s) return -1;    /* bare digits: no unit */
    long total = h * 3600 + mn * 60 + sc;
    if (total <= 0) return -1;
    *ok = 1;
    return total;
}

static char *loops_parse_loop_args(const char *text, char *out, size_t outsz) {
    WCUP_BEGIN("loops_parse_loop_args");
    char raw[4096];
    snprintf(raw, sizeof raw, "%s", text ? text : "");
    size_t rl = strlen(raw);
    while (rl > 0 && isspace((unsigned char)raw[rl-1])) raw[--rl] = '\0';
    char *rp = raw;
    while (*rp && isspace((unsigned char)*rp)) rp++;
    memmove(raw, rp, strlen(rp) + 1);

    long times = 0;
    char until[2048]; until[0] = '\0';
    long interval = -1;

    /* B1: empty */
    if (raw[0] == '\0') {
        WCUP_BRANCH(1);
        snprintf(out, outsz,
            "{\"interval_seconds\":null,\"prompt\":\"\",\"times\":0,"
            "\"until\":\"\",\"error\":\"empty\"}");
        goto done_nofree;
    }
    WCUP_BRANCH(0);

    /* B2/B3: --times */
    {
        regex_t rt;
        if (regcomp(&rt, "[[:space:]]--times[[:space:]]+([^[:space:]]+)", REG_EXTENDED) == 0) {
            regmatch_t m[2];
            if (regexec(&rt, raw, 2, m, 0) == 0) {
                WCUP_BRANCH(1);
                char tv[64];
                size_t len = (size_t)(m[1].rm_eo - m[1].rm_so);
                if (len >= sizeof tv) len = sizeof tv - 1;
                memcpy(tv, raw + m[1].rm_so, len); tv[len] = '\0';
                char *end; long tvv = strtol(tv, &end, 10);
                if (*end != '\0' || tvv < 1) {
                    WCUP_BRANCH(1);
                    regfree(&rt);
                    snprintf(out, outsz,
                        "{\"interval_seconds\":null,\"prompt\":\"\",\"times\":0,"
                        "\"until\":\"\",\"error\":\"--times expects a positive integer\"}");
                    goto done_nofree;
                }
                WCUP_BRANCH(0);
                times = tvv;
                size_t pre = (size_t)m[0].rm_so, post = (size_t)m[0].rm_eo;
                memmove(raw + pre, raw + post, strlen(raw + post) + 1);
                rl = strlen(raw);
                while (rl > 0 && isspace((unsigned char)raw[rl-1])) raw[--rl] = '\0';
            } else {
                WCUP_BRANCH(0);
            }
            regfree(&rt);
        }
    }

    /* B4: --until */
    {
        regex_t ru;
        if (regcomp(&ru, "[[:space:]]--until[[:space:]]+(.+)$", REG_EXTENDED) == 0) {
            regmatch_t m[2];
            if (regexec(&ru, raw, 2, m, 0) == 0) {
                WCUP_BRANCH(1);
                size_t len = (size_t)(m[1].rm_eo - m[1].rm_so);
                if (len >= sizeof until) len = sizeof until - 1;
                memcpy(until, raw + m[1].rm_so, len); until[len] = '\0';
                size_t ul = strlen(until);
                while (ul > 0 && isspace((unsigned char)until[ul-1])) until[--ul] = '\0';
                size_t pre = (size_t)m[0].rm_so, post = (size_t)m[0].rm_eo;
                memmove(raw + pre, raw + post, strlen(raw + post) + 1);
                rl = strlen(raw);
                while (rl > 0 && isspace((unsigned char)raw[rl-1])) raw[--rl] = '\0';
            } else {
                WCUP_BRANCH(0);
            }
            regfree(&ru);
        }
    }

    /* B5: "every" sugar */
    {
        size_t sp = 0;
        while (raw[sp] && !isspace((unsigned char)raw[sp])) sp++;
        int is_every = (sp == 5 && strncasecmp(raw, "every", 5) == 0 &&
                        raw[5] && isspace((unsigned char)raw[5]));
        if (is_every) {
            WCUP_BRANCH(1);
            char *rest = raw + 5;
            while (*rest && isspace((unsigned char)*rest)) rest++;
            memmove(raw, rest, strlen(rest) + 1);
        } else {
            WCUP_BRANCH(0);
        }
    }

    /* B6: leading interval token */
    char prompt[4096];
    prompt[0] = '\0';
    {
        size_t sp = 0;
        while (raw[sp] && !isspace((unsigned char)raw[sp])) sp++;
        char first[128];
        size_t fl = sp < sizeof first ? sp : sizeof first - 1;
        memcpy(first, raw, fl); first[fl] = '\0';
        int ok = 0;
        long v = wc_parse_interval_token(first, &ok);
        if (ok && v > 0) {
            WCUP_BRANCH(1);
            interval = v;
            char *rest = raw + sp;
            while (*rest && isspace((unsigned char)*rest)) rest++;
            snprintf(prompt, sizeof prompt, "%s", rest);
        } else {
            WCUP_BRANCH(0);
            snprintf(prompt, sizeof prompt, "%s", raw);
        }
    }

    /* B7: missing prompt */
    if (prompt[0] == '\0') {
        WCUP_BRANCH(1);
        snprintf(out, outsz,
            "{\"interval_seconds\":null,\"prompt\":\"\",\"times\":%ld,"
            "\"until\":\"\",\"error\":\"missing prompt (usage: /loop [interval] <prompt>)\"}",
            times);
        goto done_nofree;
    }
    WCUP_BRANCH(0);

    {
        char pe[8192], ue[2048];
        /* escape minimal JSON */
        size_t o = 0;
        for (const char *q = prompt; *q && o + 2 < sizeof pe; q++) {
            if (*q == '"' || *q == '\\') pe[o++] = '\\';
            pe[o++] = *q;
        }
        pe[o] = '\0';
        o = 0;
        for (const char *q = until; *q && o + 2 < sizeof ue; q++) {
            if (*q == '"' || *q == '\\') ue[o++] = '\\';
            ue[o++] = *q;
        }
        ue[o] = '\0';
        char iv[32];
        if (interval >= 0) snprintf(iv, sizeof iv, "%ld", interval);
        else strcpy(iv, "null");
        snprintf(out, outsz,
            "{\"interval_seconds\":%s,\"prompt\":\"%s\",\"times\":%ld,"
            "\"until\":\"%s\",\"error\":null}", iv, pe, times, ue);
    }

done_nofree:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("loops_parse_loop_args");
        return out;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s loops_parse_loop_args <text>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char out[16384];
        loops_parse_loop_args(argv[i], out, sizeof out);
    }
    return 0;
}
