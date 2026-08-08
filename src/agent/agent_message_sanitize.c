/*
 * agent_message_sanitize.c — Post-response message sanitization pipeline.
 *
 * Port of the sanitization steps inside Python's build_assistant_message().
 * Applied to each assistant message after creation to ensure content
 * is clean before entering conversation history:
 *
 *   1. Surrogate character sanitization  (sanitize_surrogates)
 *   2. Think block stripping              (inline tag stripper)
 *   3. Secret redaction                   (hermes_redact)
 */

#include "hermes_core_types.h"
#include "hermes_think_scrubber.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "hermes_redact.h"
#include "hermes_sanitize.h"

/* ================================================================
 *  Think Block Stripping (non-streaming)
 *
 *  Strips <think>, <thinking>, <reasoning>, <thought>,
 *  <REASONING_SCRATCHPAD> blocks from complete text.
 *  Matches Python agent_runtime_helpers.strip_think_blocks().
 * ================================================================ */

/* Tag definitions — must match think_scrubber.c */
#define STRIP_TAG_COUNT 5
static const char *STRIP_OPEN[] = {
    "<think>", "<thinking>", "<reasoning>", "<thought>", "<REASONING_SCRATCHPAD>"
};
static const char *STRIP_CLOSE[] = {
    "</think>", "</thinking>", "</reasoning>", "</thought>", "</REASONING_SCRATCHPAD>"
};

/* Additional tool-call XML blocks to strip (Gemma-style inline tool calls) */
#define TOOLCALL_TAG_COUNT 6
static const char *TOOLCALL_TAGS[] = {
    "tool_call", "tool_calls", "tool_result",
    "function_call", "function_calls", "function"
};

/* Case-insensitive match at position pos */
static int tag_match_at(const char *text, int text_len, int pos,
                        const char *tag, int tag_len) {
    if (pos + tag_len > text_len) return 0;
    for (int j = 0; j < tag_len; j++) {
        if (tolower((unsigned char)text[pos + j]) != tolower((unsigned char)tag[j]))
            return 0;
    }
    return 1;
}

/* Find next open tag (any variant) starting from pos. Returns -1 if none. */
static int find_next_open(const char *text, int text_len, int pos,
                          int *tag_idx, int *tag_pos) {
    for (int i = pos; i < text_len; i++) {
        if (text[i] != '<') continue;
        for (int t = 0; t < STRIP_TAG_COUNT; t++) {
            int tlen = (int)strlen(STRIP_OPEN[t]);
            if (tag_match_at(text, text_len, i, STRIP_OPEN[t], tlen)) {
                *tag_idx = t;
                *tag_pos = i;
                return 1;
            }
        }
    }
    return 0;
}

/* Find close tag for given tag index starting from pos. Returns -1 if none. */
static int find_close_for(const char *text, int text_len, int pos, int tag_idx) {
    int tlen = (int)strlen(STRIP_CLOSE[tag_idx]);
    for (int i = pos; i <= text_len - tlen; i++) {
        if (tag_match_at(text, text_len, i, STRIP_CLOSE[tag_idx], tlen))
            return i + tlen;
    }
    return -1;
}

/* Port of Python agent/agent_runtime_helpers.py:strip_think_blocks().
 * Strip all think/reasoning blocks and tool-call XML from text.
 * Returns malloc'd string; caller must free. Returns NULL on NULL input.
 * Declared extern so other port files (e.g. port_context_compressor_ports.c
 * _serialize_for_summary) can reuse it without duplicating the scrubbing logic. */
char *strip_think_blocks(const char *text) {
    if (!text) return NULL;

    int text_len = (int)strlen(text);
    if (text_len == 0) return strdup("");

    /* Allocate worst-case output buffer (same size) */
    char *out = (char *)malloc((size_t)text_len + 1);
    if (!out) return NULL;
    int out_pos = 0;
    int src_pos = 0;

    /* Pass 1: Strip closed <tag>...</tag> pairs for reasoning tags */
    while (src_pos < text_len) {
        int tag_idx, open_pos;
        if (find_next_open(text, text_len, src_pos, &tag_idx, &open_pos)) {
            int close_pos = find_close_for(text, text_len,
                                          open_pos + (int)strlen(STRIP_OPEN[tag_idx]),
                                          tag_idx);
            if (close_pos >= 0) {
                /* Copy everything before the open tag */
                while (src_pos < open_pos && out_pos < text_len)
                    out[out_pos++] = text[src_pos++];
                /* Skip the pair */
                src_pos = close_pos;
                continue;
            }
        }
        if (out_pos < text_len)
            out[out_pos++] = text[src_pos++];
    }
    out[out_pos] = '\0';

    /* Re-alloc after pass 1 */
    int cur_len = out_pos;
    char *cur = (char *)realloc(out, (size_t)cur_len + 1);
    if (!cur) { free(out); return NULL; }
    out = cur;

    /* Pass 2: Strip tool-call XML blocks (<tool_call>...</tool_call>, etc.) */
    {
        int out2_pos = 0;
        int src2_pos = 0;
        int cur2_len = cur_len;
        char *out2 = (char *)malloc((size_t)cur2_len + 1);
        if (!out2) { free(out); return NULL; }

        while (src2_pos < cur2_len) {
            int found = 0;
            for (int t = 0; t < TOOLCALL_TAG_COUNT && !found; t++) {
                char open_buf[64];
                int olen = snprintf(open_buf, sizeof(open_buf), "<%s", TOOLCALL_TAGS[t]);
                char close_buf[64];
                int clen = snprintf(close_buf, sizeof(close_buf), "</%s>", TOOLCALL_TAGS[t]);

                if (tag_match_at(out, cur2_len, src2_pos, open_buf, olen)) {
                    /* Find matching close tag */
                    for (int i = src2_pos + olen; i <= cur2_len - clen; i++) {
                        if (tag_match_at(out, cur2_len, i, close_buf, clen)) {
                            /* Skip entire block: open + content + close */
                            src2_pos = i + clen;
                            found = 1;
                            break;
                        }
                    }
                }
            }
            if (!found) {
                if (out2_pos < cur2_len)
                    out2[out2_pos++] = out[src2_pos++];
            }
        }
        out2[out2_pos] = '\0';
        free(out);

        cur = (char *)realloc(out2, (size_t)out2_pos + 1);
        if (!cur) { free(out2); return NULL; }
        out = cur;
        cur_len = out2_pos;
    }

    /* Pass 3: Strip unterminated open tags at block boundaries */
    {
        int out3_pos = 0;
        int src3_pos = 0;
        int boundary_start = 1;
        char *out3 = (char *)malloc((size_t)cur_len + 1);
        if (!out3) { free(out); return NULL; }

        while (src3_pos < cur_len) {
            /* Check for open tag at boundary */
            int tag_idx, open_pos = -1;
            int found = 0;
            if (boundary_start || (src3_pos > 0 && out[src3_pos - 1] == '\n')) {
                /* Check for open tag starting here or after optional whitespace */
                int check = src3_pos;
                while (check < cur_len && (out[check] == ' ' || out[check] == '\t'))
                    check++;
                if (find_next_open(out, cur_len, check, &tag_idx, &open_pos) &&
                    open_pos == check) {
                    /* Unterminated open tag — strip to end */
                    found = 1;
                }
            }
            if (found) {
                break;  /* strip remaining */
            }
            out3[out3_pos++] = out[src3_pos++];
            if (out[src3_pos - 1] == '\n')
                boundary_start = 1;
            else
                boundary_start = 0;  /* reset after non-boundary check */
        }
        out3[out3_pos] = '\0';
        free(out);

        char *shrunk = (char *)realloc(out3, (size_t)out3_pos + 1);
        if (!shrunk) { free(out3); return NULL; }
        out = shrunk;
        cur_len = out3_pos;
    }

    /* Pass 4: Strip orphan tags (any remaining open/close reasoning tags) */
    {
        int out4_pos = 0;
        int src4_pos = 0;
        char *out4 = (char *)malloc((size_t)cur_len + 1);
        if (!out4) { free(out); return NULL; }

        while (src4_pos < cur_len) {
            int stripped = 0;
            /* Orphan open tag */
            int tag_idx;
            int open_pos;
            if (find_next_open(out, cur_len, src4_pos, &tag_idx, &open_pos) &&
                open_pos == src4_pos) {
                int tlen = (int)strlen(STRIP_OPEN[tag_idx]);
                src4_pos += tlen;
                stripped = 1;
            }
            /* Orphan close tag */
            if (!stripped) {
                for (int t = 0; t < STRIP_TAG_COUNT && !stripped; t++) {
                    int clen = (int)strlen(STRIP_CLOSE[t]);
                    if (tag_match_at(out, cur_len, src4_pos, STRIP_CLOSE[t], clen)) {
                        src4_pos += clen;
                        /* skip trailing whitespace */
                        while (src4_pos < cur_len &&
                               (out[src4_pos] == ' ' || out[src4_pos] == '\t' ||
                                out[src4_pos] == '\n' || out[src4_pos] == '\r'))
                            src4_pos++;
                        stripped = 1;
                    }
                }
            }
            if (!stripped) {
                out4[out4_pos++] = out[src4_pos++];
            }
        }
        out4[out4_pos] = '\0';
        free(out);

        char *shrunk = (char *)realloc(out4, (size_t)out4_pos + 1);
        if (!shrunk) { free(out4); return NULL; }
        out = shrunk;
    }

    return out;
}

/* ================================================================
 *  Public API: hermes_message_sanitize
 * ================================================================ */

/* Sanitize a single assistant message:
 *   1. Sanitize surrogates in content and reasoning
 *   2. Strip think/reasoning blocks from content
 *   3. Redact sensitive text from content and tool call arguments
 *
 * Returns true on success (always succeeds, even if no changes needed).
 * Returns false only if allocation fails for ALL fields.
 */
bool hermes_message_sanitize(message_t *msg) {
    if (!msg) return false;
    bool any_ok = false;

    /* Step 1: Surrogate sanitization on content */
    if (msg->content) {
        char *sanitized = sanitize_surrogates(msg->content);
        if (sanitized) {
            if (strcmp(sanitized, msg->content) != 0) {
                free(msg->content);
                msg->content = sanitized;
            } else {
                free(sanitized);
            }
            any_ok = true;
        }
    }

    /* Step 2: Surrogate sanitization on reasoning */
    if (msg->reasoning) {
        char *sanitized = sanitize_surrogates(msg->reasoning);
        if (sanitized) {
            if (strcmp(sanitized, msg->reasoning) != 0) {
                free(msg->reasoning);
                msg->reasoning = sanitized;
            } else {
                free(sanitized);
            }
            any_ok = true;
        }
    }

    /* Step 3: Strip think blocks from content */
    if (msg->content) {
        char *stripped = strip_think_blocks(msg->content);
        if (stripped) {
            if (strcmp(stripped, msg->content) != 0) {
                free(msg->content);
                msg->content = stripped;
            } else {
                free(stripped);
            }
            any_ok = true;
        }
    }

    /* Step 4: Redact sensitive text from content */
    if (msg->content) {
        char *redacted = hermes_redact(msg->content);
        if (redacted) {
            free(msg->content);
            msg->content = redacted;
            any_ok = true;
        }
    }

    /* Step 5: Repair malformed tool call arguments before redaction */
    for (int i = 0; i < msg->tool_calls_count && i < 64; i++) {
        char *args = msg->tool_calls[i].arguments;
        if (!args[0] || strcmp(args, "None") == 0 || strcmp(args, "null") == 0) {
            snprintf(args, sizeof(msg->tool_calls[i].arguments), "{}");
            any_ok = true;
        }
    }

    /* Step 6: Redact sensitive text from tool call arguments */
    for (int i = 0; i < msg->tool_calls_count && i < 64; i++) {
        if (msg->tool_calls[i].arguments[0]) {
            char *redacted = hermes_redact(msg->tool_calls[i].arguments);
            if (redacted) {
                strncpy(msg->tool_calls[i].arguments, redacted,
                        sizeof(msg->tool_calls[i].arguments) - 1);
                msg->tool_calls[i].arguments[sizeof(msg->tool_calls[i].arguments) - 1] = '\0';
                free(redacted);
                any_ok = true;
            }
        }
    }

    return any_ok;
}

/* ================================================================
 *  AG16: Message sanitization gap — recursive surrogate + non-ASCII
 *        walking for messages, structures, and JSON string escaping.
 *
 *  Ports from Python agent/message_sanitization.py:
 *    _sanitize_structure_surrogates  — recursive dict/list surrogate scrub
 *    _sanitize_messages_surrogates   — surrogate scrub for message arrays
 *    _escape_invalid_chars_in_json_strings — escape control chars in JSON strs
 *    _strip_non_ascii               — encode(ascii, ignore) fallback
 *    _sanitize_messages_non_ascii   — non-ASCII scrub for message arrays
 *    _sanitize_tools_non_ascii      — non-ASCII scrub for tool payloads
 *    _sanitize_structure_non_ascii  — recursive non-ASCII scrub for nested
 * ================================================================ */

/* ---- _strip_non_ascii ---- */
/* Port of Python: _strip_non_ascii */
/* Remove non-ASCII bytes (ordinal > 127). Works on raw C strings.
 * Returns malloc'd string; caller must free. Returns NULL on NULL input. */
static char *strip_non_ascii(const char *text) {
    if (!text) return NULL;
    size_t len = strlen(text);
    char *result = (char *)malloc(len + 1);
    if (!result) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if ((unsigned char)text[i] < 0x80) {
            result[j++] = text[i];
        }
    }
    result[j] = '\0';
    return result;
}

/* ---- _escape_invalid_chars_in_json_strings ---- */
/* Port of Python: _escape_invalid_chars_in_json_strings */
/* Escape unescaped control chars (0x00-0x1F) inside JSON string values.
 * Walks character-by-character tracking string context.
 * Returns malloc'd string; caller must free. */
char *escape_invalid_chars_in_json_strings(const char *raw) {
    if (!raw) return NULL;
    size_t len = strlen(raw);
    /* Worst case: every char is a control char → 6 chars each (\\uXXXX) */
    char *out = (char *)malloc(len * 6 + 1);
    if (!out) return NULL;
    size_t j = 0;
    int in_string = 0;
    size_t i = 0;
    while (i < len) {
        unsigned char ch = (unsigned char)raw[i];
        if (in_string) {
            if (ch == '\\' && i + 1 < len) {
                /* Already-escaped char — pass through */
                out[j++] = raw[i++];
                out[j++] = raw[i++];
                continue;
            }
            if (ch == '"') {
                in_string = 0;
                out[j++] = '"';
            } else if (ch < 0x20) {
                /* Control character — escape as \\uXXXX */
                int written = snprintf(out + j, 7, "\\u%04x", ch);
                if (written > 0) j += (size_t)written;
            } else {
                out[j++] = raw[i];
            }
        } else {
            if (ch == '"') {
                in_string = 1;
            }
            out[j++] = raw[i];
        }
        i++;
    }
    out[j] = '\0';
    return out;
}

/* Port of Python: _sanitize_structure_surrogates
 * Walk a json_t tree and replace surrogate code points in string values,
 * recursing into nested dict/list values (matches Python's _walk, which
 * scrubs VALUES only — not key names). Returns true if ANY
 * replacements were made (does NOT abort on first hit — the whole tree is
 * scrubbed). Mutates in-place. */
bool agent_message_sanitize_structure_surrogates(json_t *node)
{
    if (!node) return false;
    bool found = false;
    if (node->type == JSON_STRING) {
        if (strstr(node->str_val, "\xED")) {
            char *fixed = sanitize_surrogates(node->str_val);
            if (fixed) {
                free(node->str_val);
                node->str_val = fixed;
                found = true;
            }
        }
        return found;
    }
    if (node->type == JSON_OBJECT) {
        for (size_t i = 0; i < node->c.count; i++) {
            if (node->c.items[i] &&
                agent_message_sanitize_structure_surrogates(node->c.items[i]))
                found = true;
        }
        return found;
    }
    if (node->type == JSON_ARRAY) {
        for (size_t i = 0; i < node->c.count; i++) {
            if (node->c.items[i] &&
                agent_message_sanitize_structure_surrogates(node->c.items[i]))
                found = true;
        }
        return found;
    }
    return found;
}

/* ---- sanitize_structure_non_ascii ---- */
/* Port of Python: _sanitize_structure_non_ascii */
/* Recursively strip non-ASCII bytes from all string values in a json_t tree.
 * Returns true if any changes were made. Mutates in-place. */
static bool sanitize_structure_non_ascii_json(json_node_t *node) {
    if (!node) return false;
    bool changed = false;
    if (node->type == JSON_STRING) {
        char *fixed = strip_non_ascii(node->str_val);
        if (fixed) {
            if (strcmp(fixed, node->str_val) != 0) {
                free(node->str_val);
                node->str_val = fixed;
                changed = true;
            } else {
                free(fixed);
            }
        }
    }
    if (node->type == JSON_OBJECT) {
        for (size_t i = 0; i < node->c.count; i++) {
            if (sanitize_structure_non_ascii_json(node->c.items[i]))
                changed = true;
        }
    }
    if (node->type == JSON_ARRAY) {
        for (size_t i = 0; i < node->c.count; i++) {
            if (sanitize_structure_non_ascii_json(node->c.items[i]))
                changed = true;
        }
    }
    return changed;
}

/* ---- sanitize_messages_surrogates ---- */
/* Port of Python: _sanitize_messages_surrogates */
/* Sanitize surrogate characters from message content, reasoning,
 * tool_calls (id, function name, arguments), and all additional
 * string/nested fields (reasoning_content, reasoning_details, etc.)
 * in-place on a message_t array.
 * Returns true if any surrogates were found and replaced. */
bool sanitize_messages_surrogates(message_t *messages, int count) {
    if (!messages || count <= 0) return false;
    bool found = false;

    for (int i = 0; i < count; i++) {
        message_t *msg = &messages[i];

        /* Content (string) */
        if (msg->content) {
            char *fixed = sanitize_surrogates(msg->content);
            if (fixed) {
                if (strcmp(fixed, msg->content) != 0) {
                    free(msg->content);
                    msg->content = fixed;
                    found = true;
                } else {
                    free(fixed);
                }
            }
        }

        /* Reasoning string */
        if (msg->reasoning) {
            char *fixed = sanitize_surrogates(msg->reasoning);
            if (fixed) {
                if (strcmp(fixed, msg->reasoning) != 0) {
                    free(msg->reasoning);
                    msg->reasoning = fixed;
                    found = true;
                } else {
                    free(fixed);
                }
            }
        }

        /* Encrypted content (xAI) */
        if (msg->encrypted_content) {
            char *fixed = sanitize_surrogates(msg->encrypted_content);
            if (fixed) {
                if (strcmp(fixed, msg->encrypted_content) != 0) {
                    free(msg->encrypted_content);
                    msg->encrypted_content = fixed;
                    found = true;
                } else {
                    free(fixed);
                }
            }
        }

        /* Name field */
        /* Note: name is not a pointer in message_t, it's embedded.
         * Skip — name isn't a heap pointer in our struct. */

        /* Tool calls */
        for (int j = 0; j < msg->tool_calls_count && j < 64; j++) {
            tool_call_t *tc = &msg->tool_calls[j];
            /* tc->id, tc->name, tc->arguments are fixed-size arrays.
             * We need to sanitize them in-place via temp buffer. */
            if (strstr(tc->id, "\xED")) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%s", tc->id);
                char *fixed = sanitize_surrogates(tmp);
                if (fixed) {
                    snprintf(tc->id, sizeof(tc->id), "%s", fixed);
                    free(fixed);
                    found = true;
                }
            }
            if (strstr(tc->name, "\xED")) {
                char tmp[128];
                snprintf(tmp, sizeof(tmp), "%s", tc->name);
                char *fixed = sanitize_surrogates(tmp);
                if (fixed) {
                    snprintf(tc->name, sizeof(tc->name), "%s", fixed);
                    free(fixed);
                    found = true;
                }
            }
            if (strstr(tc->arguments, "\xED")) {
                char tmp[4096];
                snprintf(tmp, sizeof(tmp), "%s", tc->arguments);
                char *fixed = sanitize_surrogates(tmp);
                if (fixed) {
                    snprintf(tc->arguments, sizeof(tc->arguments), "%s", fixed);
                    free(fixed);
                    found = true;
                }
            }
            /* Nested structured fields inside tool-call arguments JSON
             * (e.g. reasoning_details arrays of dicts) — flat per-field
             * checks above don't reach them. Port of Python's
             * _sanitize_structure_surrogates(value) on each message. */
            if (tc->arguments && tc->arguments[0]) {
                char *err = NULL;
                json_t *aj = json_parse(tc->arguments, &err);
                if (err) free(err);
                if (aj) {
                    if (agent_message_sanitize_structure_surrogates(aj)) {
                        char *ser = json_serialize(aj);
                        if (ser) {
                            size_t n = strlen(ser);
                            if (n < sizeof(tc->arguments)) {
                                snprintf(tc->arguments, sizeof(tc->arguments),
                                         "%s", ser);
                                found = true;
                            }
                            free(ser);
                        }
                    }
                    json_free(aj);
                }
            }
        }

        /* Tool call id (for tool result messages) */
        if (msg->tool_call_id) {
            char *fixed = sanitize_surrogates(msg->tool_call_id);
            if (fixed) {
                if (strcmp(fixed, msg->tool_call_id) != 0) {
                    free(msg->tool_call_id);
                    msg->tool_call_id = fixed;
                    found = true;
                } else {
                    free(fixed);
                }
            }
        }

        /* Tool name (for tool result messages) */
        if (msg->tool_name) {
            char *fixed = sanitize_surrogates(msg->tool_name);
            if (fixed) {
                if (strcmp(fixed, msg->tool_name) != 0) {
                    free(msg->tool_name);
                    msg->tool_name = fixed;
                    found = true;
                } else {
                    free(fixed);
                }
            }
        }
    }
    return found;
}

/* ---- sanitize_messages_non_ascii ---- */
/* Port of Python: _sanitize_messages_non_ascii */
/* Strip non-ASCII characters from all heap-allocated string fields
 * in a message_t array. This is a last-resort recovery for systems
 * with ASCII-only encoding (LANG=C, minimal containers).
 * Returns true if any non-ASCII content was found and sanitized. */
bool sanitize_messages_non_ascii(message_t *messages, int count) {
    if (!messages || count <= 0) return false;
    bool found = false;

    for (int i = 0; i < count; i++) {
        message_t *msg = &messages[i];

        /* Content (string) */
        if (msg->content) {
            char *fixed = strip_non_ascii(msg->content);
            if (fixed) {
                if (strcmp(fixed, msg->content) != 0) {
                    free(msg->content);
                    msg->content = fixed;
                    found = true;
                } else {
                    free(fixed);
                }
            }
        }

        /* Reasoning string */
        if (msg->reasoning) {
            char *fixed = strip_non_ascii(msg->reasoning);
            if (fixed) {
                if (strcmp(fixed, msg->reasoning) != 0) {
                    free(msg->reasoning);
                    msg->reasoning = fixed;
                    found = true;
                } else {
                    free(fixed);
                }
            }
        }

        /* Encrypted content */
        if (msg->encrypted_content) {
            char *fixed = strip_non_ascii(msg->encrypted_content);
            if (fixed) {
                if (strcmp(fixed, msg->encrypted_content) != 0) {
                    free(msg->encrypted_content);
                    msg->encrypted_content = fixed;
                    found = true;
                } else {
                    free(fixed);
                }
            }
        }

        /* Tool calls — sanitize id, name, arguments in-place */
        for (int j = 0; j < msg->tool_calls_count && j < 64; j++) {
            tool_call_t *tc = &msg->tool_calls[j];
            if (strstr(tc->name, "\xC0")) { /* quick check for any non-ASCII byte */
                char tmp[128];
                snprintf(tmp, sizeof(tmp), "%s", tc->name);
                char *fixed = strip_non_ascii(tmp);
                if (fixed) {
                    snprintf(tc->name, sizeof(tc->name), "%s", fixed);
                    free(fixed);
                    found = true;
                }
            }
            if (strstr(tc->arguments, "\xC0")) {
                char tmp[4096];
                snprintf(tmp, sizeof(tmp), "%s", tc->arguments);
                char *fixed = strip_non_ascii(tmp);
                if (fixed) {
                    snprintf(tc->arguments, sizeof(tc->arguments), "%s", fixed);
                    free(fixed);
                    found = true;
                }
            }
            if (strstr(tc->id, "\xC0")) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%s", tc->id);
                char *fixed = strip_non_ascii(tmp);
                if (fixed) {
                    snprintf(tc->id, sizeof(tc->id), "%s", fixed);
                    free(fixed);
                    found = true;
                }
            }
        }

        /* Tool call id / tool name (for tool result messages) */
        if (msg->tool_call_id) {
            char *fixed = strip_non_ascii(msg->tool_call_id);
            if (fixed) {
                if (strcmp(fixed, msg->tool_call_id) != 0) {
                    free(msg->tool_call_id);
                    msg->tool_call_id = fixed;
                    found = true;
                } else {
                    free(fixed);
                }
            }
        }
        if (msg->tool_name) {
            char *fixed = strip_non_ascii(msg->tool_name);
            if (fixed) {
                if (strcmp(fixed, msg->tool_name) != 0) {
                    free(msg->tool_name);
                    msg->tool_name = fixed;
                    found = true;
                } else {
                    free(fixed);
                }
            }
        }
    }
    return found;
}

/* ---- sanitize_tools_non_ascii ---- */
/* Port of Python: _sanitize_tools_non_ascii */
/* Strip non-ASCII from a json_t tool-schema payload.
 * Delegates to the recursive json walker. */
json_node_t *sanitize_tools_non_ascii(json_node_t *tools) {
    if (!tools) return NULL;
    sanitize_structure_non_ascii_json(tools);
    return tools;
}

/* ── Codex intermediate ack detection ──────────────────────────
 *
 * Port of Python agent/agent_runtime_helpers.py
 * :func:`looks_like_codex_intermediate_ack`.
 *
 * Detect a planning/ack message that should continue instead of
 * ending the turn. Returns true if the assistant's response looks
 * like a "let me look into that" style ack that Codex should
 * continue past rather than treating as a final answer.
 */
/* Port of Python agent/agent_runtime_helpers.py:looks_like_codex_intermediate_ack().
 * Detect a planning/ack message that should continue instead of ending the turn. */
bool looks_like_codex_intermediate_ack(const message_t *const *messages,
                                        int msg_count,
                                        const char *user_message,
                                        const char *assistant_content)
{
    if (!messages || msg_count <= 0 || !assistant_content) return false;

    /* If any tool role message is present, this is a real turn */
    for (int i = 0; i < msg_count; i++) {
        if (messages[i] && messages[i]->role == MSG_TOOL)
            return false;
    }

    /* Strip think blocks and lowercase */
    char *stripped = strip_think_blocks(assistant_content);
    if (!stripped) return false;
    char *text = stripped;
    for (int i = 0; text[i]; i++) text[i] = (char)tolower((unsigned char)text[i]);
    size_t text_len = strlen(text);
    if (text_len == 0) { free(stripped); return false; }
    if (text_len > 1200) { free(stripped); return false; }

    /* Future acknowledgment patterns */
    const char *ack_patterns[] = {
        "i'll", "i will", "let me", "i can do that", "i can help with that",
        NULL
    };
    for (int p = 0; ack_patterns[p]; p++) {
        if (strstr(text, ack_patterns[p])) break;
        if (ack_patterns[p+1] == NULL) { free(stripped); return false; }
    }

    /* Action markers */
    const char *action_markers[] = {
        "look into", "look at", "inspect", "scan", "check", "analyz",
        "review", "explore", "read", "open", "run", "test", "fix",
        "debug", "search", "find", "walkthrough", "report back", "summarize",
        NULL
    };
    /* Workspace markers */
    const char *workspace_markers[] = {
        "directory", "current directory", "current dir", "cwd",
        "repo", "repository", "codebase", "project", "folder",
        "filesystem", "file tree", "files", "path", "~/",
        NULL
    };

    bool assistant_mentions_action = false;
    bool assistant_targets_workspace = false;
    bool user_targets_workspace = false;

    for (int p = 0; action_markers[p]; p++) {
        if (strstr(text, action_markers[p])) {
            assistant_mentions_action = true; break;
        }
    }
    for (int p = 0; workspace_markers[p]; p++) {
        if (strstr(text, workspace_markers[p])) {
            assistant_targets_workspace = true; break;
        }
    }

    if (user_message && *user_message) {
        char *user_lower = strdup(user_message);
        if (user_lower) {
            for (int i = 0; user_lower[i]; i++)
                user_lower[i] = (char)tolower((unsigned char)user_lower[i]);
            for (int p = 0; workspace_markers[p]; p++) {
                if (strstr(user_lower, workspace_markers[p])) {
                    user_targets_workspace = true; break;
                }
            }
            if (!user_targets_workspace) {
                for (int i = 0; user_lower[i]; i++) {
                    if (i > 0 && ((user_lower[i] == '/' && user_lower[i-1] == '~') ||
                         (user_lower[i] == '/' && user_lower[i+1]))) {
                        user_targets_workspace = true; break;
                    }
                }
            }
            free(user_lower);
        }
    }

    free(stripped);
    return (user_targets_workspace || assistant_targets_workspace)
           && assistant_mentions_action;
}

/* ── Reasoning extraction ────────────────────────────────────────
 *
 * Port of Python agent/agent_runtime_helpers.py:extract_reasoning().
 *
 * Extract reasoning/thinking content from an assistant message.
 * Checks structured reasoning fields first, then falls back to
 * inline extraction from content (think/reasoning XML blocks).
 *
 * Returns malloc'd string or NULL. Caller must free().
 */
char *extract_reasoning(const char *content, const char *reasoning) {
    if (!reasoning && !content) return NULL;

    /* Collect reasoning parts — simple linked list of strings */
    #define MAX_REASONING_PARTS 16
    const char *parts[MAX_REASONING_PARTS];
    int part_count = 0;

    /* 1. Check structured reasoning field */
    if (reasoning && reasoning[0]) {
        parts[part_count++] = reasoning;
    }

    /* 2. Scan content for think/reasoning XML blocks */
    if (content && content[0]) {
        int text_len = (int)strlen(content);
        int pos = 0;
        while (pos < text_len && part_count < MAX_REASONING_PARTS) {
            int tag_idx, open_pos;
            if (find_next_open(content, text_len, pos, &tag_idx, &open_pos)) {
                int close_pos = find_close_for(content, text_len,
                    open_pos + (int)strlen(STRIP_OPEN[tag_idx]), tag_idx);
                if (close_pos >= 0) {
                    /* Extract the text between open and close tags */
                    int inner_start = open_pos + (int)strlen(STRIP_OPEN[tag_idx]);
                    int inner_len = close_pos - (int)strlen(STRIP_CLOSE[tag_idx]) - inner_start;
                    if (inner_len > 0) {
                        /* Trim whitespace */
                        int s = inner_start;
                        int e = inner_start + inner_len;
                        while (s < e && (content[s] == ' ' || content[s] == '\t' ||
                                         content[s] == '\n' || content[s] == '\r'))
                            s++;
                        while (e > s && (content[e-1] == ' ' || content[e-1] == '\t' ||
                                         content[e-1] == '\n' || content[e-1] == '\r'))
                            e--;
                        if (e > s) {
                            char *part = (char *)malloc((size_t)(e - s + 1));
                            if (part) {
                                memcpy(part, content + s, (size_t)(e - s));
                                part[e - s] = '\0';
                                /* Avoid duplicate parts */
                                bool duplicate = false;
                                for (int d = 0; d < part_count; d++) {
                                    if (strcmp(parts[d], part) == 0) {
                                        duplicate = true; break;
                                    }
                                }
                                if (!duplicate) {
                                    parts[part_count] = NULL; /* marker for free */
                                    parts[part_count] = part;
                                    part_count++;
                                } else {
                                    free(part);
                                }
                            }
                        }
                    }
                    pos = close_pos;
                    continue;
                }
            }
            pos++;
        }
    }

    if (part_count == 0) return NULL;

    /* Combine all parts with double-newline separator */
    size_t total = 0;
    for (int i = 0; i < part_count; i++)
        total += strlen(parts[i]) + 2;
    total++; /* null terminator */

    char *result = (char *)malloc(total);
    if (!result) {
        for (int i = 0; i < part_count; i++) {
            /* Free only malloc'd parts from inline extraction */
            bool is_malloced = true;
            if (i == 0 && parts[i] == reasoning) is_malloced = false;
            /* Actually, only the reasoning field input is not malloc'd */
            /* Check if this part came from content extraction (after reasoning) */
            if (reasoning && parts[i] == reasoning) is_malloced = false;
            if (is_malloced) free((void *)parts[i]);
        }
        return NULL;
    }
    result[0] = '\0';
    for (int i = 0; i < part_count; i++) {
        if (i > 0) strcat(result, "\n\n");
        strcat(result, parts[i]);
    }

    /* Free malloc'd parts (content-extracted ones), NOT the reasoning parameter */
    for (int i = 0; i < part_count; i++) {
        if (reasoning && parts[i] == reasoning) continue; /* caller-owned */
        free((void *)parts[i]);
    }

    return result;
}

/* Port of Python: _strip_images_from_messages — remove image parts from messages JSON */
bool strip_images_from_messages(json_t *messages) {
    if (!messages || messages->type != JSON_ARRAY) return false;
    bool found = false;

    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *msg = messages->c.items[i];
        if (!msg || msg->type != JSON_OBJECT) continue;

        json_t *content = json_obj_get(msg, "content");
        if (!content || content->type != JSON_ARRAY) continue;

        json_t *new_parts = json_array();
        bool had_image = false;

        for (size_t j = 0; j < content->c.count; j++) {
            json_t *part = content->c.items[j];
            json_t *type_node = json_obj_get(part, "type");
            const char *type = type_node ? type_node->str_val : NULL;

            if (type && (strcmp(type, "image_url") == 0 ||
                         strcmp(type, "image") == 0 ||
                         strcmp(type, "input_image") == 0)) {
                had_image = true;
                continue;
            }
            json_append(new_parts, part);
        }

        if (had_image) {
            found = true;
            if (json_len(new_parts) > 0) {
                json_set(msg, "content", new_parts);
            } else {
                json_t *role_node = json_obj_get(msg, "role");
                const char *role = role_node ? role_node->str_val : NULL;
                if (role && strcmp(role, "tool") == 0) {
                    json_set(msg, "content", json_string(
                        "[image content removed \xe2\x80\x94 server does not support images]"));
                }
            }
        }
        json_free(new_parts);
    }
    return found;
}

/* ================================================================
 *  close_interrupted_tool_sequence
 *
 *  Port of Python agent/message_sanitization.close_interrupted_tool_sequence.
 *  If messages (a json_t array) ends on a "tool" role message, append a
 *  synthetic assistant turn (text.trim() or "Operation interrupted.") and
 *  return 1. Returns 0 otherwise (empty, or tail not a tool message).
 *  Mutates the array in place (mirrors Python in-place behaviour).
 * ================================================================ */

/* PoP: message_sanitize_close_interrupted @ agent/message_sanitization.py:close_interrupted_tool_sequence */
bool message_sanitize_close_interrupted(json_t *messages, const char *final_response)
{
    if (!messages || messages->type != JSON_ARRAY || json_len(messages) == 0)
        return 0;
    json_t *last = messages->c.items[json_len(messages) - 1];
    if (!last || last->type != JSON_OBJECT)
        return 0;
    json_t *role_node = json_obj_get(last, "role");
    const char *role = role_node ? role_node->str_val : NULL;
    if (!role || strcmp(role, "tool") != 0)
        return 0;

    const char *text = (final_response && *final_response) ? final_response : NULL;
    char *trimmed = NULL;
    if (text) {
        /* strip leading/trailing whitespace */
        while (*text && isspace((unsigned char)*text)) text++;
        const char *end = text + strlen(text);
        while (end > text && isspace((unsigned char)end[-1])) end--;
        size_t n = (size_t)(end - text);
        trimmed = (char *)malloc(n + 1);
        memcpy(trimmed, text, n);
        trimmed[n] = '\0';
    }
    const char *content = (trimmed && trimmed[0]) ? trimmed : "Operation interrupted.";

    json_t *turn = json_object();
    json_set(turn, "role", json_string("assistant"));
    json_set(turn, "content", json_string(content));
    json_append(messages, turn);
    /* ownership: json_set/json_append steal, turn is now owned by array */
    if (trimmed) free(trimmed);
    return 1;
}
