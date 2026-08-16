#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
/*
 * ports/gateway/response_filters.c — C11 port of gateway/response_filters.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/cli/port_gateway_response_filters.c ── */
/*
 * port_gateway_response_filters.c — C port of gateway/response_filters.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Known silence markers (uppercase, normalized).
 * MUST match LIVE Python gateway/response_filters.py LIVE_GATEWAY_SILENT_MARKERS
 * exactly (4 markers) — the legacy wider list fabricated silences Python would
 * not (e.g. "NO_RESPONSE", "[SILENCE]"). */
static const char *SILENT_MARKERS[] = {
    "[SILENT]", "SILENT", "NO_REPLY", "NO REPLY", NULL
};

/* PoP: cli_gateway_response_filters__canonical_silence_candidate @ gateway/response_filters.py:_canonical_silence_candidate */

/* Port of Python gateway/response_filters.py:_canonical_silence_candidate */
/* Canonicalize a silence candidate: strip, uppercase, collapse whitespace. */
char *cli_gateway_response_filters__canonical_silence_candidate(const char *text)
{
    if (!text) return strdup("");

    /* Skip leading whitespace */
    while (*text == ' ' || *text == '\t') text++;

    /* Build canonical form: uppercase, single spaces */
    size_t len = strlen(text);
    char *result = (char *)malloc(len + 1);
    if (!result) return NULL;

    int j = 0;
    int in_space = 0;
    for (const char *p = text; *p; p++) {
        if (*p == ' ' || *p == '\t') {
            if (!in_space && j > 0) {
                result[j++] = ' ';
                in_space = 1;
            }
        } else {
            result[j++] = (char)toupper((unsigned char)*p);
            in_space = 0;
        }
    }

    /* Trim trailing space */
    if (j > 0 && result[j - 1] == ' ') j--;
    result[j] = '\0';

    return result;
}

/* PoP: cli_gateway_response_filters_is_intentional_silence_response @ gateway/response_filters.py:is_intentional_silence_response */

/* Port of Python gateway/response_filters.py:is_intentional_silence_response */
/* Return 1 only when response is exactly a silence marker. */
int cli_gateway_response_filters_is_intentional_silence_response(const char *response)
{
    if (!response) return 0;

    /* Skip leading whitespace */
    while (*response == ' ' || *response == '\t') response++;
    if (!*response) return 0; /* blank response is not silence */

    /* Check length limit */
    size_t len = strlen(response);
    if (len > 64) return 0;

    /* Canonicalize and check against known markers */
    char *canonical = cli_gateway_response_filters__canonical_silence_candidate(response);
    if (!canonical) return 0;

    for (int i = 0; SILENT_MARKERS[i]; i++) {
        if (strcmp(canonical, SILENT_MARKERS[i]) == 0) {
            free(canonical);
            return 1;
        }
    }

    free(canonical);
    return 0;
}

/* PoP: cli_gateway_response_filters_is_intentional_silence_agent_result @ gateway/response_filters.py:is_intentional_silence_agent_result */

/* Port of Python gateway/response_filters.py:is_intentional_silence_agent_result */
/* Silence markers suppress delivery only for successful agent turns. */
int cli_gateway_response_filters_is_intentional_silence_agent_result(
    const char *agent_result_json, const char *response)
{
    if (!agent_result_json || !response) return 0;

    /* Check if agent_result indicates failure */
    if (strstr(agent_result_json, "\"failed\"")) {
        /* Check for "failed": true */
        const char *failed = strstr(agent_result_json, "\"failed\"");
        if (failed) {
            failed += 8; /* skip '"failed"' */
            while (*failed == ' ' || *failed == '\t' || *failed == ':') failed++;
            if (strncmp(failed, "true", 4) == 0) {
                return 0; /* failed agent turn — not silence */
            }
        }
    }

    return cli_gateway_response_filters_is_intentional_silence_response(response);
}

/* LIVE Python marker set (gateway/response_filters.py LIVE_GATEWAY_SILENT_MARKERS).
 * NOTE: must match the Python frozenset exactly, NOT the wider legacy list above. */
static const char *PARTIAL_SILENT_MARKERS[] = {
    "[SILENT]", "SILENT", "NO_REPLY", "NO REPLY", NULL
};

/* PoP: marker @ gateway/response_filters.py:is_partial_silence_marker */

/* Port of Python gateway/response_filters.py:is_partial_silence_marker */
/* True while `text` is a non-empty prefix of some silence marker (streaming hold-back). */
int cli_gateway_response_filters_is_partial_silence_marker(const char *text)
{
    if (!text) return 0;

    /* Skip leading whitespace */
    while (*text == ' ' || *text == '\t') text++;
    if (!*text) return 0;

    size_t len = strlen(text);
    if (len > 64) return 0;

    char *canonical = cli_gateway_response_filters__canonical_silence_candidate(text);
    if (!canonical) return 0;

    /* strip trailing space produced by canonicalizer */
    size_t cl = strlen(canonical);
    if (cl > 0 && canonical[cl - 1] == ' ') canonical[cl - 1] = '\0';

    int found = 0;
    if (canonical[0] != '\0') {
        for (int i = 0; PARTIAL_SILENT_MARKERS[i]; i++) {
            if (strncmp(PARTIAL_SILENT_MARKERS[i], canonical, strlen(canonical)) == 0) {
                found = 1;
                break;
            }
        }
    }
    free(canonical);
    return found;
}

/* PoP: lanes @ gateway/response_filters.py:is_autonomous_silence_response */

/* Loose silence matcher for autonomous lanes (cron, webhook).
 * Suppresses when a marker is the whole response, sits on its own first or
 * last non-empty line, or the bracketed sentinel opens the response
 * ("[SILENT] No changes detected"). A token buried mid-sentence is delivered. */
int cli_gateway_response_filters_is_autonomous_silence_response(const char *response)
{
    if (!response) return 0;

    /* strip() */
    const char *s = response;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return 0;
    size_t sl = strlen(s);
    while (sl > 0 && isspace((unsigned char)s[sl - 1])) sl--;
    if (sl == 0) return 0;

    /* Whole response is exactly a marker. */
    {
        char *tmp = strndup(s, sl);
        char *canon = cli_gateway_response_filters__canonical_silence_candidate(tmp);
        int whole = 0;
        if (canon) {
            for (int i = 0; SILENT_MARKERS[i]; i++) {
                if (strcmp(canon, SILENT_MARKERS[i]) == 0) { whole = 1; break; }
            }
            free(canon);
        }
        free(tmp);
        if (whole) return 1;
    }

    /* Marker on its own first or last non-empty line. */
    {
        char *copy = strndup(s, sl);
        /* first non-empty line token */
        char *save1 = NULL;
        char *tok = strtok_r(copy, "\n", &save1);
        while (tok && !*tok) tok = strtok_r(NULL, "\n", &save1);
        if (tok) {
            char *c1 = cli_gateway_response_filters__canonical_silence_candidate(tok);
            if (c1) {
                for (int i = 0; SILENT_MARKERS[i]; i++) {
                    if (strcmp(c1, SILENT_MARKERS[i]) == 0) { free(c1); free(copy); return 1; }
                }
                free(c1);
            }
        }
        /* last non-empty line token */
        char *last = NULL, *prev = NULL;
        char *save2 = NULL;
        char *t2 = strtok_r(copy, "\n", &save2);
        while (t2) {
            if (*t2) prev = last, last = t2;
            t2 = strtok_r(NULL, "\n", &save2);
        }
        if (last && last != tok) {
            char *c2 = cli_gateway_response_filters__canonical_silence_candidate(last);
            if (c2) {
                for (int i = 0; SILENT_MARKERS[i]; i++) {
                    if (strcmp(c2, SILENT_MARKERS[i]) == 0) { free(c2); free(copy); return 1; }
                }
                free(c2);
            }
        }
        (void)prev;
        free(copy);
    }

    /* Bracketed sentinel prefix: stripped.upper().startswith("[SILENT]"). */
    if (sl >= 7) {
        if ((s[0] == '[' || s[0] == '[') &&
            strncasecmp(s, "[SILENT]", 8) == 0)
            return 1;
    }

    return 0;
}

