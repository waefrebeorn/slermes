/**
 * port_agent_context_compressor.c — Port of Python agent/context_compressor.py
 *
 * Real C implementations for context compression helpers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include "hermes_logger.h"
#include "hermes_json.h"

/* Port of Python: _compute_threshold_tokens */
/* PoP: compute_threshold_tokens @ hermes_cli/context_switch_guard.py:_threshold_tokens */
int compute_threshold_tokens(int context_length, float threshold_percent)
{
    if (context_length <= 0) return 0;
    int base = (int)(context_length * threshold_percent);
    int minimum_context = 64000; /* MINIMUM_CONTEXT_LENGTH */
    int result;
    if (base >= context_length) {
        /* Floor meets/exceeds window — use 85% trigger */
        result = (int)(context_length * 0.85f);
    } else {
        result = (base > minimum_context) ? base : minimum_context;
    }
    hermes_log(LOG_DEBUG, "port", "compute_threshold_tokens: len=%d pct=%.2f -> %d",
               context_length, threshold_percent, result);
    return result;
}

/* Port of Python: _effective_protect_first_n */
int effective_protect_first_n(int compression_count, bool has_previous_summary)
{
    int result;
    if (compression_count >= 1 || has_previous_summary) {
        result = 0; /* Decay to 0 after first compression */
    } else {
        result = -1; /* Use default protect_first_n */
    }
    hermes_log(LOG_DEBUG, "port", "effective_protect_first_n: count=%d summary=%d -> %d",
               compression_count, has_previous_summary, result);
    return result;
}
