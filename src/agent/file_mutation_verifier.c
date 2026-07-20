/*
 * file_mutation_verifier.c — Port of Python run_agent.AIAgent
 *   _record_file_mutation_result() + _format_file_mutation_failure_footer()
 * MIT License — WuBu Slermes Project
 *
 * Per-turn file write verification. Tracks write_file/patch outcomes and
 * renders a failure footer at turn end. (GAP 2 from the original
 * hermes_gap_fixes.c monolith — split into a self-contained module.)
 */

#include "file_mutation_verifier.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void file_mutation_tracker_init(file_mutation_tracker_t *tracker) {
    if (tracker)
        memset(tracker, 0, sizeof(*tracker));
}

/* Helper: extract file paths from tool arguments JSON */
static int extract_mutation_paths(const char *args_json, char paths[][MUTATION_PATH_MAX], int max_paths) {
    if (!args_json || !*args_json) return 0;

    json_t *root = json_parse(args_json, NULL);
    if (!root) return 0;

    int count = 0;

    json_t *path_val = json_object_get(root, "path");
    if (path_val && path_val->type == JSON_STRING && path_val->str_val) {
        snprintf(paths[count], MUTATION_PATH_MAX, "%s", path_val->str_val);
        count++;
    }

    json_t *fp_val = json_object_get(root, "file_path");
    if (fp_val && fp_val->type == JSON_STRING && fp_val->str_val && count < max_paths) {
        snprintf(paths[count], MUTATION_PATH_MAX, "%s", fp_val->str_val);
        count++;
    }

    json_t *paths_arr = json_object_get(root, "paths");
    if (paths_arr && paths_arr->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_count(paths_arr) && count < max_paths; i++) {
            json_t *elem = json_array_get(paths_arr, i);
            if (elem && elem->type == JSON_STRING && elem->str_val) {
                snprintf(paths[count], MUTATION_PATH_MAX, "%s", elem->str_val);
                count++;
            }
        }
    }

    json_free(root);
    return count;
}

static bool is_file_mutating_tool(const char *tool_name) {
    if (!tool_name) return false;
    static const char *tools[] = {"write_file", "patch", "edit_file", "append_file",
                                   "delete_file", "create_file", NULL};
    for (int i = 0; tools[i]; i++) {
        if (strcmp(tool_name, tools[i]) == 0)
            return true;
    }
    return false;
}

static void extract_error_preview(const char *result, char *out, size_t out_sz) {
    out[0] = '\0';
    if (!result || !*result) {
        return;
    }

    json_t *root = json_parse(result, NULL);
    if (root && root->type == JSON_OBJECT) {
        json_t *err_val = json_object_get(root, "error");
        if (err_val && err_val->type == JSON_STRING && err_val->str_val && err_val->str_val[0]) {
            snprintf(out, out_sz, "%.*s", (int)(out_sz - 1), err_val->str_val);
            json_free(root);
            return;
        }
        json_free(root);
    } else if (root) {
        json_free(root);
    }

    const char *nl = strchr(result, '\n');
    size_t len = nl ? (size_t)(nl - result) : strlen(result);
    if (len > out_sz - 1) len = out_sz - 1;
    memcpy(out, result, len);
    out[len] = '\0';
}

void file_mutation_tracker_record(file_mutation_tracker_t *tracker,
                                  const char *tool_name,
                                  const char *args_json,
                                  const char *result,
                                  bool is_error) {
    if (!tracker || !tool_name || !is_file_mutating_tool(tool_name))
        return;

    char paths[MAX_FILE_MUTATIONS][MUTATION_PATH_MAX];
    int path_count = extract_mutation_paths(args_json, paths, MAX_FILE_MUTATIONS);
    if (path_count == 0) return;

    char preview[MUTATION_PREVIEW_MAX] = "";
    if (is_error)
        extract_error_preview(result, preview, sizeof(preview));

    for (int p = 0; p < path_count && tracker->count < MAX_FILE_MUTATIONS; p++) {
        int existing = -1;
        for (int i = 0; i < tracker->count; i++) {
            if (strcmp(tracker->entries[i].path, paths[p]) == 0) {
                existing = i;
                break;
            }
        }

        if (existing >= 0) {
            if (!is_error) {
                memmove(&tracker->entries[existing], &tracker->entries[existing + 1],
                        (size_t)(tracker->count - existing - 1) * sizeof(file_mutation_entry_t));
                tracker->count--;
            }
        } else if (is_error) {
            file_mutation_entry_t *e = &tracker->entries[tracker->count++];
            snprintf(e->path, sizeof(e->path), "%s", paths[p]);
            snprintf(e->tool, sizeof(e->tool), "%s", tool_name);
            snprintf(e->error_preview, sizeof(e->error_preview), "%s", preview);
            e->is_error = true;
        }
    }
}

char *file_mutation_tracker_format_footer(const file_mutation_tracker_t *tracker) {
    if (!tracker || tracker->count == 0)
        return strdup("");

    int error_count = 0;
    for (int i = 0; i < tracker->count; i++) {
        if (tracker->entries[i].is_error)
            error_count++;
    }

    if (error_count == 0)
        return strdup("");

    size_t buf_sz = 256 + (size_t)error_count * (MUTATION_PATH_MAX + MUTATION_PREVIEW_MAX + 32);
    char *buf = (char *)malloc(buf_sz);
    if (!buf) return strdup("");

    int pos = snprintf(buf, buf_sz,
        "File-mutation verifier: %d file(s) were not modified this turn. "
        "Run `git status` or `read_file` to confirm.",
        error_count);

    int shown = 0;
    for (int i = 0; i < tracker->count && shown < 10; i++) {
        if (!tracker->entries[i].is_error) continue;
        const char *path = tracker->entries[i].path;
        const char *tool = tracker->entries[i].tool;
        const char *preview = tracker->entries[i].error_preview;

        if (preview && preview[0]) {
            pos += snprintf(buf + pos, buf_sz - (size_t)pos,
                "\n  - `%s`  [%s] %s", path, tool, preview);
        } else {
            pos += snprintf(buf + pos, buf_sz - (size_t)pos,
                "\n  - `%s`  [%s] failed", path, tool);
        }
        shown++;
    }

    if (error_count > 10) {
        snprintf(buf + pos, buf_sz - (size_t)pos, "\n  - ... and %d more", error_count - 10);
    }

    return buf;
}
