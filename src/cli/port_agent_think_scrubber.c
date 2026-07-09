/*
 * port_agent_think_scrubber.c — C port of agent/think_scrubber.py
 */

#include "hermes.h"
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

