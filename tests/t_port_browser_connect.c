/*
 * t_port_browser_connect.c -- oracle harness for port_browser_connect.c
 *
 * Emits one JSON object per line, echoing the inputs from C so the oracle
 * cannot drift from the fixtures.
 */
#include "port_browser_connect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if (*p == '\n') fputs("\\n", stdout);
        else putchar(*p);
    }
    putchar('"');
}

/* substring match, the way the Python side uses re.search for these tables */
static int sub_match(const char *value, const char *needle)
{
    if (!value || !needle) return 0;
    return strstr(value, needle) != NULL;
}

static const char *CHANNELS[] = { "beta", "dev", "canary", "nightly", NULL };

static const bc_fragment_t TABLE[] = {
    { "chrome",        "chrome" },
    { "msedge",        "edge" },
    { "firefox",       "firefox" },
    { "brave",         "brave" },
    { "arc",           "arc" },
    { NULL, NULL }
};

static void emit_classify(const char *value)
{
    const char *got = bc_classify_default(value, CHANNELS, TABLE, sub_match);
    printf("{\"fn\":\"_classify_default\",\"in\":");
    jstr(value);
    printf(",\"out\":");
    jstr(got);
    printf("}\n");
}

static void emit_first_present(const char *label, const char *const *paths)
{
    const char *got = bc_first_present(paths);
    printf("{\"fn\":\"_first_present\",\"case\":");
    jstr(label);
    printf(",\"out\":");
    jstr(got);
    printf("}\n");
}

int main(void)
{
    /* ---- _classify_default ---- */
    emit_classify("Chrome");
    emit_classify("chrome-beta");
    emit_classify("Microsoft Edge");
    emit_classify("Firefox");
    emit_classify("Brave Browser");
    emit_classify("Arc");
    emit_classify("some-unknown-browser");
    emit_classify(" Canary ");
    emit_classify("dev");                 /* a channel fragment alone */
    emit_classify("chromium");            /* not in the table */

    /* ---- _first_present ---- */
    {
        const char *paths[] = { "", "/definitely/not/here", NULL };
        emit_first_present("all-missing", paths);
    }
    {
        const char *paths[] = { NULL };
        emit_first_present("empty", paths);
    }
    {
        /* this harness file always exists, so it must be found */
        const char *paths[] = { "/nope/nope", "tests/t_port_browser_connect.c", NULL };
        emit_first_present("second-exists", paths);
    }
    {
        const char *paths[] = { "tests", NULL };   /* a directory, not a file */
        emit_first_present("directory-not-file", paths);
    }

    return 0;
}
