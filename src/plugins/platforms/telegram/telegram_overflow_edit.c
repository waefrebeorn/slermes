/*
 * telegram_overflow_edit.c -- real C11 port of the oversized-edit split of
 * upstream Hermes plugins/platforms/telegram/telegram_overflow_edit.py.
 *
 * PoP: w20_telegramov_43149b_edit_overflow_split @ plugins/platforms/telegram/telegram_overflow_edit.py:edit_overflow_split
 *
 * The coroutine's decisions are all here: which chunk is edited, when the
 * identical-text "not modified" reply still sends the tail, when a continuation
 * failure makes the send partial-but-retryable, and the exact raw_response the
 * stream consumer reads back. Telegram I/O, the chunker and the sent-text store
 * arrive as scripted values. Logger lines are emitted with the upstream format
 * strings; they are operator output, not part of the returned record.
 */
#include "noc_w20_telegramov_43149b.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TG_MAX_CHUNKS 64
#define TG_NAME 128

static char *tg_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static const char *tg_str(const json_t *obj, const char *key, const char *def) {
    if (!obj || obj->type != JSON_OBJECT) return def;
    return json_get_str(obj, key, def);
}

static const json_t *tg_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}

static json_t *tg_call(json_t *calls, const char *name) {
    json_t *c = json_object();
    json_set(c, "call", json_string(name));
    json_append(calls, c);
    return c;
}

/* Python's `str(exc).lower()` substring test. */
static int tg_contains_ci(const char *haystack, const char *needle) {
    size_t i, j, n, m;
    if (!haystack || !needle) return 0;
    n = strlen(haystack);
    m = strlen(needle);
    if (m == 0) return 1;
    if (m > n) return 0;
    for (i = 0; i + m <= n; i++) {
        for (j = 0; j < m; j++) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) break;
        }
        if (j == m) return 1;
    }
    return 0;
}

/* re.sub(r" \(\d+/\d+\)$", "", text) */
static void tg_strip_indicator(const char *text, char *out, size_t cap) {
    size_t n = strlen(text ? text : "");
    size_t i = n, start;
    snprintf(out, cap, "%s", text ? text : "");
    if (n < 4) return;
    if (out[n - 1] != ')') return;
    i = n - 1;
    while (i > 0 && isdigit((unsigned char)out[i - 1])) i--;      /* digits after '/' */
    if (i == n - 1) return;
    if (i == 0 || out[i - 1] != '/') return;
    i--;
    start = i;
    while (i > 0 && isdigit((unsigned char)out[i - 1])) i--;
    if (i == start) return;
    if (i == 0 || out[i - 1] != '(') return;
    i--;
    if (i == 0 || out[i - 1] != ' ') return;
    out[i - 1] = '\0';
}

typedef struct {
    const char *text[TG_MAX_CHUNKS];
    int count;
} tg_chunks;

static void tg_load_chunks(const json_t *split, const char *key, const char *content,
                           tg_chunks *out, int use_content_fallback) {
    const json_t *arr = tg_get(split, key);
    size_t i;
    out->count = 0;
    if (arr && arr->type == JSON_ARRAY) {
        for (i = 0; i < arr->c.count && out->count < TG_MAX_CHUNKS; i++) {
            const json_t *v = arr->c.items[i];
            if (v && v->type == JSON_STRING) out->text[out->count++] = v->str_val;
        }
    }
    if (use_content_fallback && out->count <= 1) {
        out->text[0] = content ? content : "";
        out->count = 1;
    }
}

/* adapter.format_message(chunk) -- identity in the harness adapter. */
static const char *tg_format(json_t *calls, const char *chunk) {
    json_t *c = tg_call(calls, "format_message");
    json_set(c, "text", json_string(chunk ? chunk : ""));
    return chunk ? chunk : "";
}

static const char *tg_fence(json_t *calls, const char *text) {
    json_t *c = tg_call(calls, "separate_chunk_indicator_from_fence");
    json_set(c, "text", json_string(text ? text : ""));
    return text ? text : "";
}

static const char *tg_strip_mdv2(json_t *calls, const char *text) {
    json_t *c = tg_call(calls, "_strip_mdv2");
    json_set(c, "text", json_string(text ? text : ""));
    return text ? text : "";
}

static const char *tg_quotable(json_t *calls, const char *text) {
    json_t *c = tg_call(calls, "quotable_text");
    json_set(c, "text", json_string(text ? text : ""));
    return text ? text : "";
}

static json_t *tg_send_result(int success, const char *message_id, const char *error,
                              json_t *raw, int retryable, const json_t *retry_after,
                              json_t *ids) {
    json_t *r = json_object();
    json_set(r, "success", json_bool(success ? 1 : 0));
    json_set(r, "message_id", message_id ? json_string(message_id) : json_null());
    json_set(r, "error", error ? json_string(error) : json_null());
    json_set(r, "raw_response", raw ? raw : json_null());
    json_set(r, "retryable", json_bool(retryable ? 1 : 0));
    json_set(r, "retry_after", retry_after ? json_copy(retry_after) : json_null());
    json_set(r, "continuation_message_ids", ids ? ids : json_array());
    json_set(r, "error_kind", json_null());
    return r;
}

json_t *w20_telegramov_43149b_edit_overflow_split(const char *chat_id,
                                                  const char *message_id,
                                                  const char *content,
                                                  const w20_telegramov_43149b_env *e) {
    json_t *out = json_object(), *calls = json_array(), *ids = json_array();
    json_t *result = NULL;
    tg_chunks chunks, raw_chunks;
    const json_t *first_edit, *conts;
    const char *adapter_name;
    const char *thread_id;
    const char *prev_id = message_id ? message_id : "";
    int i, delivered, total;
    int first_failed = 0;
    const char *first_error = NULL;

    json_set(out, "calls", calls);
    json_set(out, "result", json_null());

    adapter_name = tg_str(e ? e->adapter : NULL, "name", "telegram");
    thread_id = tg_str(e ? e->adapter : NULL, "thread_id", NULL);

    tg_load_chunks(e ? e->split : NULL, "chunks", content, &chunks, 1);
    tg_load_chunks(e ? e->split : NULL, "raw_chunks", content, &raw_chunks, 0);
    total = chunks.count;
    first_edit = e ? e->first_edit : NULL;
    conts = e ? e->continuations : NULL;

    /* First chunk: MarkdownV2-with-plain-fallback for a final, plain for a preview. */
    {
        const char *first_chunk = chunks.text[0];
        int raises = !strcmp(tg_str(first_edit, "outcome", "ok"), "raise");
        const char *error = tg_str(first_edit, "error", "");
        if (e && e->finalize) {
            const char *formatted = tg_format(calls, first_chunk);
            const char *fenced = tg_fence(calls, formatted);
            const char *plain = tg_strip_mdv2(calls, first_chunk);
            json_t *c = tg_call(calls, "edit_markdown_or_plain");
            json_set(c, "chat_id", json_string(chat_id ? chat_id : ""));
            json_set(c, "message_id", json_string(message_id ? message_id : ""));
            json_set(c, "text", json_string(fenced));
            json_set(c, "plain_text", json_string(plain));
            json_set(c, "log_format", json_string("[%s] Overflow split: MarkdownV2 first-chunk edit failed, falling back to plain text: %s"));
        } else {
            json_t *c = tg_call(calls, "edit_text");
            json_set(c, "chat_id", json_string(chat_id ? chat_id : ""));
            json_set(c, "message_id", json_string(message_id ? message_id : ""));
            json_set(c, "text", json_string(first_chunk));
        }
        if (raises && !tg_contains_ci(error, "not modified")) {
            first_failed = 1;
            first_error = error;
        }
    }
    if (first_failed) {
        /* logger.error("[%s] Overflow split: first-chunk edit failed: %s", name, redacted) */
        (void)adapter_name;
        result = tg_send_result(0, NULL, first_error, NULL, 0, NULL, NULL);
        json_set(out, "result", result);
        return out;
    }
    if (e && e->finalize) {
        /* quotable_text() is evaluated before _record_sent_text is entered. */
        const char *quoted = tg_quotable(calls, chunks.text[0]);
        json_t *c = tg_call(calls, "record_sent_text");
        json_set(c, "chat_id", json_string(chat_id ? chat_id : ""));
        json_set(c, "message_id", json_string(message_id ? message_id : ""));
        json_set(c, "text", json_string(quoted));
    }

    /* Continuations call _bot.send_message directly to skip adapter.send's pre-chunking. */
    {
        json_t *c = tg_call(calls, "metadata_thread_id");
        json_set(c, "thread_id", thread_id ? json_string(thread_id) : json_null());
    }

    for (i = 1; i < chunks.count; i++) {
        const json_t *spec = NULL;
        const char *chunk = chunks.text[i];
        long long reply_to = prev_id && prev_id[0] ? strtoll(prev_id, NULL, 10) : 0;
        int has_reply = prev_id && prev_id[0];
        const char *kind;
        json_t *c, *tk;

        tk = tg_call(calls, "thread_kwargs_for_send");
        json_set(tk, "chat_id", json_string(chat_id ? chat_id : ""));
        json_set(tk, "thread_id", thread_id ? json_string(thread_id) : json_null());
        if (has_reply) json_set(tk, "reply_to_message_id", json_number((double)reply_to));
        else json_set(tk, "reply_to_message_id", json_null());

        c = tg_call(calls, "send_overflow_continuation");
        json_set(c, "chat_id", json_string(chat_id ? chat_id : ""));
        json_set(c, "chunk", json_string(chunk));
        if (has_reply) json_set(c, "reply_to_id", json_number((double)reply_to));
        else json_set(c, "reply_to_id", json_null());
        json_set(c, "thread_id", thread_id ? json_string(thread_id) : json_null());
        json_set(c, "finalize", json_bool(e && e->finalize ? 1 : 0));

        if (conts && conts->type == JSON_ARRAY && (size_t)(i - 1) < conts->c.count) {
            spec = conts->c.items[i - 1];
        }
        kind = tg_str(spec, "kind", "ok");

        if (!strcmp(kind, "none") || !strcmp(kind, "flood")) {
            /* Partial delivery: do NOT report success. */
            int flood = !strcmp(kind, "flood");
            json_t *raw = json_object();
            char delivered_prefix[16384];
            const char *flood_error = tg_str(spec, "error", NULL);
            json_t *undelivered = NULL, *delivered_ids = NULL;
            size_t at = 0;

            delivered = 1 + i - 1;   /* the first chunk plus every continuation that landed */
            delivered_prefix[0] = '\0';
            {
                int k, lim = delivered < raw_chunks.count ? delivered : raw_chunks.count;
                for (k = 0; k < lim; k++) {
                    char stripped[4096];
                    tg_strip_indicator(raw_chunks.text[k], stripped, sizeof(stripped));
                    if (at + strlen(stripped) + 1 < sizeof(delivered_prefix)) {
                        memcpy(delivered_prefix + at, stripped, strlen(stripped) + 1);
                        at += strlen(stripped);
                    }
                }
            }
            json_set(raw, "partial_overflow", json_bool(1));
            json_set(raw, "delivered_chunks", json_number((double)delivered));
            json_set(raw, "total_chunks", json_number((double)total));
            json_set(raw, "last_message_id", json_string(prev_id ? prev_id : ""));
            json_set(raw, "delivered_prefix", json_string(delivered_prefix));
            json_set(raw, "continuation_message_ids", json_copy(ids));

            if (flood && e && e->finalize) {
                int k;
                undelivered = json_array();
                for (k = delivered; k < chunks.count; k++) {
                    const char *formatted = tg_format(calls, chunks.text[k]);
                    json_append(undelivered, json_string(tg_fence(calls, formatted)));
                }
                json_set(raw, "undelivered_chunks", undelivered);
                delivered_ids = json_array();
                json_append(delivered_ids, json_string(message_id ? message_id : ""));
                for (k = 0; k < (int)json_len(ids); k++) {
                    json_append(delivered_ids, json_copy(json_get(ids, (size_t)k)));
                }
                json_set(raw, "delivered_message_ids", delivered_ids);
            }
            result = tg_send_result(0, prev_id, flood ? flood_error : "overflow_continuation_failed",
                                    raw, 1, flood ? tg_get(spec, "retry_after") : NULL,
                                    json_copy(ids));
            json_set(out, "result", result);
            return out;
        }

        {
            const char *new_id = tg_str(spec, "message_id", "");
            if (!new_id || !new_id[0]) new_id = prev_id;
            if (e && e->finalize && strcmp(new_id, prev_id)) {
                const char *quoted = tg_quotable(calls, chunk);
                json_t *r = tg_call(calls, "record_sent_text");
                json_set(r, "chat_id", json_string(chat_id ? chat_id : ""));
                json_set(r, "message_id", json_string(new_id));
                json_set(r, "text", json_string(quoted));
            }
            json_append(ids, json_string(new_id));
            prev_id = new_id;
        }
    }

    /* success: message_id is the LAST id so later edits target the newest message */
    {
        const char *last_id = json_len(ids) ? json_get(ids, json_len(ids) - 1)->str_val : message_id;
        result = tg_send_result(1, last_id ? last_id : "", NULL, NULL, 0, NULL, json_copy(ids));
        json_set(out, "result", result);
    }
    return out;
}
