/*
 * port_agent_background_review.c — Port of Python agent/background_review.py
 *
 * Background memory/skill review — fork the agent to evaluate the turn.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <json.h>

/* Port of Python: _msg_text */
const char *msg_text(const json_t *m) {
    if (!m) return "";
    
    json_t *content = json_object_get(m, "content");
    if (!content) return "";
    
    if (json_is_string(content)) {
        const char *str = json_string_value(content);
        if (!str) return "";
        
        /* Simple trim - strip leading/trailing whitespace */
        while (*str && isspace(*str)) str++;
        if (!*str) return "";
        
        /* Find end */
        const char *end = str + strlen(str) - 1;
        while (end > str && isspace(*end)) end--;
        end[1] = '\0';
        
        return str;
    }
    
    if (json_is_array(content)) {
        /* Join text parts from array */
        static char buffer[4096];
        size_t pos = 0;
        size_t len = json_array_size(content);
        for (size_t i = 0; i < len && pos < sizeof(buffer) - 2; i++) {
            json_t *item = json_array_get(content, i);
            if (json_is_object(item)) {
                json_t *text = json_object_get(item, "text");
                if (json_is_string(text)) {
                    const char *t = json_string_value(text);
                    if (t && *t) {
                        if (pos > 0) buffer[pos++] = ' ';
                        size_t copy_len = strlen(t);
                        if (pos + copy_len >= sizeof(buffer) - 1) copy_len = sizeof(buffer) - 1 - pos;
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
    if (!messages_snapshot || !json_is_array(messages_snapshot)) {
        return json_array();
    }
    
    size_t total = json_array_size(messages_snapshot);
    if ((size_t)tail >= total) {
        return json_copy((json_t *)messages_snapshot);
    }
    
    /* Keep the last 'tail' messages */
    size_t keep_start = total - tail;
    json_t *keep = json_array();
    for (size_t i = keep_start; i < total; i++) {
        json_t *msg = json_array_get(messages_snapshot, i);
        json_array_append(keep, json_copy(msg));
    }
    
    /* Ensure we don't start with tool messages */
    while (json_array_size(keep) > 0) {
        json_t *first = json_array_get(keep, 0);
        json_t *role = json_object_get(first, "role");
        if (json_is_string(role) && strcmp(json_string_value(role), "tool") == 0) {
            tail++;
            if (tail >= (int)total) {
                json_decref(keep);
                return json_copy((json_t *)messages_snapshot);
            }
            keep_start = total - tail;
            json_decref(keep);
            keep = json_array();
            for (size_t i = keep_start; i < total; i++) {
                json_t *msg = json_array_get(messages_snapshot, i);
                json_array_append(keep, json_copy(msg));
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
        if (!json_is_object(m)) continue;
        
        json_t *role = json_object_get(m, "role");
        if (!json_is_string(role)) continue;
        
        const char *role_str = json_string_value(role);
        const char *text = msg_text(m);
        if (!text || !*text) continue;
        
        char buffer[512];
        if (strcmp(role_str, "user") == 0) {
            snprintf(buffer, sizeof(buffer), "USER: %.300s", text);
            json_array_append(lines, json_string(buffer));
        } else if (strcmp(role_str, "assistant") == 0) {
            json_t *tool_calls = json_object_get(m, "tool_calls");
            if (json_is_array(tool_calls) && json_array_size(tool_calls) > 0) {
                /* Extract tool names */
                char tools_buffer[1024];
                size_t tpos = 0;
                size_t tc_len = json_array_size(tool_calls);
                for (size_t j = 0; j < tc_len; j++) {
                    json_t *tc = json_array_get(tool_calls, j);
                    json_t *func = json_object_get(tc, "function");
                    if (json_is_object(func)) {
                        json_t *name = json_object_get(func, "name");
                        if (json_is_string(name)) {
                            const char *n = json_string_value(name);
                            if (tpos > 0) {
                                if (tpos + 2 >= sizeof(tools_buffer)) break;
                                tools_buffer[tpos++] = ',';
                                tools_buffer[tpos++] = ' ';
                            }
                            size_t nlen = strlen(n);
                            if (tpos + nlen >= sizeof(tools_buffer) - 1) nlen = sizeof(tools_buffer) - 1 - tpos;
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
    
    /* Build digest message */
    char *lines_joined = json_string_value(json_array_to_string(lines, "\n"));
    json_t *digest = json_object();
    json_object_set(digest, "role", json_string("user"));
    
    char digest_content[4096];
    snprintf(digest_content, sizeof(digest_content),
             "[Earlier conversation digest — older turns summarised to bound the "
             "review's cold-write cost on the routed aux model. Recent turns "
             "follow verbatim below.]\n%s",
             lines_joined ? lines_joined : "");
    json_object_set(digest, "content", json_string(digest_content));
    
    /* Combine digest + kept messages */
    json_t *result = json_array();
    json_array_append(result, digest);
    for (size_t i = 0; i < json_array_size(keep); i++) {
        json_array_append(result, json_copy(json_array_get(keep, i)));
    }
    
    json_decref(keep);
    json_decref(lines);
    
    return result;
}

/* Review prompts - used by spawn_background_review_thread */
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
    "Not a long flat list of narrow one-session-one-skill entries. This "
    "shapes HOW you update, not WHETHER you update.\n\n"
    "Signals to look for (any one of these warrants action):\n"
    "  • User corrected your style, tone, format, legibility, or "
    "verbosity. Frustration signals like 'stop doing X', 'this is too "
    "verbose', 'don't format like this', 'why are you explaining', "
    "'just give me the answer', 'you always do Y and I hate it', or an "
    "explicit 'remember this' are FIRST-CLASS skill signals, not just "
    "memory signals. Update the relevant skill(s) to embed the "
    "preference so the next session starts already knowing.\n"
    "  • User corrected your workflow, approach, or sequence of steps. "
    "Encode the correction as a pitfall or explicit step in the skill "
    "that governs that class of task.\n"
    "  • Non-trivial technique, fix, workaround, debugging path, or "
    "tool-usage pattern emerged that a future session would benefit "
    "from. Capture it.\n"
    "  • A skill that got loaded or consulted this session turned out "
    "to be wrong, missing a step, or outdated. Patch it NOW.\n\n"
    "Preference order — prefer the earliest action that fits, but do "
    "pick one when a signal above fired:\n"
    "  1. UPDATE A CURRENTLY-LOADED SKILL. Look back through the "
    "conversation for skills the user loaded via /skill-name or you "
    "read via skill_view. If any of them covers the territory of the "
    "new learning, PATCH that one first. It is the skill that was in "
    "the conversation.\n";

const char *resolve_review_runtime(const json_t *parent_runtime, const json_t *config) {
    /* Simplified: just return parent runtime.
     * Full implementation requires hermes_cli.config and hermes_cli.runtime_provider.
     */
    static char result[4096];
    if (parent_runtime) {
        json_dump(parent_runtime, result, sizeof(result));
        return result;
    }
    return "{}";
}