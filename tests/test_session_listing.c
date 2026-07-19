/*
 * test_session_listing.c — unit tests for the pure hermes_cli/session_listing.py
 * helpers. Invariants derived from a Python oracle.
 */

#include "session_listing_helpers.h"
#include "libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;
#define CHK(cond, lbl) do { \
    if (!(cond)) { printf("FAIL: %s\n", lbl); g_fail++; } \
    else printf("ok: %s\n", lbl); \
} while (0)

int main(void)
{
    /* parse_session_listing_args flags */
    int ia, iu; char tgt[256];
    session_listing_parse_flags("", &ia, &iu, tgt, sizeof tgt);
    CHK(!ia && !iu && tgt[0] == '\0', "flags empty");

    session_listing_parse_flags("list", &ia, &iu, tgt, sizeof tgt);
    CHK(!ia && !iu && tgt[0] == '\0', "flags list alias dropped");

    session_listing_parse_flags("ls browse", &ia, &iu, tgt, sizeof tgt);
    CHK(!ia && !iu && tgt[0] == '\0', "flags ls+browse dropped");

    session_listing_parse_flags("--all --full", &ia, &iu, tgt, sizeof tgt);
    CHK(ia && iu && tgt[0] == '\0', "flags --all --full");

    session_listing_parse_flags("all full My Session", &ia, &iu, tgt, sizeof tgt);
    CHK(ia && iu && strcmp(tgt, "My Session") == 0, "flags all full + target");

    session_listing_parse_flags("resume abc123", &ia, &iu, tgt, sizeof tgt);
    CHK(!ia && !iu && strcmp(tgt, "resume abc123") == 0, "flags target delegates to resume");

    session_listing_parse_flags("   spaced   target  ", &ia, &iu, tgt, sizeof tgt);
    CHK(strcmp(tgt, "spaced target") == 0, "flags target trimmed");

    /* shlex: quotes dropped, tokens split */
    char **av; int n;
    av = session_listing_parse_args("\"quoted arg\" plain", &n);
    CHK(n == 2 && strcmp(av[0], "quoted arg") == 0 && strcmp(av[1], "plain") == 0,
         "shlex quoted token");
    session_listing_free_argv(av, n);

    /* format_gateway_session_listing */
    const char *rows =
        "["
        "{\"id\":\"s1\",\"title\":\"First\",\"preview\":\"hello world\",\"source\":\"cli\"},"
        "{\"id\":\"s2\",\"title\":\"\",\"preview\":\"\",\"source\":\"tg\"},"
        "{\"id\":\"s3\",\"title\":\"Third\",\"preview\":\"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\","
        "\"source\":\"disc\"}"
        "]";

    char *f1 = session_listing_format_gateway(rows, 0, "Sessions");
    CHK(f1 != NULL, "format non-empty");
    CHK(strstr(f1, "\xf0\x9f\x93\x8b **Sessions**") != NULL, "format header emoji+title");
    CHK(strstr(f1, "1. **First**") != NULL, "format row1 title");
    CHK(strstr(f1, "— `s1` — _hello world_") != NULL, "format row1 id+preview");
    CHK(strstr(f1, "2. **\xe2\x80\x94**") != NULL, "format row2 unnamed -> em dash");
    /* preview truncated to <=40 chars (oracle: Python [:40]); assert a 41-x run is absent */
    CHK(strstr(f1, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx") == NULL, "format preview truncated to 40");
    CHK(strstr(f1, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx") != NULL, "format preview kept 40 chars");
    CHK(strstr(f1, "Resume: `/resume <session id>`") != NULL, "format resume footer");
    free(f1);

    char *f2 = session_listing_format_gateway(rows, 1, "All");
    CHK(strstr(f2, "1. **First** `cli` — `s1`") != NULL, "format with-source includes `cli`");
    free(f2);

    char *f3 = session_listing_format_gateway("[]", 0, "Sessions");
    CHK(strstr(f3, "No sessions found.") != NULL, "format empty -> no-sessions msg");
    free(f3);

    if (g_fail) { printf("\n%d FAIL\n", g_fail); return 1; }
    printf("\nALL PASSED\n");
    return 0;
}
