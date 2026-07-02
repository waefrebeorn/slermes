/*
 * port_tools_tool_result_storage.c — C port of tools/tool_result_storage.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_tool_result_storage__resolve_storage_dir @ tools/tool_result_storage.py:_resolve_storage_dir */

/* Port of Python tools/tool_result_storage.py:_resolve_storage_dir */
/* Returns the best temp-backed storage dir for this environment. */
int cli_tools_tool_result_storage__resolve_storage_dir(
    const char *env_temp_dir, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return -1;
    }
    if (env_temp_dir && env_temp_dir[0]) {
        snprintf(output, output_size, "%s/hermes-results", env_temp_dir);
    } else {
        strncpy(output, "/tmp/hermes-results", output_size - 1);
        output[output_size - 1] = '\0';
    }
    return 0;
}

/* PoP: cli_tools_tool_result_storage__heredoc_marker @ tools/tool_result_storage.py:_heredoc_marker */

/* Port of Python tools/tool_result_storage.py:_heredoc_marker */
/* Returns a heredoc delimiter that doesn't collide with content. */
int cli_tools_tool_result_storage__heredoc_marker(
    const char *content, char *marker_out, size_t marker_size)
{
    if (!content || !marker_out || marker_size == 0) {
        return -1;
    }
    /* Use a fixed marker — collision check is a safety measure. */
    if (strstr(content, "HERMES_PERSIST_EOF") != NULL) {
        /* Content contains the default marker; use a unique one. */
        snprintf(marker_out, marker_size, "HERMES_PERSIST_%08x",
                 (unsigned int)(uintptr_t)content);
    } else {
        strncpy(marker_out, "HERMES_PERSIST_EOF", marker_size - 1);
        marker_out[marker_size - 1] = '\0';
    }
    return 0;
}

/* PoP: cli_tools_tool_result_storage__write_to_sandbox @ tools/tool_result_storage.py:_write_to_sandbox */

/* Port of Python tools/tool_result_storage.py:_write_to_sandbox */
/* Writes content into the sandbox via env.execute(). Returns 1 on success. */
int cli_tools_tool_result_storage__write_to_sandbox(
    const char *content, const char *remote_path)
{
    if (!content || !remote_path) {
        return 0;
    }
    /* CLI port: sandbox write requires env.execute(). */
    /* Return 0 (failure) — the caller should fall back to inline truncation. */
    hermes_log(LOG_DEBUG, "tool_result_storage",
               "sandbox write to %s not available in CLI port", remote_path);
    return 0;
}

/* PoP: cli_tools_tool_result_storage__build_persisted_message @ tools/tool_result_storage.py:_build_persisted_message */

/* Port of Python tools/tool_result_storage.py:_build_persisted_message */
/* Builds the <persisted-output> replacement block. */
int cli_tools_tool_result_storage__build_persisted_message(
    const char *preview, int has_more, int original_size,
    const char *file_path, char *output, size_t output_size)
{
    if (!preview || !file_path || !output || output_size == 0) {
        return -1;
    }
    double size_kb = original_size / 1024.0;
    char size_str[64];
    if (size_kb >= 1024.0) {
        snprintf(size_str, sizeof(size_str), "%.1f MB", size_kb / 1024.0);
    } else {
        snprintf(size_str, sizeof(size_str), "%.1f KB", size_kb);
    }
    snprintf(output, output_size,
             "<persisted-output>\n"
             "This tool result was too large (%d characters, %s).\n"
             "Full output saved to: %s\n"
             "Use the read_file tool with offset and limit to access specific sections of this output.\n\n"
             "Preview (first %d chars):\n%s%s\n"
             "</persisted-output>",
             original_size, size_str, file_path,
             (int)strlen(preview), preview,
             has_more ? "\n..." : "");
    return 0;
}

/* PoP: cli_tools_tool_result_storage_maybe_persist_tool_result @ tools/tool_result_storage.py:maybe_persist_tool_result */

/* Port of Python tools/tool_result_storage.py:maybe_persist_tool_result */
/* Layer 2: persist oversized result into the sandbox, return preview + path. */
int cli_tools_tool_result_storage_maybe_persist_tool_result(
    const char *content, const char *tool_name, const char *tool_use_id,
    int threshold, char *output, size_t output_size)
{
    if (!content || !tool_name || !tool_use_id || !output || output_size == 0) {
        return -1;
    }
    int content_len = (int)strlen(content);
    if (content_len <= threshold) {
        /* Content is small enough — return as-is. */
        strncpy(output, content, output_size - 1);
        output[output_size - 1] = '\0';
        return 0;
    }
    /* Content exceeds threshold — generate preview. */
    int preview_len = threshold / 2;
    if (preview_len > content_len) preview_len = content_len;
    /* Truncate at last newline within preview. */
    int last_nl = -1;
    for (int i = 0; i < preview_len; i++) {
        if (content[i] == '\n') last_nl = i;
    }
    if (last_nl > preview_len / 2) {
        preview_len = last_nl + 1;
    }
    char *preview = malloc(preview_len + 1);
    if (!preview) {
        strncpy(output, content, output_size - 1);
        output[output_size - 1] = '\0';
        return 0;
    }
    strncpy(preview, content, preview_len);
    preview[preview_len] = '\0';
    char storage_dir[256];
    cli_tools_tool_result_storage__resolve_storage_dir(NULL, storage_dir, sizeof(storage_dir));
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/%s.txt", storage_dir, tool_use_id);
    /* Try sandbox write (will fail in CLI port). */
    int written = cli_tools_tool_result_storage__write_to_sandbox(content, file_path);
    if (written) {
        cli_tools_tool_result_storage__build_persisted_message(
            preview, 1, content_len, file_path, output, output_size);
    } else {
        /* Fall back to inline truncation. */
        snprintf(output, output_size,
                 "%s\n\n[Truncated: tool response was %d chars. "
                 "Full output could not be saved to sandbox.]",
                 preview, content_len);
    }
    free(preview);
    return 0;
}

/* PoP: cli_tools_tool_result_storage_enforce_turn_budget @ tools/tool_result_storage.py:enforce_turn_budget */

/* Port of Python tools/tool_result_storage.py:enforce_turn_budget */
/* Layer 3: enforce aggregate budget across all tool results in a turn. */
int cli_tools_tool_result_storage_enforce_turn_budget(
    char *tool_contents[], int *tool_sizes[], int tool_count,
    int turn_budget, int *persisted_indices[], int *persisted_count)
{
    if (!tool_contents || !tool_sizes || !persisted_indices || !persisted_count) {
        return -1;
    }
    *persisted_count = 0;
    /* Calculate total size. */
    int total = 0;
    for (int i = 0; i < tool_count; i++) {
        if (tool_sizes[i]) total += *tool_sizes[i];
    }
    if (total <= turn_budget) {
        return 0;  /* under budget */
    }
    /* CLI port: budget enforcement requires sandbox writes. */
    /* Just report the overflow. */
    hermes_log(LOG_DEBUG, "tool_result_storage",
               "turn budget exceeded: %d > %d chars", total, turn_budget);
    return 0;
}
