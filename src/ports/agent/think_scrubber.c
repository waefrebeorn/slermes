/*
 * ports/agent/think_scrubber.c — C11 port of agent/think_scrubber.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/agent/port_think_scrubber_helpers2.c ── */
/*
 * port_think_scrubber_remaining2.c — Port of agent/think_scrubber.py
 * block-boundary surface. Tag-boundary detection, partial-suffix
 * holdback, in-block state.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static const char *TAGS[] = {"<thinking>", "</thinking>", "<reasoning_scratchpad>", "</reasoning_scratchpad>", NULL};

/* PoP: ths_init @ agent/think_scrubber.py:__init__ */
char *ths_init(void) {
    return strdup("{\"in_block\": false, \"buf\": \"\"}");
}

/* PoP: ths_is_block_boundary @ agent/think_scrubber.py:_is_block_boundary */
bool ths_is_block_boundary(const char *buf, long idx) {
    /* Python: position is boundary of any tag. */
    if (!buf || idx < 0) return false;
    for (int t = 0; TAGS[t]; t++) {
        size_t tl = strlen(TAGS[t]);
        if ((size_t)idx == tl || (size_t)idx == 0) continue;
        if ((size_t)idx >= tl) {
            if (strncmp(buf + idx - tl, TAGS[t], tl) == 0) return true;
        }
        if (strncmp(buf + idx, TAGS[t], tl) == 0) return true;
    }
    return false;
}

/* PoP: ths_max_partial_suffix @ agent/think_scrubber.py:_max_partial_suffix */
long ths_max_partial_suffix(const char *buf) {
    /* Python: longest suffix that is tag prefix. */
    if (!buf) return 0;
    size_t bl = strlen(buf);
    for (size_t len = bl; len > 0; len--) {
        const char *suffix = buf + bl - len;
        for (int t = 0; TAGS[t]; t++) {
            size_t tl = strlen(TAGS[t]);
            if (len < tl && strncmp(suffix, TAGS[t], len) == 0) return (long)len;
        }
    }
    return 0;
}


/* ── from src/cli/port_agent_think_scrubber.c ── */
/*
 * port_agent_think_scrubber.c — C port of agent/think_scrubber.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_think_scrubber_feed @ agent/think_scrubber.py:feed */

/* Port of Python agent/think_scrubber.py:feed */
/* Feeds a delta to the streaming think scrubber. Returns visible text. */
int cli_agent_think_scrubber_feed(
    const char *delta, int in_think_block, char *output, size_t output_size)
{
    if (!delta || !output || output_size == 0) {
        return -1;
    }
    /* Simple implementation: if in a think block, suppress output. */
    /* Full implementation would track partial tag boundaries. */
    if (in_think_block) {
        /* Check if this delta closes the think block. */
        if (strstr(delta, "</think>") || strstr(delta, "</thinking>")) {
            /* Block closed — output content after the closing tag. */
            const char *close = strstr(delta, "</think>");
            if (!close) close = strstr(delta, "</thinking>");
            if (close) {
                const char *after = close + (strstr(delta, "</think>") ? 8 : 11);
                strncpy(output, after, output_size - 1);
                output[output_size - 1] = '\0';
                return 0;  /* no longer in think block */
            }
        }
        output[0] = '\0';  /* suppress */
        return 1;  /* still in think block */
    }
    /* Not in a think block — check if this delta opens one. */
    if (strstr(delta, "<think>") || strstr(delta, "<thinking>")) {
        /* Block opened — suppress the tag and everything after. */
        output[0] = '\0';
        return 1;  /* now in think block */
    }
    /* Normal content — pass through. */
    strncpy(output, delta, output_size - 1);
    output[output_size - 1] = '\0';
    return 0;  /* not in think block */
}

/* PoP: cli_agent_think_scrubber_flush @ agent/think_scrubber.py:flush */

/* Port of Python agent/think_scrubber.py:flush */
/* Flushes any held-back content at end of stream. */
int cli_agent_think_scrubber_flush(
    const char *held_back, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return -1;
    }
    if (held_back && held_back[0]) {
        strncpy(output, held_back, output_size - 1);
        output[output_size - 1] = '\0';
    } else {
        output[0] = '\0';
    }
    return 0;
}




/* Port of Python agent/think_scrubber.py:is_in_think_block */
/* Returns 1 if currently inside a think block, 0 otherwise. */


