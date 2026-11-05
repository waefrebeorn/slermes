/* t_port_cli_process_dock.c — oracle harness for hermes_cli/cli_process_dock.py
 * pure helpers: _last_output_line, process_status, process_glyph. */
#include "port_cli_process_dock.h"

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

    /* _last_output_line: buffers with blank lines, \r\n, whitespace. */
    static const char *const bufs[] = {
        "first\n\nlast line\n", "only one", "", NULL, "a  b\tc\n  \nd\re",
        "\n\n\n", "  pad  \n  tail ",
    };
    for (size_t i = 0; i < sizeof(bufs) / sizeof(bufs[0]); i++) {
        char *out = cpa_last_output_line(bufs[i]);
        printf("{\"family\":\"lol\",\"case\":%zu,\"buf\":", i);
        jstr(bufs[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }

    /* process_status: (exited, reason, exit_truthy). */
    struct { int e; const char *r; int x; } st[] = {
        {0, "", 0},{1, "", 0},{1, "killed", 0},{1, "lost", 0},{1, "failed_start", 0},
        {1, "", 1},{1, "unknown", 0},{1, "quota", 1},{1, "killed", 1},
    };
    for (size_t i = 0; i < sizeof(st) / sizeof(st[0]); i++) {
        const char *out = cpa_process_status(st[i].e, st[i].r, st[i].x);
        printf("{\"family\":\"st\",\"case\":%zu,\"exited\":%d,\"reason\":", i, st[i].e);
        jstr(st[i].r);
        printf(",\"exit\":%d,\"out\":", st[i].x);
        jstr(out);
        printf("}\n");
    }

    /* process_glyph: status -> glyph. */
    static const char *const gly[] = {
        "running", "done", "failed", "killed", "lost", "unknown", NULL,
    };
    for (size_t i = 0; i < sizeof(gly) / sizeof(gly[0]); i++) {
        const char *out = cpa_process_glyph(gly[i]);
        printf("{\"family\":\"gly\",\"case\":%zu,\"status\":", i);
        jstr(gly[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
    }
    return 0;
}
