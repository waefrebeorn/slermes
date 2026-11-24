/*
 * plugins_cmd_update.c -- faithful C11 port of the deterministic re-install
 * message in hermes_cli/plugins_cmd_update.py (wave17 gate w17_pluginscmd_488ba1).
 *
 * PoP: w17_pluginscmd_488ba1_reclone_revision_message @ hermes_cli/plugins_cmd_update.py:_reclone_plugin_update
 *
 * The clone/consent/scan and filesystem halves stay in Python. The exported
 * function implements the message branch exactly, including the 8-character
 * slices and the `isinstance(str)` fallback for a non-string previous revision.
 */
#include "noc_w17_pluginscmd_488ba1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s)
{
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* Copy at most 8 leading bytes, writing the NUL at the *new* length. */
static size_t lead8(char *dst, const char *s)
{
    size_t n = strlen(s);
    if (n > 8) n = 8;
    memcpy(dst, s, n);
    dst[n] = '\0';
    return n;
}

/* PoP: w17_pluginscmd_488ba1_reclone_revision_message @ hermes_cli/plugins_cmd_update.py:_reclone_plugin_update */
char *w17_pluginscmd_488ba1_reclone_revision_message(const char *source,
                                                     json_t *previous_revision,
                                                     const char *revision)
{
    const char *src = source ? source : "";
    const char *rev = revision ? revision : "";
    /* previous = previous_revision if isinstance(previous_revision, str) else "" */
    const char *prev = "";
    if (previous_revision && json_is_string(previous_revision)) {
        const char *p = json_string_value(previous_revision);
        prev = p ? p : "";
    }

    /* if revision and revision == previous */
    if (*rev && strcmp(rev, prev) == 0)
        return dupstr("Already up to date.");

    char pbuf[9], rbuf[9];
    lead8(pbuf, prev);
    lead8(rbuf, rev);
    size_t need = strlen(src) + 64;
    char *msg = (char *)malloc(need);
    if (!msg) return NULL;
    snprintf(msg, need, "Re-installed from %s: %s..%s", src, pbuf, rbuf);
    return msg;
}

