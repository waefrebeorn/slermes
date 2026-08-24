/*
 * trajectory_compressor.c — canonical C11 port of trajectory_compressor.py
 * (core compression strategy surface).
 *
 * Post-processes completed agent trajectories to compress them within a
 * target token budget while preserving training signal quality.
 *
 * Strategy (mirrors the Python constants):
 *   1. Protect first turns (system, human, first gpt, first tool)
 *   2. Protect last N turns (final actions/conclusions)
 *   3. Compress middle turns only, from the 2nd tool response onward
 *   4. Compress only as much as needed to fit under target
 *   5. Replace the compressed region with a single summary message
 *   6. Keep remaining tool calls intact
 *
 * The offline training-data tool (JSONL in/out) is a Python-side utility;
 * this port exposes the turn-selection + budget math for the agent core.
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: tc_protect_first_turns @ trajectory_compressor.py:PROTECT_FIRST_TURNS */
int tc_protect_first_turns(void) { return 4; }

/* PoP: tc_protect_last_turns_default @ trajectory_compressor.py:PROTECT_LAST_TURNS */
int tc_protect_last_turns(void) { return 3; }

/* PoP: tc_target_max_tokens_default @ trajectory_compressor.py:TARGET_MAX_TOKENS */
int tc_target_max_tokens(void) { return 16000; }

/*
 * PoP: tc_estimate_tokens @ trajectory_compressor.py:_estimate_tokens
 */
long tc_estimate_tokens(const char *text) {
    if (!text) return 0;
    return (long)(strlen(text) / 4);
}

/* PoP: tc_select_compressible_range @ trajectory_compressor.py (turn selection)
 * Given n messages, compute the [start,end) index window eligible for
 * compression (protect head and tail). Returns 0 when nothing is eligible. */
int tc_select_compressible_range(int n, int protect_last,
                                 int *out_start, int *out_end) {
    int start = tc_protect_first_turns() + 1; /* skip first gpt tool response */
    int end = n - protect_last;
    if (end <= start) return 0;
    *out_start = start; *out_end = end;
    return 1;
}

/* PoP: tc_build_summary_message @ trajectory_compressor.py (summary insertion)
 * Builds the single human summary message that replaces the compressed
 * region. Caller frees. */
json_t *tc_build_summary_message(const char *summary_text, long tokens_before,
                                 long tokens_after) {
    json_t *msg = json_object();
    json_set(msg, "role", json_string("human"));
    char body[4096];
    snprintf(body, sizeof body,
        "[Conversation history compressed to fit the %ld-token budget. "
        "Earlier turns (%ld -> %ld estimated tokens) were replaced with this "
        "summary. Recent tool calls and final conclusions follow verbatim.]",
        tc_target_max_tokens(), tokens_before, tokens_after);
    json_set(msg, "content",
             json_string(summary_text && *summary_text ? summary_text : body));
    return msg;
}
