/*
 * context.c — Message context management for Hermes C.
 * Manages the array of messages in agent_state_t.
 * Provides: push, pop, get, truncate, eviction strategies, system prompt injection.
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Token estimation (rough: ~4 chars per token)
 * ================================================================ */

/* PoP: cli_hermes_cli_context_switch_guard__estimate_tokens @ hermes_cli/context_switch_guard.py:_estimate_tokens */
static int estimate_tokens(const char *text) {
    if (!text) return 0;
    size_t len = strlen(text);
    return (int)(len / 4) + 1; /* rough estimate */
}

/* ================================================================
 *  Message lifecycle
 * ================================================================ */

message_t *message_new(message_role_t role, const char *content) {
    message_t *msg = (message_t *)calloc(1, sizeof(message_t));
    if (!msg) return NULL;
    msg->role = role;
    msg->content = content ? strdup(content) : NULL;
    msg->tool_call_id = NULL;
    msg->tool_name = NULL;
    msg->reasoning = NULL;
    msg->encrypted_content = NULL;
    return msg;
}

message_t *message_new_tool(const char *tool_call_id, const char *content) {
    message_t *msg = message_new(MSG_TOOL, content);
    if (msg && tool_call_id)
        msg->tool_call_id = strdup(tool_call_id);
    return msg;
}

message_t *message_new_assistant(const char *content, const char *tool_name,
                                  const char *tool_call_id, const char *reasoning,
                                  const char *encrypted_content)
{
    message_t *msg = message_new(MSG_ASSISTANT, content);
    if (msg) {
        if (tool_name) msg->tool_name = strdup(tool_name);
        if (tool_call_id) msg->tool_call_id = strdup(tool_call_id);
        if (reasoning) msg->reasoning = strdup(reasoning);
        if (encrypted_content) msg->encrypted_content = strdup(encrypted_content);
        msg->tool_calls_count = 0;
    }
    return msg;
}

/* Create assistant message with tool calls (from LLM response) */
message_t *message_new_assistant_with_toolcalls(const char *content,
                                                  const tool_call_t *tcalls,
                                                  int tcalls_count,
                                                  const char *reasoning,
                                                  const char *encrypted_content)
{
    message_t *msg = message_new(MSG_ASSISTANT, content);
    if (!msg) return NULL;
    if (reasoning) msg->reasoning = strdup(reasoning);
    if (encrypted_content) msg->encrypted_content = strdup(encrypted_content);
    msg->tool_calls_count = tcalls_count > 64 ? 64 : tcalls_count;
    for (int i = 0; i < msg->tool_calls_count; i++) {
        memcpy(&msg->tool_calls[i], &tcalls[i], sizeof(tool_call_t));
    }
    return msg;
}

void message_free(message_t *msg) {
    if (!msg) return;
    free(msg->content);
    free(msg->tool_call_id);
    free(msg->tool_name);
    free(msg->reasoning);
    free(msg->encrypted_content);
    free(msg);
}

/* Clone a message (deep copy) */
message_t *message_clone(const message_t *src) {
    if (!src) return NULL;
    message_t *dst = (message_t *)calloc(1, sizeof(message_t));
    if (!dst) return NULL;
    dst->role = src->role;
    dst->content = src->content ? strdup(src->content) : NULL;
    dst->tool_call_id = src->tool_call_id ? strdup(src->tool_call_id) : NULL;
    dst->tool_name = src->tool_name ? strdup(src->tool_name) : NULL;
    dst->reasoning = src->reasoning ? strdup(src->reasoning) : NULL;
    dst->encrypted_content = src->encrypted_content ? strdup(src->encrypted_content) : NULL;
    dst->tool_calls_count = src->tool_calls_count;
    for (int i = 0; i < src->tool_calls_count && i < 64; i++) {
        memcpy(&dst->tool_calls[i], &src->tool_calls[i], sizeof(tool_call_t));
    }
    return dst;
}

/* ================================================================
 *  Context operations
 * ================================================================ */

void context_init(agent_state_t *state) {
    /* messages/capacity set by init_agent, not here */
    state->message_count = 0;
}

bool context_push(agent_state_t *state, message_t *msg) {
    if (!state || !msg) return false;

    if (state->message_count >= state->message_capacity) {
        size_t new_cap = state->message_capacity == 0 ? 64 : state->message_capacity * 2;
        if (new_cap > HERMES_MAX_MESSAGES) new_cap = HERMES_MAX_MESSAGES;
        message_t **new_msgs = (message_t **)realloc(
            state->messages, new_cap * sizeof(message_t *));
        if (!new_msgs) return false;
        state->messages = new_msgs;
        state->message_capacity = new_cap;
    }

    state->messages[state->message_count++] = msg;
    return true;
}

message_t *context_pop(agent_state_t *state) {
    if (!state || state->message_count == 0) return NULL;
    return state->messages[--state->message_count];
}

void context_clear(agent_state_t *state) {
    if (!state || !state->messages) return;
    for (size_t i = 0; i < state->message_count; i++)
        message_free(state->messages[i]);
    free(state->messages);
    state->messages = NULL;
    state->message_count = 0;
    state->message_capacity = 0;
}

bool context_set_system(agent_state_t *state, const char *content) {
    /* Insert or update first system message */
    for (size_t i = 0; i < state->message_count; i++) {
        if (state->messages[i]->role == MSG_SYSTEM) {
            free(state->messages[i]->content);
            state->messages[i]->content = content ? strdup(content) : NULL;
            return true;
        }
    }
    /* No system message — insert at front */
    message_t *sys = message_new(MSG_SYSTEM, content);
    if (!sys) return false;

    /* Shift all messages right by one */
    size_t new_count = state->message_count + 1;
    if (new_count > state->message_capacity) {
        size_t new_cap = state->message_capacity == 0 ? 64 : state->message_capacity * 2;
        message_t **new_msgs = (message_t **)realloc(
            state->messages, new_cap * sizeof(message_t *));
        if (!new_msgs) { message_free(sys); return false; }
        state->messages = new_msgs;
        state->message_capacity = new_cap;
    }

    memmove(&state->messages[1], &state->messages[0],
            state->message_count * sizeof(message_t *));
    state->messages[0] = sys;
    state->message_count++;
    return true;
}

void context_truncate(agent_state_t *state, size_t max_messages) {
    if (!state || state->message_count <= max_messages) return;
    /* Keep system message (index 0) + most recent max_messages-1 */
    size_t keep_system = (state->message_count > 0 &&
                          state->messages[0]->role == MSG_SYSTEM) ? 1 : 0;
    size_t remove_count = state->message_count - max_messages;

    if (keep_system && remove_count > 0) {
        /* Don't remove system message */
        if (remove_count >= state->message_count - 1) {
            remove_count = state->message_count - 2;
        }
        if (remove_count > 0) {
            for (size_t i = 0; i < remove_count; i++)
                message_free(state->messages[1 + i]);
            memmove(&state->messages[1], &state->messages[1 + remove_count],
                    (state->message_count - 1 - remove_count) * sizeof(message_t *));
            state->message_count -= remove_count;
        }
    } else {
        /* Remove from front */
        for (size_t i = 0; i < remove_count; i++)
            message_free(state->messages[i]);
        memmove(&state->messages[0], &state->messages[remove_count],
                (state->message_count - remove_count) * sizeof(message_t *));
        state->message_count -= remove_count;
    }
}

const message_t *context_get(const agent_state_t *state, size_t index) {
    if (!state || index >= state->message_count) return NULL;
    return state->messages[index];
}

/* ================================================================
 *  P90: Smart context eviction
 * ================================================================ */

/* Count tokens in a message */
int context_message_tokens(const message_t *msg) {
    if (!msg) return 0;
    int t = estimate_tokens(msg->content);
    t += estimate_tokens(msg->reasoning);
    /* Tool calls add tokens */
    for (int i = 0; i < msg->tool_calls_count && i < 64; i++) {
        t += estimate_tokens(msg->tool_calls[i].name);
        t += estimate_tokens(msg->tool_calls[i].arguments);
    }
    return t;
}

/* Estimate total tokens in context */
int context_total_tokens(const agent_state_t *state) {
    if (!state) return 0;
    int total = 0;
    for (size_t i = 0; i < state->message_count; i++)
        total += context_message_tokens(state->messages[i]);
    return total;
}

/* Smart eviction: remove messages to stay within max messages, preferring
 * to drop tool results before conversation turns. Always preserves system. */
void context_evict_smart(agent_state_t *state, size_t max_messages,
                          eviction_strategy_t strategy) {
    if (!state || state->message_count <= max_messages) return;

    size_t keep_system = (state->message_count > 0 &&
                          state->messages[0]->role == MSG_SYSTEM) ? 1 : 0;
    size_t target = max_messages > keep_system ? max_messages : keep_system + 4;
    size_t remove_count = state->message_count - target;

    if (remove_count == 0) return;

    size_t start_idx = keep_system; /* where to start removing from */

    switch (strategy) {
    case EVICT_OLDEST_TOOL_FIRST: {
        /* Count non-tool messages */
        size_t non_tool = 0;
        for (size_t i = start_idx; i < state->message_count; i++)
            if (state->messages[i]->role != MSG_TOOL)
                non_tool++;

        /* If removing would delete all non-tool messages beyond N, keep more */
        size_t max_tool_remove = state->message_count - start_idx - non_tool;
        if (remove_count > max_tool_remove + (non_tool > 2 ? non_tool - 2 : 0))
            remove_count = max_tool_remove + (non_tool > 2 ? non_tool - 2 : 0);
        if (remove_count == 0) return;

        /* Remove oldest messages (prefer tool results) */
        for (size_t i = 0; i < remove_count; i++) {
            /* Find oldest non-system message */
            size_t oldest = start_idx;
            for (size_t j = start_idx + 1; j < state->message_count; j++) {
                /* Prefer tools for removal */
                if (state->messages[j]->role == MSG_TOOL &&
                    state->messages[oldest]->role != MSG_TOOL) {
                    oldest = j;
                } else if (j < oldest) {
                    oldest = j;
                }
            }
            message_free(state->messages[oldest]);
            memmove(&state->messages[oldest], &state->messages[oldest + 1],
                    (state->message_count - oldest - 1) * sizeof(message_t *));
            state->message_count--;
        }
        break;
    }
    case EVICT_OLDEST_USER: {
        /* Count user+assistant pairs */
        size_t pairs = 0;
        for (size_t i = start_idx; i < state->message_count; i++)
            if (state->messages[i]->role == MSG_USER || state->messages[i]->role == MSG_ASSISTANT)
                pairs++;
        /* Remove oldest user/assistant messages, keep the rest */
        size_t removed = 0;
        size_t i = start_idx;
        while (i < state->message_count && removed < remove_count) {
            if (state->messages[i]->role == MSG_USER ||
                state->messages[i]->role == MSG_ASSISTANT) {
                message_free(state->messages[i]);
                memmove(&state->messages[i], &state->messages[i + 1],
                        (state->message_count - i - 1) * sizeof(message_t *));
                state->message_count--;
                removed++;
            } else {
                i++;
            }
        }
        break;
    }
    case EVICT_KEEP_RECENT_N: {
        /* Simple: remove oldest, keep N most recent + system */
        context_truncate(state, target);
        break;
    }
    }
}

/* Get system prompt text (first MSG_SYSTEM content, or NULL) */
const char *context_get_system(const agent_state_t *state) {
    if (!state) return NULL;
    for (size_t i = 0; i < state->message_count; i++)
        if (state->messages[i]->role == MSG_SYSTEM)
            return state->messages[i]->content;
    return NULL;
}

/* ================================================================
 *  P97: Compression feedback tracker
 * ================================================================ */

/* Track user ratings of compression quality and adapt threshold */
void compression_feedback_init(compression_feedback_t *fb) {
    if (!fb) return;
    memset(fb, 0, sizeof(*fb));
    fb->quality_score = 0.5f;  /* start neutral */
    fb->adapt_threshold = 0.5f; /* default threshold */
}

/* Record positive feedback: user said compression was good.
 * Increases quality score and may raise threshold (compress more aggressively). */
void compression_feedback_positive(compression_feedback_t *fb) {
    if (!fb) return;
    fb->total_compressions++;
    fb->positive_feedback++;
    /* Update running quality score (EMA-like) */
    fb->quality_score = fb->quality_score * 0.7f + 1.0f * 0.3f;
    /* If compression is consistently good, raise threshold to compress more */
    if (fb->positive_feedback >= 3 && fb->negative_feedback == 0)
        fb->adapt_threshold += 0.05f;
    if (fb->adapt_threshold > 0.9f) fb->adapt_threshold = 0.9f;
}

/* Record negative feedback: user said compression was bad.
 * Decreases quality score and lowers threshold (compress less). */
void compression_feedback_negative(compression_feedback_t *fb) {
    if (!fb) return;
    fb->total_compressions++;
    fb->negative_feedback++;
    fb->quality_score = fb->quality_score * 0.7f + (-0.5f) * 0.3f;
    if (fb->quality_score < 0.0f) fb->quality_score = 0.0f;
    /* Lower threshold to compress less aggressively */
    fb->adapt_threshold -= 0.1f;
    if (fb->adapt_threshold < 0.1f) fb->adapt_threshold = 0.1f;
}

/* Get the effective compression threshold (config default * adaptive factor) */
float compression_feedback_get_threshold(const compression_feedback_t *fb, float config_threshold) {
    if (!fb) return config_threshold;
    return config_threshold * fb->adapt_threshold;
}

/* Get a brief status string for display */
void compression_feedback_status(const compression_feedback_t *fb, char *buf, size_t sz) {
    if (!fb || !buf || sz == 0) return;
    snprintf(buf, sz, "compr: %d pos, %d neg, score=%.2f, thresh=%.2f",
             fb->positive_feedback, fb->negative_feedback,
             fb->quality_score, fb->adapt_threshold);
}

/* ================================================================
 *  Image part helpers — port of Python context_compressor.py
 *  _is_image_part, _content_has_images, _strip_images_from_content,
 *  _strip_image_parts_from_parts, _strip_historical_media,
 * content_length_for_budget.
 * Port of Python agent/context_compressor.py.
 * ================================================================ */

/* Image part type constants */
static const char *IMAGE_PART_TYPES[] = {"image_url", "input_image", "image"};

/**
 * json_is_image_part — port of Python _is_image_part()
 * Returns true if a JSON content part dict represents an image.
 * Recognizes: {"type": "image_url", ...}, {"type": "input_image", ...},
 * json_is_image_part — port of Python _is_image_part()
 */
/* Port of Python: _is_image_part */
/* AG26: Port of Python agent/context_compressor.py:_is_image_part() */
bool json_is_image_part(const json_t *part) {
    if (!part || part->type != JSON_OBJECT) return false;
    const char *ptype = json_get_str(part, "type", NULL);
    if (!ptype) return false;
    for (size_t i = 0; i < sizeof(IMAGE_PART_TYPES) / sizeof(IMAGE_PART_TYPES[0]); i++) {
        if (strcmp(ptype, IMAGE_PART_TYPES[i]) == 0) return true;
    }
    return false;
}

/**
 * json_content_has_images — port of Python _content_has_images()
 * json_content_has_images — port of Python _content_has_images()
 */
/* Port of Python: _content_has_images */
/* AG26: Port of Python agent/context_compressor.py:_content_has_images() */
bool json_content_has_images(const json_t *content) {
    if (!content || content->type != JSON_ARRAY) return false;
    size_t n = json_len(content);
    for (size_t i = 0; i < n; i++) {
        const json_t *part = json_get(content, i);
        if (json_is_image_part(part)) return true;
    }
    return false;
}

/* Port of Python: _strip_images_from_content */
/* AG26: Port of Python agent/context_compressor.py:_strip_images_from_content() */
json_t *json_strip_images_from_content(const json_t *content) {
    if (!content || content->type != JSON_ARRAY) return (json_t *)content;
    if (!json_content_has_images(content)) return (json_t *)content;

    json_t *new_parts = json_new_array();
    if (!new_parts) return (json_t *)content;

    size_t n = json_len(content);
    for (size_t i = 0; i < n; i++) {
        const json_t *part = json_get(content, i);
        if (json_is_image_part(part)) {
            json_t *placeholder = json_new_object();
            if (!placeholder) { json_free(new_parts); return (json_t *)content; }
            json_object_set(placeholder, "type", json_new_string("text"));
            json_object_set(placeholder, "text",
                json_new_string("[Attached image — stripped after compression]"));
            json_array_append(new_parts, placeholder);
        } else {
            /* Deep copy non-image parts via serialize/parse round-trip */
            char *serialized = json_serialize(part);
            if (serialized) {
                json_t *copy = json_parse(serialized, NULL);
                free(serialized);
                if (copy) {
                    json_array_append(new_parts, copy);
                }
            }
        }
    }
    return new_parts;
}

/* Port of Python: _strip_image_parts_from_parts */
/* AG26: Port of Python agent/context_compressor.py:_strip_image_parts_from_parts() */
json_t *json_strip_image_parts_from_parts(const json_t *parts) {
    if (!parts || parts->type != JSON_ARRAY) return NULL;

    bool had_image = false;
    json_t *out = json_new_array();
    if (!out) return NULL;

    size_t n = json_len(parts);
    for (size_t i = 0; i < n; i++) {
        const json_t *part = json_get(parts, i);
        if (!part || part->type != JSON_OBJECT) {
            char *serialized = json_serialize(part);
            if (serialized) {
                json_t *copy = json_parse(serialized, NULL);
                free(serialized);
                if (copy) json_array_append(out, copy);
            }
            continue;
        }
        if (json_is_image_part(part)) {
            had_image = true;
            json_t *placeholder = json_new_object();
            if (placeholder) {
                json_object_set(placeholder, "type", json_new_string("text"));
                json_object_set(placeholder, "text",
                    json_new_string("[screenshot removed to save context]"));
                json_array_append(out, placeholder);
            }
        } else {
            char *serialized = json_serialize(part);
            if (serialized) {
                json_t *copy = json_parse(serialized, NULL);
                free(serialized);
                if (copy) json_array_append(out, copy);
            }
        }
    }

    if (!had_image) {
        json_free(out);
        return NULL;
    }
    return out;
}

/**
 * strip_historical_media — Port of Python: _strip_historical_media
 * AG26: Port of Python agent/context_compressor.py:_strip_historical_media()
 * Replaces image parts in older messages with placeholder text.
 *
 * The anchor is the *last* user message that has any image content.
 * Every message before that anchor gets its image parts replaced.
 * Messages from the anchor onward are kept unchanged.
 *
 * Mutates the input array in place (modifies content of messages before anchor).
 * Returns the count of messages that were modified.
 *
 * This is the key function that prevents re-shipping multi-MB base64 image
 * blobs on every turn, which causes context bloat and provider rejections.
 */
int strip_historical_media(json_t *messages) {
    if (!messages || messages->type != JSON_ARRAY) return 0;

    size_t msg_count = json_len(messages);
    if (msg_count == 0) return 0;

    /* Find the newest user message that carries at least one image part */
    int anchor = -1;
    for (int i = (int)msg_count - 1; i >= 0; i--) {
        const json_t *msg = json_get(messages, (size_t)i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        const char *role = json_get_str(msg, "role", NULL);
        if (!role || strcmp(role, "user") != 0) continue;
        const json_t *content = json_object_get(msg, "content");
        if (json_content_has_images(content)) {
            anchor = i;
            break;
        }
    }

    if (anchor <= 0) return 0;  /* No image-bearing user message, or it's the first */

    int changed = 0;
    for (int i = 0; i < anchor; i++) {
        json_t *msg = json_get(messages, (size_t)i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        const json_t *content = json_object_get(msg, "content");
        if (!json_content_has_images(content)) continue;

        /* Replace content with stripped version */
        json_t *stripped = json_strip_images_from_content(content);
        if (stripped && stripped != content) {
            json_object_set(msg, "content", stripped);
            changed++;
        }
    }
    return changed;
}

/**
 * content_length_for_budget — Port of Python: _content_length_for_budget
 * AG26: Port of Python agent/context_compressor.py:_content_length_for_budget()
 * Returns the effective char-length of a message's content for token budgeting.
 *
 * - Plain strings: len(content)
 * - Arrays: sum of text part lengths + IMAGE_CHAR_EQUIVALENT per image part
 * - Other: len(str(content))
 *
 * This prevents the compressor from treating a turn with 5 attached images
 * as near-zero tokens just because the text part is empty.
 */
int content_length_for_budget(const json_t *raw_content) {
    static const int IMAGE_CHAR_EQUIVALENT = 2000;  /* ~500 tokens per image */

    if (!raw_content) return 0;

    if (raw_content->type == JSON_STRING) {
        const char *s = json_get_str(raw_content, "", NULL);
        return s ? (int)strlen(s) : 0;
    }

    if (raw_content->type != JSON_ARRAY) {
        char *s = json_serialize(raw_content);
        int len = s ? (int)strlen(s) : 0;
        free(s);
        return len;
    }

    int total = 0;
    size_t n = json_len(raw_content);
    for (size_t i = 0; i < n; i++) {
        const json_t *p = json_get(raw_content, i);
        if (!p) continue;

        if (p->type == JSON_STRING) {
            const char *s = json_get_str(p, "", NULL);
            total += s ? (int)strlen(s) : 0;
            continue;
        }

        if (p->type == JSON_OBJECT) {
            if (json_is_image_part(p)) {
                total += IMAGE_CHAR_EQUIVALENT;
            } else {
                const char *text = json_get_str(p, "text", NULL);
                total += text ? (int)strlen(text) : 0;
            }
            continue;
        }

        /* Fallback: serialize and measure */
        char *s = json_serialize(p);
        total += s ? (int)strlen(s) : 0;
        free(s);
    }
    return total;
}

/* ================================================================
 *  context_compressor utility functions (PoP)
 * ================================================================ */

/* Port of Python: _extract_tool_call_name_and_args */
void context_compressor_extract_name_args(const json_t *tool_call,
                                           char **name_out, char **args_out) {
    const char *name = "unknown";
    const char *args = "";
    if (tool_call && tool_call->type == JSON_OBJECT) {
        const json_t *fn = json_object_get(tool_call, "function");
        if (fn && fn->type == JSON_OBJECT) {
            const char *n = json_get_str(fn, "name", NULL);
            if (n) name = n;
            const char *a = json_get_str(fn, "arguments", NULL);
            if (a) args = a;
        }
    }
    *name_out = strdup(name);
    *args_out = strdup(args);
}

/* Port of Python: _extract_tool_call_id / _get_tool_call_id */
const char *context_compressor_extract_id(const json_t *tool_call) {
    if (!tool_call || tool_call->type != JSON_OBJECT) return strdup("");
    const char *id = json_get_str(tool_call, "id", "");
    return strdup(id);
}

/* Port of Python: _content_text_for_contains */
char *context_compressor_content_text(const json_t *content) {
    if (!content || content->type == JSON_NULL) return strdup("");
    if (content->type == JSON_STRING) {
        const char *s = json_string_value(content);
        return strdup(s ? s : "");
    }
    if (content->type != JSON_ARRAY) {
        const char *s = json_serialize(content);
        char *ret = strdup(s ? s : "");
        free((void *)s);
        return ret;
    }
    size_t total = 0;
    size_t n = json_len(content);
    for (size_t i = 0; i < n; i++) {
        const json_t *part = json_get(content, i);
        if (!part) continue;
        if (part->type == JSON_STRING) {
            const char *s = json_string_value(part);
            if (s) total += strlen(s) + 1;
        } else if (part->type == JSON_OBJECT) {
            const char *t = json_get_str(part, "text", NULL);
            if (t) total += strlen(t) + 1;
        }
    }
    if (total == 0) return strdup("");
    char *buf = malloc(total + 1);
    if (!buf) return strdup("");
    buf[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        const json_t *part = json_get(content, i);
        if (!part) continue;
        const char *text = NULL;
        if (part->type == JSON_STRING) {
            text = json_string_value(part);
        } else if (part->type == JSON_OBJECT) {
            text = json_get_str(part, "text", NULL);
        }
        if (text && text[0]) {
            if (buf[0]) strcat(buf, "\n");
            strncat(buf, text, total - strlen(buf) - 1);
        }
    }
    return buf;
}

/* Port of Python: _append_text_to_content — append/prepend plain text safely. */
json_t *context_compressor_append_text(const json_t *content, const char *text, bool prepend) {
    if (!content || content->type == JSON_NULL) return json_new_string(text ? text : "");
    if (content->type == JSON_STRING) {
        const char *existing = json_string_value(content);
        if (!existing) existing = "";
        size_t elen = strlen(existing);
        size_t tlen = text ? strlen(text) : 0;
        size_t blen = elen + tlen + 1;
        char *buf = malloc(blen);
        if (!buf) return json_new_string(existing);
        if (prepend) {
            memcpy(buf, text, tlen);
            memcpy(buf + tlen, existing, elen + 1);
        } else {
            memcpy(buf, existing, elen);
            memcpy(buf + elen, text, tlen + 1);
        }
        json_t *ret = json_new_string(buf);
        free(buf);
        return ret;
    }
    if (content->type == JSON_ARRAY) {
        json_t *out = json_new_array();
        if (!out) return json_new_string(text ? text : "");
        size_t n = json_len(content);
        if (prepend) {
            json_t *tb = json_new_object();
            json_object_set(tb, "type", json_new_string("text"));
            json_object_set(tb, "text", json_new_string(text ? text : ""));
            json_array_append(out, tb);
        }
        for (size_t i = 0; i < n; i++) {
            const json_t *part = json_get(content, i);
            if (part) {
                char *s = json_serialize(part);
                json_t *copy = s ? json_parse(s, NULL) : NULL;
                free(s);
                if (copy) json_array_append(out, copy);
            }
        }
        if (!prepend) {
            json_t *tb = json_new_object();
            json_object_set(tb, "type", json_new_string("text"));
            json_object_set(tb, "text", json_new_string(text ? text : ""));
            json_array_append(out, tb);
        }
        return out;
    }
    /* Non-string, non-array content: mirror Python text + str(content). */
    {
        char *rendered = json_serialize(content);
        const char *r = rendered ? rendered : "";
        size_t rlen = strlen(r);
        size_t tlen = text ? strlen(text) : 0;
        size_t blen = rlen + tlen + 1;
        char *buf = malloc(blen);
        if (!buf) { free(rendered); return json_new_string(text ? text : ""); }
        if (prepend) { memcpy(buf, text ? text : "", tlen); memcpy(buf + tlen, r, rlen + 1); }
        else         { memcpy(buf, r, rlen); memcpy(buf + rlen, text ? text : "", tlen + 1); }
        free(rendered);
        json_t *ret = json_new_string(buf);
        free(buf);
        return ret;
    }
}

/* Port of Python: _collect_path_mentions — collect file path mentions from text */
/* AG26: Port of Python agent/context_compressor.py:_collect_path_mentions() */
void collect_path_mentions(const char *text, const char **result, int *count, int limit) {
    if (!text || !result || !count) return;
    *count = 0;
    /* Simple pattern: look for paths starting with ./ or / or ~/ */
    const char *p = text;
    while (*p && *count < limit) {
        /* Skip non-path chars */
        if (*p != '/' && *p != '.' && *p != '~') { p++; continue; }
        /* Check for ./ or ../ or ~/ */
        if (*p == '.' && (*(p+1) != '/' || *(p+1) != '.')) { p++; continue; }
        if (*p == '~' && *(p+1) != '/') { p++; continue; }
        /* Found a potential path start - extract to end of word */
        const char *start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != ',' && *p != ';')
            p++;
        size_t len = (size_t)(p - start);
        if (len > 1 && len < 4096) {
            result[*count] = strndup(start, len);
            (*count)++;
        }
    }
}

/* Port of Python: _clear_backend_probe_cache — also covers this in C */
/* Already defined: collect_path_mentions */
