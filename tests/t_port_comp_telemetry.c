/* t_port_comp_telemetry.c — oracle harness for the pure helpers of
 * agent/context_compressor_telemetry.py. */
#include "port_comp_telemetry.h"

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

    /* _compression_method: (fallback, has_aux_error). */
    static const int ms[][2] = {{0,0},{1,0},{0,1},{1,1}};
    for (int i = 0; i < 4; i++) {
        const char *out = ctele_method(ms[i][0], ms[i][1]);
        printf("{\"family\":\"method\",\"case\":%d,\"fallback\":%d,"
               "\"auxerr\":%d,\"out\":\"%s\"}\n", i, ms[i][0], ms[i][1], out);
    }

    /* failure truncation. */
    char shortf[300], longf[300];
    int k;
    const char small[] = "endpoint quota exceeded";
    for (k = 0; k < 219; k++) shortf[k] = 'a'; shortf[219] = '\0';   /* len 219 */
    for (k = 0; k < 230; k++) longf[k] = 'b'; longf[230] = '\0';     /* len 230 */
    char trail[230];
    for (k = 0; k < 220; k++) trail[k] = 'c'; trail[220] = '\0';     /* len 220 */
    const char *fs[] = {small, shortf, longf, trail, "x"};
    for (int i = 0; i < 5; i++) {
        char *out = ctele_clamp_failure(fs[i]);
        printf("{\"family\":\"clamp\",\"case\":%d,\"f\":", i);
        jstr(fs[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }
    /* a >220 case with trailing spaces to exercise rstrip. */
    char sp[300];
    for (k = 0; k < 222; k++) sp[k] = 'e'; sp[222] = ' '; sp[223] = ' '; sp[224] = '\0';
    {
        char *out = ctele_clamp_failure(sp);
        printf("{\"family\":\"clamp\",\"case\":6,\"f\":");
        jstr(sp);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }

    /* fit_margin: (eff, prompt, resv, resv_present). */
    struct { long long e, p, r; int pr; } fm[] = {
        {9000, 6000, 1000, 1}, {9000, 6000, 0, 1}, {9000, 6000, 0, 0},
        {2000, 500, 50, 1}, {10000, 0, 0, 0},
    };
    for (int i = 0; i < 5; i++) {
        long long out = ctele_fit_margin(fm[i].e, fm[i].p, fm[i].r, fm[i].pr);
        printf("{\"family\":\"margin\",\"case\":%d,\"eff\":%lld,\"prompt\":%lld,"
               "\"resv\":%lld,\"resvpres\":%d,\"out\":%lld}\n",
               i, fm[i].e, fm[i].p, fm[i].r, fm[i].pr, out);
    }
    return 0;
}
