/*
 * tool_output.c — Configurable tool-output truncation limits.
 * Port of Python tools/tool_output_limits.py.
 *
 * Reads from HERMES_TOOL_OUTPUT_* env vars. Falls back to hardcoded defaults.
 *
 * MIT License — WuBu Hermes Project
 */

#include "tool_output.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Process-lifetime cache, mirroring Python's _cached_limits (populated on
 * first call, cleared by tool_output_reset_cache). */
static int g_cached_max_bytes = 0;
static int g_cached_max_lines = 0;
static int g_cached_max_line_length = 0;
static bool g_cache_initialized = false;

/* ================================================================
 *  Helpers
 * ================================================================ */

static int read_env_int(const char *name, int default_val) {
    const char *val = getenv(name);
    if (!val || !*val) return default_val;
    char *end = NULL;
    long n = strtol(val, &end, 10);
    if (end == val || *end != '\0' || n <= 0 || n > 100000000)
        return default_val;
    return (int)n;
}

/* ================================================================
 *  Public API
 * ================================================================ */

static void tool_output_ensure_cache(void) {
    if (g_cache_initialized) return;
    g_cached_max_bytes = read_env_int("HERMES_TOOL_OUTPUT_MAX_BYTES",
                                       TOOL_OUTPUT_DEFAULT_MAX_BYTES);
    g_cached_max_lines = read_env_int("HERMES_TOOL_OUTPUT_MAX_LINES",
                                       TOOL_OUTPUT_DEFAULT_MAX_LINES);
    g_cached_max_line_length = read_env_int("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH",
                                             TOOL_OUTPUT_DEFAULT_MAX_LINE_LENGTH);
    g_cache_initialized = true;
}

void tool_output_reset_cache(void) {
    g_cache_initialized = false;
    g_cached_max_bytes = 0;
    g_cached_max_lines = 0;
    g_cached_max_line_length = 0;
}

int tool_output_get_max_bytes(void) {
    tool_output_ensure_cache();
    return g_cached_max_bytes;
}

int tool_output_get_max_lines(void) {
    tool_output_ensure_cache();
    return g_cached_max_lines;
}

int tool_output_get_max_line_length(void) {
    tool_output_ensure_cache();
    return g_cached_max_line_length;
}

bool tool_output_exceeds_byte_limit(size_t byte_count) {
    return byte_count > (size_t)tool_output_get_max_bytes();
}

bool tool_output_exceeds_line_limit(int line_count) {
    return line_count > tool_output_get_max_lines();
}
