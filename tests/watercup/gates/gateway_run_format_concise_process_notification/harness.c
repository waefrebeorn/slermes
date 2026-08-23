/* gate: gateway_run_format_concise_process_notification — traced copy of
 * src/gateway/run_gaps2.c::gw_format_concise_process_notification (+ its
 * inlined gw_shorten_command_for_display)
 * (LIVE twin: gateway/run.py::_format_concise_process_notification).
 *
 * argv input per cup: "exit_code|duration|command|output" where exit_code is
 * an int, "-" = None; duration "-" = None else seconds.
 *
 * Ladder:
 *   B1: ok (!has_exit_code || exit_code == 0)
 *   B2: short_cmd non-empty
 *   B3: has_duration && duration >= 0
 *     B4: secs >= 3600
 *     B5: secs >= 60
 *   B6: !ok and output has non-blank content
 *     B7: tail non-empty (after >500 truncation)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "watercup.h"

/* traced copy of gw_shorten_command_for_display */
static void shorten_command(const char *command, int limit,
                            char *out, size_t cap)
{
    if (!out || cap == 0) return;
    out[0] = '\0';
    if (!command) return;
    size_t w = 0;
    const char *p = command;
    bool in_ws = false;
    while (*p && w + 1 < cap) {
        unsigned char c = (unsigned char)*p;
        if (c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\v'||c=='\f') { in_ws = true; p++; continue; }
        if (in_ws && w > 0 && w + 1 < cap) out[w++] = ' ';
        in_ws = false;
        out[w++] = (char)c;
        p++;
    }
    out[w] = '\0';
    /* untraced: byte-level UTF-8 ellipsis truncation at `limit` chars */
    if ((int)strlen(out) > limit && limit > 0) {
        int keep = limit - 1;
        while (keep > 0 && ((unsigned char)out[keep] & 0xC0) == 0x80) keep--;
        out[keep] = '\0';
        strcat(out, "\xe2\x80\xa6");
    }
}

static void format_concise(int has_exit_code, long exit_code,
                           int has_duration, double duration_seconds,
                           const char *command, const char *output)
{
    WCUP_BEGIN("gw_format_concise_process_notification");
    bool ok = !has_exit_code || exit_code == 0;
    WCUP_BRANCH(ok);
    const char *icon = ok ? "\xe2\x9c\x85" : "\xe2\x9d\x8c";
    char verb[64];
    if (ok) snprintf(verb, sizeof verb, "finished");
    else snprintf(verb, sizeof verb, "failed (exit %ld)", exit_code);

    char short_cmd[512];
    shorten_command(command, 80, short_cmd, sizeof short_cmd);

    char text[8192];
    size_t n = (size_t)snprintf(text, sizeof text, "%s Background task %s", icon, verb);
    if (short_cmd[0]) {
        WCUP_BRANCH(1);
        n += (size_t)snprintf(text + n, sizeof text - n, " \xe2\x80\x94 `%s`", short_cmd);
    } else {
        WCUP_BRANCH(0);
    }
    if (has_duration && duration_seconds >= 0) {
        WCUP_BRANCH(1);
        long secs = (long)duration_seconds;
        char dur[32];
        if (secs >= 3600) { WCUP_BRANCH(1); snprintf(dur, sizeof dur, "%ldh %ldm", secs/3600, (secs%3600)/60); }
        else {
            WCUP_BRANCH(0);
            if (secs >= 60) { WCUP_BRANCH(1); snprintf(dur, sizeof dur, "%ldm %lds", secs/60, secs%60); }
            else { WCUP_BRANCH(0); snprintf(dur, sizeof dur, "%lds", secs); }
        }
        n += (size_t)snprintf(text + n, sizeof text - n, " (%s)", dur);
    } else {
        WCUP_BRANCH(0);
    }
    if (!ok && output && output[strspn(output, " \t\r\n")] != '\0') {
        WCUP_BRANCH(1);
        /* Faithful to LIVE: strip() whole output, split on '\n', drop
         * fully-blank lines, keep last 5 verbatim (leading spaces kept). */
        const char *b = output;
        while (*b==' '||*b=='\t'||*b=='\r'||*b=='\n') b++;
        const char *endp = output + strlen(output);
        while (endp > b && (endp[-1]==' '||endp[-1]=='\t'||endp[-1]=='\r'||endp[-1]=='\n')) endp--;
        const char *lines[6]; int lens[6]; int nl = 0;
        const char *p2 = b;
        while (p2 < endp) {
            const char *e2 = memchr(p2, '\n', (size_t)(endp - p2));
            if (!e2) e2 = endp;
            int allws = 1;
            for (const char *q = p2; q < e2; q++)
                if (!(*q==' '||*q=='\t'||*q=='\r')) { allws = 0; break; }
            if (!allws) { lines[nl % 5] = p2; lens[nl % 5] = (int)(e2 - p2); nl++; }
            p2 = (e2 < endp) ? e2 + 1 : endp;
        }
        int start = nl > 5 ? nl - 5 : 0;
        char tail[4096]; size_t t = 0;
        for (int i = start; i < nl && i < start + 5; i++) {
            int li = i % 5;
            if (t + (size_t)lens[li] + 2 < sizeof tail) {
                memcpy(tail + t, lines[li], (size_t)lens[li]); t += (size_t)lens[li];
                tail[t++] = '\n';
            }
        }
        if (t) tail[t-1] = '\0'; else tail[0] = '\0';
        if (strlen(tail) > 500) memmove(tail, tail + strlen(tail) - 500, 501);
        if (tail[0]) {
            WCUP_BRANCH(1);
            snprintf(text + n, sizeof text - n, "\n```\n%s\n```", tail);
        } else {
            WCUP_BRANCH(0);
        }
    } else {
        WCUP_BRANCH(0);
    }

    WCUP_VALSTR(text);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_format_concise_process_notification");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_format_concise_process_notification <exit|dur|cmd|output>...\n", argv[0]); return 2; }
    for (int a = 2; a < argc; a++) {
        static char buf[16384];
        snprintf(buf, sizeof buf, "%s", argv[a]);
        char *f1 = strchr(buf, '|');
        char *f2 = f1 ? strchr(f1+1, '|') : NULL;
        char *f3 = f2 ? strchr(f2+1, '|') : NULL;
        if (!f3) { fprintf(stderr, "bad input\n"); return 2; }
        *f1 = '\0'; *f2 = '\0'; *f3 = '\0';
        const char *ec_s = buf;      /* exit code field */
        const char *dur_s = f1 + 1;  /* duration field */
        int hec = strcmp(ec_s, "-") != 0;
        int hd = strcmp(dur_s, "-") != 0;
        format_concise(hec, hec ? atol(ec_s) : 0,
                       hd, hd ? atof(dur_s) : -1.0,
                       f2 + 1,   /* command field */
                       f3 + 1);  /* output field */
    }
    return 0;
}
