/* Trajectory saving utilities and static helpers.
 *
 * Python equivalent: agent/trajectory.py
 *
 * Provides:
 *   - convert_scratchpad_to_think() — tag normalization
 *   - has_incomplete_scratchpad()   — unclosed tag detection
 *   - save_trajectory()             — JSONL append
 */

#include "hermes_trajectory.h"
#include "../lib/libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/*  Scratchpad tag helpers                                            */
/* ------------------------------------------------------------------ */

/* Port of Python agent/trajectory.py:convert_scratchpad_to_think(). */
char *convert_scratchpad_to_think(const char *content)
{
    if (!content)
        return NULL;

    /* Quick check: does it contain the tag? */
    if (!strstr(content, "<REASONING_SCRATCHPAD>"))
        return strdup(content);

    /* Allocate generous buffer (worst case: same length if no match) */
    size_t len = strlen(content);
    char *result = malloc(len + 1);
    if (!result)
        return NULL;

    /* Walk through content, replacing tags char-by-char.
     * Each replacement is exactly the same length, so result
     * length is identical to input length. */
    const char *src = content;
    char *dst = result;
    while (*src) {
        if (strncmp(src, "<REASONING_SCRATCHPAD>", 22) == 0) {
            memcpy(dst, "<think>", 7);
            dst += 7;
            src += 22;
        } else if (strncmp(src, "</REASONING_SCRATCHPAD>", 23) == 0) {
            memcpy(dst, "</think>", 8);
            dst += 8;
            src += 23;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    return result;
}

/* Port of Python agent/trajectory.py:has_incomplete_scratchpad(). */
bool has_incomplete_scratchpad(const char *content)
{
    if (!content)
        return false;
    return strstr(content, "<REASONING_SCRATCHPAD>") != NULL
        && strstr(content, "</REASONING_SCRATCHPAD>") == NULL;
}

/* ------------------------------------------------------------------ */
/*  Trajectory saving to JSONL                                        */
/* ------------------------------------------------------------------ */

/* Port of Python agent/trajectory.py:save_trajectory(). */
int save_trajectory(const char *trajectory_json,
                            const char *model,
                            bool completed,
                            const char *filename)
{
    /* Default filename */
    char default_fn[256];
    if (!filename) {
        const char *base = completed
            ? "trajectory_samples.jsonl"
            : "failed_trajectories.jsonl";
        snprintf(default_fn, sizeof(default_fn), "%s", base);
        filename = default_fn;
    }

    /* Build ISO timestamp */
    char ts[64];
    {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        if (tm)
            strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tm);
        else
            snprintf(ts, sizeof(ts), "unknown");
    }

    /* Build JSON entry: {"conversations":...,"timestamp":...,"model":...,"completed":...} */
    json_t *root = json_object();
    if (!root) return -1;

    /* conversations — parse the passed JSON array string */
    json_t *conv = json_parse(trajectory_json, NULL);
    if (!conv || conv->type != JSON_ARRAY) {
        json_free(conv);
        json_free(root);
        return -1;
    }
    json_set(root, "conversations", conv);
    /* conv is now owned by root — json_free(root) handles it */

    /* timestamp */
    json_set(root, "timestamp", json_string(ts));

    /* model */
    json_set(root, "model", json_string(model ? model : ""));

    /* completed */
    json_set(root, "completed", json_bool(completed));

    /* Serialize to JSON string */
    char *json_str = json_serialize(root);
    json_free(root);

    if (!json_str)
        return -1;

    /* Append to file */
    FILE *f = fopen(filename, "a");
    if (!f) {
        free(json_str);
        return -1;
    }
    fprintf(f, "%s\n", json_str);
    fclose(f);

    free(json_str);
    return 0;
}
/* Port of Python: convert_to_trajectory_format */

/* ------------------------------------------------------------------ */
/*  Convert messages array to trajectory format                        */
/* ------------------------------------------------------------------ */
/* Port of Python agent/trajectory.py convert_to_trajectory_format().
 * Takes an array of OpenAI-format message dicts and returns a JSON
 * string in the trajectory training format:
 *   { "messages": [ ... sanitized messages ... ] }
 *
 * Sanitization steps per message:
 *   - Strip <REASONING_SCRATCHPAD> tags (convert to <think>)
 *   - Remove tool_call_id from assistant messages (not needed for training)
 *   - Ensure content is a plain string (join text parts if array)
 *
 * Returns malloc'd JSON string; caller must free. Returns NULL on error. */
char *convert_to_trajectory_format(const message_t *messages, int count) {
    if (!messages || count <= 0) return NULL;

    json_t *root = json_object();
    if (!root) return NULL;

    json_t *msg_arr = json_array();
    if (!msg_arr) { json_free(root); return NULL; }

    for (int i = 0; i < count; i++) {
        const message_t *msg = &messages[i];
        json_t *entry = json_object();
        if (!entry) continue;

        /* Role */
        const char *role_str = "unknown";
        switch (msg->role) {
            case MSG_SYSTEM:    role_str = "system"; break;
            case MSG_USER:      role_str = "user"; break;
            case MSG_ASSISTANT: role_str = "assistant"; break;
            case MSG_TOOL:      role_str = "tool"; break;
        }
        json_set(entry, "role", json_string(role_str));

        /* Content — convert scratchpad tags, ensure string */
        if (msg->content) {
            char *converted = convert_scratchpad_to_think(msg->content);
            json_set(entry, "content", json_string(converted ? converted : msg->content));
            if (converted) free(converted);
        }

        /* Tool calls (assistant messages) */
        if (msg->role == MSG_ASSISTANT && msg->tool_calls_count > 0) {
            json_t *tc_arr = json_array();
            if (tc_arr) {
                for (int j = 0; j < msg->tool_calls_count && j < 64; j++) {
                    json_t *tc = json_object();
                    if (!tc) continue;
                    json_set(tc, "id", json_string(msg->tool_calls[j].id));

                    json_t *fn = json_object();
                    if (fn) {
                        json_set(fn, "name", json_string(msg->tool_calls[j].name));
                        json_set(fn, "arguments", json_string(msg->tool_calls[j].arguments));
                        json_set(tc, "function", fn);
                    }
                    json_set(tc, "type", json_string("function"));
                    json_array_append(tc_arr, tc);
                }
                json_set(entry, "tool_calls", tc_arr);
            }
        }

        /* Tool call ID (tool result messages) */
        if (msg->role == MSG_TOOL && msg->tool_call_id) {
            json_set(entry, "tool_call_id", json_string(msg->tool_call_id));
        }

        /* Name field (tool result messages) */
        if (msg->role == MSG_TOOL && msg->tool_name) {
            json_set(entry, "name", json_string(msg->tool_name));
        }

        json_array_append(msg_arr, entry);
    }

    json_set(root, "messages", msg_arr);

    char *result = json_serialize(root);
    json_free(root);
    return result;
}
