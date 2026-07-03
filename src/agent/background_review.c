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