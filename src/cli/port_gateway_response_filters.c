/*
 * port_gateway_response_filters.c — C port of gateway/response_filters.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Known silence markers (uppercase, normalized) */
static const char *SILENT_MARKERS[] = {
    "NO_REPLY", "NO REPLY", "[SILENT]", "[SILENCE]",
    "SILENT", "NO_RESPONSE", "NO RESPONSE", NULL
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
