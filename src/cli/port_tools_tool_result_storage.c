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

/* PoP: cli_tools_tool_result_storage__safe_result_filename @ tools/tool_result_storage.py:_safe_result_filename */

/* Port of Python tools/tool_result_storage.py:_safe_result_filename.
 * Returns a single safe filename for a tool result id. Caller frees. */
char *cli_tools_tool_result_storage__safe_result_filename(const char *tool_use_id)
{
    /* Mirror of Python:
     *   raw_id = str(tool_use_id or "tool_result")
     *   safe_stem = _UNSAFE_RESULT_FILENAME_CHARS.sub("_", raw_id).strip("._-")
     *   changed = safe_stem != raw_id
     *   if not safe_stem: safe_stem = "tool_result"; changed = True
     *   if changed or len(safe_stem) > 120:
     *       digest = sha256(raw_id)[:12]
     *       safe_stem = safe_stem[:120].rstrip("._-") or "tool_result"
     *       safe_stem = f"{safe_stem}_{digest}"
     *   return f"{safe_stem}.txt"
     */
    const char *raw_id = tool_use_id ? tool_use_id : "tool_result";

    /* Build safe_stem: replace any char not in [A-Za-z0-9_.-] with '_',
     * then strip leading/trailing '.', '_', '-'. */
    char stem[1024];
    size_t n = 0;
    for (const char *s = raw_id; *s && n + 1 < sizeof(stem); s++) {
        unsigned char c = (unsigned char)*s;
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
               || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        stem[n++] = ok ? (char)c : '_';
    }
    stem[n] = '\0';

    /* changed flags any divergence from raw_id: either an unsafe char was
     * substituted for '_', or leading/trailing '.', '_', '-' were stripped. */
    int changed = 0;
    for (size_t k = 0; raw_id[k]; k++) {
        unsigned char c = (unsigned char)raw_id[k];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
               || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (!ok) { changed = 1; break; }
    }

    /* strip leading '.', '_', '-' */
    size_t start = 0;
    while (stem[start] == '.' || stem[start] == '_' || stem[start] == '-') { start++; changed = 1; }
    /* strip trailing '.', '_', '-' */
    size_t end = n;
    while (end > start && (stem[end - 1] == '.' || stem[end - 1] == '_' || stem[end - 1] == '-')) {
        end--; changed = 1;
    }
    char safe_stem[1024];
    size_t m = 0;
    for (size_t k = start; k < end && m + 1 < sizeof(safe_stem); k++) {
        safe_stem[m++] = stem[k];
    }
    safe_stem[m] = '\0';

    if (m == 0) {
        strcpy(safe_stem, "tool_result");
        m = strlen(safe_stem);
        changed = 1;
    }

    const int MAX_STEM = 120;
    if (changed || (int)m > MAX_STEM) {
        /* sha256(raw_id)[:12] — hexdigest()[:12] is 12 hex chars (6 bytes). */
        unsigned char hash[32];
        crypto_sha256((const unsigned char *)raw_id, strlen(raw_id), hash);
        char digest[13];
        for (int d = 0; d < 6; d++) {
            sprintf(digest + d * 2, "%02x", hash[d]);
        }
        digest[12] = '\0';

        /* truncate safe_stem to 120 and rstrip '.', '_', '-', fallback. */
        if ((int)m > MAX_STEM) {
            m = MAX_STEM;
            safe_stem[m] = '\0';
        }
        while (m > 0 && (safe_stem[m - 1] == '.' || safe_stem[m - 1] == '_' || safe_stem[m - 1] == '-')) {
            m--;
            safe_stem[m] = '\0';
        }
        if (m == 0) {
            strcpy(safe_stem, "tool_result");
            m = strlen(safe_stem);
        }
        char out[1024];
        snprintf(out, sizeof(out), "%s_%s", safe_stem, digest);
        size_t need = strlen(out) + 5; /* + ".txt" + NUL */
        char *result = (char *)malloc(need);
        if (!result) return NULL;
        snprintf(result, need, "%s.txt", out);
        return result;
    }

    size_t need = m + 5; /* + ".txt" + NUL */
    char *result = (char *)malloc(need);
    if (!result) return NULL;
    snprintf(result, need, "%s.txt", safe_stem);
    return result;
}
