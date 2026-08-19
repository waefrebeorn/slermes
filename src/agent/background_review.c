/*
 * background_review.c — Port of Python agent/background_review.py
 *
 * Python API -> C implementation mapping:
 *   background_review() / review_conversation()
 *       -> llm_background_review() in llm_client.c (declared in hermes_agent.h:159)
 *   summarize_background_review_actions()
 *       -> summarize_background_review_actions() in hermes_agent.h:167
 *
 * All ported functions already exist with their proper C names in
 * llm_client.c / hermes_agent.h. This file is name-parity only.
 * Background review is called from run_conversation in conversation_loop.c
 * when state->enable_background_review is true.
 *
 * Key signatures in hermes_agent.h:
 *   char *llm_background_review(llm_config_t *cfg, const char *tool_name,
 *                               const char *tool_args, const char *tool_result);
 *   char *summarize_background_review_actions(const char *review_messages_json,
 *                                             const char *prior_snapshot_json);
 */

#include "hermes_agent.h"
#include "hermes_json.h"
#include <string.h>
#include <stdlib.h>

/* PoP: bg_review_msg_text @ agent/background_review.py:_msg_text */
char *bg_review_msg_text(const char *msg_json)
{
    if (!msg_json || !*msg_json) return strdup("");

    json_t *root = json_parse(msg_json, NULL);
    if (!root) return strdup("");

    json_t *content = json_object_get(root, "content");
    if (!content) {
        json_free(root);
        return strdup("");
    }

    char *result = NULL;
    if (content->type == JSON_STRING) {
        const char *text = json_node_get_string(content);
        if (text) {
            while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') text++;
            size_t len = strlen(text);
            while (len > 0 && (text[len-1] == ' ' || text[len-1] == '\t' ||
                               text[len-1] == '\n' || text[len-1] == '\r')) len--;
            result = strndup(text, len);
        }
    } else if (content->type == JSON_ARRAY) {
        size_t total = 0;
        size_t cap = 256;
        result = malloc(cap);
        if (result) result[0] = '\0';

        size_t idx = 0;
        json_t *item;
        while (result && (item = json_array_get(content, idx++)) != NULL) {
            json_t *text_node = json_object_get(item, "text");
            if (text_node && text_node->type == JSON_STRING) {
                const char *part = json_node_get_string(text_node);
                if (part) {
                    size_t plen = strlen(part);
                    if (total + plen + 2 > cap) {
                        cap = (total + plen + 2) * 2;
                        char *tmp = realloc(result, cap);
                        if (!tmp) { free(result); result = NULL; break; }
                        result = tmp;
                    }
                    if (total > 0) { result[total++] = ' '; }
                    memcpy(result + total, part, plen);
                    total += plen;
                    result[total] = '\0';
                }
            }
        }
    }

    json_free(root);
    if (!result) return strdup("");
    return result;
}

/* PoP: bg_review_digest_history @ agent/background_review.py:_digest_history */
char *bg_review_digest_history(const char *messages_json, int tail)
{
    if (!messages_json || !*messages_json) return strdup("[]");

    json_t *msgs = json_parse(messages_json, NULL);
    if (!msgs || msgs->type != JSON_ARRAY) {
        if (msgs) json_free(msgs);
        return strdup("[]");
    }

    /* Count elements */
    size_t count = 0;
    while (json_array_get(msgs, count) != NULL) count++;

    if (count <= (size_t)tail || tail <= 0) {
        char *result = json_serialize(msgs);
        json_free(msgs);
        return result ? result : strdup("[]");
    }

    size_t keep_start = count - (size_t)tail;
    json_t *keep = json_array();
    if (!keep) { json_free(msgs); return strdup("[]"); }

    for (size_t i = keep_start; i < count; i++) {
        json_t *msg = json_array_get(msgs, i);
        if (msg) json_array_append(keep, json_copy(msg));
    }

    /* Check if first kept message is tool role — if so, expand window */
    json_t *first = json_array_get(keep, 0);
    if (first) {
        json_t *role = json_object_get(first, "role");
        if (role && role->type == JSON_STRING) {
            const char *r = json_node_get_string(role);
            if (r && strcmp(r, "tool") == 0 && keep_start > 0) {
                keep_start--;
                json_free(keep);
                keep = json_array();
                if (!keep) { json_free(msgs); return strdup("[]"); }
                for (size_t i = keep_start; i < count; i++) {
                    json_t *msg = json_array_get(msgs, i);
                    if (msg) json_array_append(keep, json_copy(msg));
                }
            }
        }
    }

    char *result = json_serialize(keep);
    json_free(keep);
    json_free(msgs);
    return result ? result : strdup("[]");
}

/* === merged from port_agent_background_review.c === */
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
