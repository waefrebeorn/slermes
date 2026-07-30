/*
 * port_agent_background_review.c — Port of Python agent/background_review.py
 *
 * Background memory/skill review — fork the agent to evaluate the turn.
 * Uses hermes_json.h compat macros (maps old API to libjson).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* Port of Python: _msg_text */
const char *msg_text(const json_t *m) {
    if (!m) return "";

    json_t *content = json_object_get(m, "content");
    if (!content) return "";

    if (json_node_is_string(content)) {
        const char *str = json_node_get_string(content);
        if (!str) return "";

        /* Simple trim - strip leading/trailing whitespace */
        while (*str && isspace(*str)) str++;
        if (!*str) return "";

        /* Find end */
        const char *end = str + strlen(str) - 1;
        while (end > str && isspace(*end)) end--;

        /* Return trimmed string */
        static char trimmed[4096];
        size_t len = (size_t)(end - str + 1);
        if (len >= sizeof(trimmed)) len = sizeof(trimmed) - 1;
        memcpy(trimmed, str, len);
        trimmed[len] = '\0';
        return trimmed;
    }

    if (json_node_is_array(content)) {
        /* Join text parts from array */
        static char buffer[4096];
        size_t pos = 0;
        size_t len = json_array_count(content);
        for (size_t i = 0; i < len && pos < sizeof(buffer) - 2; i++) {
            json_t *item = json_array_get(content, i);
            if (item && json_node_is_object(item)) {
                json_t *text = json_object_get(item, "text");
                if (json_node_is_string(text)) {
                    const char *t = json_node_get_string(text);
                    if (t && *t) {
                        if (pos > 0) buffer[pos++] = ' ';
                        size_t copy_len = strlen(t);
                        if (pos + copy_len >= sizeof(buffer) - 1)
                            copy_len = sizeof(buffer) - 1 - pos;
                        memcpy(buffer + pos, t, copy_len);
                        pos += copy_len;
                    }
                }
            }
        }
        buffer[pos] = '\0';
        return buffer;
    }

    return "";
}

/* Port of Python: _digest_history */
json_t *digest_history(const json_t *messages_snapshot, int tail) {
    if (!messages_snapshot || !json_node_is_array(messages_snapshot)) {
        return json_array();
    }

    size_t total = json_array_count(messages_snapshot);
    if ((size_t)tail >= total) {
        return json_copy(messages_snapshot);
    }

    /* Keep the last 'tail' messages */
    size_t keep_start = total - tail;
    json_t *keep = json_array();
    for (size_t i = keep_start; i < total; i++) {
        json_t *msg = json_array_get(messages_snapshot, i);
        json_array_append(keep, json_copy(msg));
    }

    /* Ensure we don't start with tool messages */
    while (json_array_count(keep) > 0) {
        json_t *first = json_array_get(keep, 0);
        json_t *role = json_object_get(first, "role");
        if (json_node_is_string(role)) {
            const char *rstr = json_node_get_string(role);
            if (rstr && strcmp(rstr, "tool") == 0) {
                tail++;
                if (tail >= (int)total) {
                    json_free(keep);
                    return json_copy(messages_snapshot);
                }
                keep_start = total - tail;
                json_free(keep);
                keep = json_array();
                for (size_t i = keep_start; i < total; i++) {
                    json_t *msg = json_array_get(messages_snapshot, i);
                    json_array_append(keep, json_copy(msg));
                }
            } else {
                break;
            }
        } else {
            break;
        }
    }

    /* Build digest from older messages */
    size_t old_count = keep_start;
    json_t *lines = json_array();
    for (size_t i = 0; i < old_count; i++) {
        json_t *m = json_array_get(messages_snapshot, i);
        if (!m || !json_node_is_object(m)) continue;

        json_t *role = json_object_get(m, "role");
        if (!json_node_is_string(role)) continue;

        const char *role_str = json_node_get_string(role);
        const char *text = msg_text(m);
        if (!text || !*text) continue;

        char buffer[512];
        if (strcmp(role_str, "user") == 0) {
            snprintf(buffer, sizeof(buffer), "USER: %.300s", text);
            json_array_append(lines, json_string(buffer));
        } else if (strcmp(role_str, "assistant") == 0) {
            json_t *tool_calls = json_object_get(m, "tool_calls");
            if (tool_calls && json_node_is_array(tool_calls) && json_array_count(tool_calls) > 0) {
                /* Extract tool names */
                char tools_buffer[1024];
                size_t tpos = 0;
                size_t tc_len = json_array_count(tool_calls);
                for (size_t j = 0; j < tc_len; j++) {
                    json_t *tc = json_array_get(tool_calls, j);
                    json_t *func = json_object_get(tc, "function");
                    if (func && json_node_is_object(func)) {
                        json_t *name = json_object_get(func, "name");
                        if (json_node_is_string(name)) {
                            const char *n = json_node_get_string(name);
                            if (tpos > 0) {
                                if (tpos + 2 >= sizeof(tools_buffer)) break;
                                tools_buffer[tpos++] = ',';
                                tools_buffer[tpos++] = ' ';
                            }
                            size_t nlen = strlen(n);
                            if (tpos + nlen >= sizeof(tools_buffer) - 1)
                                nlen = sizeof(tools_buffer) - 1 - tpos;
                            memcpy(tools_buffer + tpos, n, nlen);
                            tpos += nlen;
                        }
                    }
                }
                tools_buffer[tpos] = '\0';
                snprintf(buffer, sizeof(buffer), "ASSISTANT[tools: %s]", tools_buffer);
                json_array_append(lines, json_string(buffer));
            }
            if (text && *text) {
                snprintf(buffer, sizeof(buffer), "ASSISTANT: %.200s", text);
                json_array_append(lines, json_string(buffer));
            }
        }
    }

    /* Build digest message — serialize lines into a single newline-separated string */
    char digest_content[4096];
    size_t dpos = 0;
    size_t lcount = json_array_count(lines);
    for (size_t i = 0; i < lcount && dpos < sizeof(digest_content) - 2; i++) {
        json_t *line = json_array_get(lines, i);
        if (json_node_is_string(line)) {
            const char *s = json_node_get_string(line);
            if (s) {
                size_t slen = strlen(s);
                if (dpos + slen + 2 > sizeof(digest_content) - 1)
                    slen = sizeof(digest_content) - 1 - dpos - 2;
                memcpy(digest_content + dpos, s, slen);
                dpos += slen;
            }
        }
        if (dpos < sizeof(digest_content) - 2)
            digest_content[dpos++] = '\n';
    }
    digest_content[dpos] = '\0';

    json_t *digest = json_object();
    json_object_set(digest, "role", json_string("user"));

    char content_buf[4096];
    snprintf(content_buf, sizeof(content_buf),
             "[Earlier conversation digest — older turns summarised to bound the "
             "review's cold-write cost on the routed aux model. Recent turns "
             "follow verbatim below.]\n%s",
             dpos > 0 ? digest_content : "");
    json_object_set(digest, "content", json_string(content_buf));

    /* Combine digest + kept messages */
    json_t *result = json_array();
    json_array_append(result, digest);
    size_t kcount = json_array_count(keep);
    for (size_t i = 0; i < kcount; i++) {
        json_array_append(result, json_copy(json_array_get(keep, i)));
    }

    json_free(keep);
    json_free(lines);

    return result;
}

/* Review prompts */
const char *memory_review_prompt =
    "Review the conversation above and consider saving to memory if appropriate.\n\n"
    "Focus on:\n"
    "1. Has the user revealed things about themselves — their persona, desires, "
    "preferences, or personal details worth remembering?\n"
    "2. Has the user expressed expectations about how you should behave, their work "
    "style, or ways they want you to operate?\n\n"
    "If something stands out, save it using the memory tool. "
    "If nothing is worth saving, just say 'Nothing to save.' and stop.";

const char *skill_review_prompt =
    "Review the conversation above and update the skill library. Be "
    "ACTIVE — most sessions produce at least one skill update, even if "
    "small. A pass that does nothing is a missed learning opportunity, "
    "not a neutral outcome.\n\n"
    "Target shape of the library: CLASS-LEVEL skills, each with a rich "
    "SKILL.md and a `references/` directory for session-specific detail. "
    "Not a long flat list of narrow one-session-one-skill entries.";

const char *resolve_review_runtime(const json_t *parent_runtime, const json_t *config) {
    static char result[4096];
    (void)config;
    if (parent_runtime) {
        char *serialized = json_serialize(parent_runtime);
        if (serialized) {
            size_t slen = strlen(serialized);
            if (slen >= sizeof(result)) slen = sizeof(result) - 1;
            memcpy(result, serialized, slen);
            result[slen] = '\0';
            json_free((json_t *)serialized);
        } else {
            result[0] = '\0';
        }
        return result;
    }
    return "{}";
}