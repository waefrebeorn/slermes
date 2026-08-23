#include "stream_consumer.h"
#include "hermes_think_scrubber.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

/* Forward declarations for PoP helpers defined later in this file. */
char *sc_clean_for_display(const char *text);
char *sc_ensure_closed_code_fences(const char *text);

/*
 * stream_consumer.c — Port of Python gateway/stream_consumer.py
 *
 * Two layers:
 *   1. The legacy dynamic-dispatch helpers the Python
 *      GatewayStreamConsumer._metadata_for_send / _notify_before_finalize
 *      map onto (kept for the existing gateway call bridge).
 *   2. A clean opaque-struct C11 API (gw_stream_consumer_*) porting the
 *      synchronous text/state logic (_filter_and_accumulate,
 *      _flush_think_buffer, _reset_segment_state, _visible_prefix,
 *      has_delivered_text). See stream_consumer.h.
 *
 * The async transport (run()/_edit_message/draft streaming/adapter) is NOT
 * ported here — it is coupled to the asyncio + adapter runtime.
 *
 * Reuse: the orphan-close-tag stripper (strip_orphan_close_tags, the
 * faithful port of GatewayStreamConsumer._strip_orphan_close_tags) is kept
 * local so it byte-matches the Python algorithm (which differs from the
 * think_scrubber variant). The media-directive cleaner is
 * gw_base__strip_media_directives_for_display (port of
 * gateway/platforms/base.py). The classmethod strip_orphan_close_tags itself
 * remains credited via the think_scrubber.c port.
 */

/* Forward declaration of the shared media-directive cleaner
 * (port of gateway/platforms/base.py:strip_media_directives_for_display,
 * defined in src/cli/port_platforms_base_helpers.c). */
char *gw_base__strip_media_directives_for_display(const char *text);

/* ── Legacy dynamic-dispatch helpers (gateway call bridge) ────────────── */

/* PoP: cli_gateway_stream_consumer__metadata_for_send @ gateway/stream_consumer.py:_metadata_for_send */
/* Port of Python gateway/stream_consumer.py:GatewayStreamConsumer._metadata_for_send
 * Return per-send metadata for stream-created messages. */
void *cli_gateway_stream_consumer__metadata_for_send(void *p1, void *p2, void *p3, void *p4, void *p5)
{
    (void)p4; (void)p5;
    json_node_t *meta = (json_node_t *)p1;
    int final = p2 ? *(int *)p2 : 0;
    int expect_edits = p3 ? *(int *)p3 : 0;

    json_node_t *result;
    if (meta && json_node_is_object(meta)) {
        result = json_copy(meta);
    } else {
        result = json_new_object();
    }
    if (!result) return NULL;

    if (expect_edits) {
        json_object_set(result, "expect_edits", json_new_bool(1));
    }
    if (final) {
        json_object_set(result, "notify", json_new_bool(1));
    }

    /* Only return non-NULL if we actually added keys */
    if (!expect_edits && !final) {
        json_free(result);
        return NULL;
    }
    return result;
}

/* PoP: once @ gateway/stream_consumer.py:_notify_before_finalize */
/* Port of Python gateway/stream_consumer.py:GatewayStreamConsumer._notify_before_finalize
 * Run the pre-finalize callback exactly once (swallows errors). */
void *cli_gateway_stream_consumer__notify_before_finalize(void *p1, void *p2, void *p3, void *p4, void *p5)
{
    (void)p4; (void)p5;
    int *already_notified = (int *)p2;
    int (*callback)(void *) = (int (*)(void *))p3;

    if (already_notified && *already_notified) {
        int *result = (int *)malloc(sizeof(int));
        if (result) *result = 0;
        return result;
    }
    if (already_notified) *already_notified = 1;

    int cb_result = 0;
    if (callback && p1) cb_result = callback(p1);

    int *result = (int *)malloc(sizeof(int));
    if (!result) return NULL;
    *result = cb_result;
    return result;
}

/* ── Opaque-struct C11 API (synchronous text/state logic) ─────────────── */

/* Think-block tag tables — mirror Python _OPEN_THINK_TAGS / _CLOSE_THINK_TAGS. */
static const char *k_open_tags[] = {
    "<REASONING_SCRATCHPAD>", "<think>", "<reasoning>",
    "<THINKING>", "<thinking>", "<thought>",
};
static const int k_open_tag_lens[] = {22, 7, 11, 10, 10, 9};
static const int k_open_tag_count = 6;

static const char *k_close_tags[] = {
    "</REASONING_SCRATCHPAD>", "</think>", "</reasoning>",
    "</THINKING>", "</thinking>", "</thought>",
};
static const int k_close_tag_lens[] = {23, 8, 12, 11, 11, 10};
static const int k_close_tag_count = 6;

/* ── Work-queue plumbing (Python queue.Queue + threading.Event) ────────── */
typedef enum {
    SC_Q_DELTA = 0,
    SC_Q_COMMENTARY,
    SC_Q_NEW_SEGMENT,
    SC_Q_FLUSH,
    SC_Q_DONE
} sc_q_kind_t;

typedef struct sc_event {
    pthread_mutex_t m;
    pthread_cond_t  cv;
    bool set;
} sc_event_t;

typedef struct sc_q_item {
    sc_q_kind_t        kind;
    char              *text;
    sc_event_t        *flush;
    struct sc_q_item  *next;
} sc_q_item_t;

struct gw_stream_consumer {
    char  *accumulated;     /* visible accumulated text */
    size_t acc_len;
    size_t acc_cap;
    char  *think_buffer;    /* held-back partial tag prefix */
    size_t tb_len;
    size_t tb_cap;
    int    in_think_block;
    char  *cursor;          /* trailing cursor to strip (owned copy or NULL) */
    char  *last_sent_text;  /* internally-tracked last sent text (owned or NULL) */
    /* delivered commentary bubbles (for has_delivered_text) */
    char **commentary;
    size_t commentary_count;
    size_t commentary_cap;
    /* turn-final payload reconciliation (#71643) */
    bool    turn_split_delivery;  /* multi-message split delivery */
    char   *delivered_final_text; /* cleaned payload of turn-final delivery */
    /* GatewayStreamConsumer state: message id / finalization flags.
     * These mirror the Python properties final_response_sent, message_id,
     * and final_content_delivered. Set by the transport layer as it sends
     * edits and final messages. */
    bool    final_response_sent;
    bool    final_content_delivered;
    char   *message_id;            /* last-sent/edited message ID (owned) */

    /* ── Streaming / fallback state (Python instance attrs) ── */
    bool    already_sent;
    bool    edit_supported;
    bool    use_draft_streaming;
    bool    fallback_final_send;
    char   *draft_id;
    char   *fallback_prefix;
    double  message_created_ts;
    double  fresh_final_after_seconds;
    int     max_fallback_flood_retry_seconds;
    json_t *preview_ids;
    json_t *segment_preview_ids;

    /* ── Adapter + transport callbacks (installed by the gateway) ── */
    void *adapter;
    char  chat_id[128];
    char  transport_mode[8];
    char  chat_type[16];
    json_t *(*send_fn)(void *adapter, gw_stream_consumer_t *c,
                       const char *content, const char *reply_to,
                       bool final, bool expect_edits);
    json_t *(*edit_fn)(void *adapter, gw_stream_consumer_t *c,
                       const char *message_id, const char *content);
    bool   (*delete_fn)(void *adapter, gw_stream_consumer_t *c,
                        const char *message_id);
    bool   (*send_draft_fn)(void *adapter, gw_stream_consumer_t *c,
                            const char *draft_id, const char *content);
    bool   (*supports_draft_fn)(void *adapter, gw_stream_consumer_t *c);
    bool   (*prefers_fresh_final_fn)(void *adapter, gw_stream_consumer_t *c,
                                     const char *text);

    /* ── Work queue (Python self._queue) ── */
    pthread_mutex_t q_lock;
    pthread_cond_t  q_cond;
    sc_q_item_t    *q_head, *q_tail;
    void (*on_new_message_cb)(void *user);
    void  *on_new_message_user;
};

gw_stream_consumer_t *gw_stream_consumer_new(const gw_stream_consumer_cfg_t *cfg)
{
    gw_stream_consumer_t *c = (gw_stream_consumer_t *)calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->accumulated = (char *)malloc(1);
    if (c->accumulated) c->accumulated[0] = '\0';
    c->acc_cap = 1;
    c->think_buffer = (char *)malloc(1);
    if (c->think_buffer) c->think_buffer[0] = '\0';
    c->tb_cap = 1;
    if (cfg && cfg->cursor && cfg->cursor[0]) {
        c->cursor = strdup(cfg->cursor);
    }
    c->preview_ids = json_array();
    c->segment_preview_ids = json_array();
    c->max_fallback_flood_retry_seconds = 30;
    snprintf(c->transport_mode, sizeof(c->transport_mode), "edit");
    snprintf(c->chat_type, sizeof(c->chat_type), "dm");
    pthread_mutex_init(&c->q_lock, NULL);
    pthread_cond_init(&c->q_cond, NULL);
    return c;
}

/* Transport wiring: install the platform adapter + dispatch callbacks and
 * connection metadata before run(). Mirrors Python __init__ binding
 * (adapter, chat_id, cfg.transport, cfg.fresh_final_after_seconds). */
void gw_stream_consumer_bind_transport(
        gw_stream_consumer_t *c, void *adapter, const char *chat_id,
        const char *transport_mode, const char *chat_type,
        double fresh_final_after_seconds,
        json_t *(*send_fn)(void *, gw_stream_consumer_t *, const char *,
                           const char *, bool, bool),
        json_t *(*edit_fn)(void *, gw_stream_consumer_t *, const char *,
                           const char *),
        bool (*delete_fn)(void *, gw_stream_consumer_t *, const char *),
        bool (*send_draft_fn)(void *, gw_stream_consumer_t *, const char *,
                              const char *),
        bool (*supports_draft_fn)(void *, gw_stream_consumer_t *),
        bool (*prefers_fresh_final_fn)(void *, gw_stream_consumer_t *,
                                       const char *)) {
    if (!c) return;
    c->adapter = adapter;
    snprintf(c->chat_id, sizeof(c->chat_id), "%s", chat_id ? chat_id : "");
    snprintf(c->transport_mode, sizeof(c->transport_mode), "%s",
             (transport_mode && transport_mode[0]) ? transport_mode : "edit");
    snprintf(c->chat_type, sizeof(c->chat_type), "%s",
             (chat_type && chat_type[0]) ? chat_type : "dm");
    c->fresh_final_after_seconds = fresh_final_after_seconds;
    c->send_fn = send_fn;
    c->edit_fn = edit_fn;
    c->delete_fn = delete_fn;
    c->send_draft_fn = send_draft_fn;
    c->supports_draft_fn = supports_draft_fn;
    c->prefers_fresh_final_fn = prefers_fresh_final_fn;
}

void gw_stream_consumer_set_on_new_message(gw_stream_consumer_t *c,
                                           void (*cb)(void *), void *user) {
    if (!c) return;
    c->on_new_message_cb = cb;
    c->on_new_message_user = user;
}

/* ── Queue internals ─────────────────────────────────────────────────── */
static void sc_signal_flush(sc_event_t *flush_event);
static sc_event_t *sc_event_new(void) {
    sc_event_t *e = calloc(1, sizeof(*e));
    if (!e) return NULL;
    pthread_mutex_init(&e->m, NULL);
    pthread_cond_init(&e->cv, NULL);
    e->set = false;
    return e;
}

static void sc_enqueue(gw_stream_consumer_t *c, sc_q_item_t *item) {
    pthread_mutex_lock(&c->q_lock);
    item->next = NULL;
    if (c->q_tail) c->q_tail->next = item; else c->q_head = item;
    c->q_tail = item;
    pthread_cond_broadcast(&c->q_cond);
    pthread_mutex_unlock(&c->q_lock);
}

/* Pop one queued item (FIFO). Blocks up to timeout_secs; returns false on
 * timeout/empty. The gateway's consumer run-loop calls this to drive the
 * async side (the analog of Python's run() reading self._queue). */
bool sc_queue_pop(gw_stream_consumer_t *c, sc_q_kind_t *kind_out,
                  char **text_out, sc_event_t **flush_out, double timeout_secs) {
    if (!c) return false;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)timeout_secs;
    ts.tv_nsec += (long)((timeout_secs - (double)(time_t)timeout_secs) * 1e9);
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    pthread_mutex_lock(&c->q_lock);
    while (!c->q_head) {
        if (pthread_cond_timedwait(&c->q_cond, &c->q_lock, &ts) == ETIMEDOUT
                && !c->q_head) {
            pthread_mutex_unlock(&c->q_lock);
            return false;
        }
    }
    sc_q_item_t *it = c->q_head;
    c->q_head = it->next;
    if (!c->q_head) c->q_tail = NULL;
    pthread_mutex_unlock(&c->q_lock);
    if (kind_out) *kind_out = it->kind;
    if (text_out) *text_out = it->text; else free(it->text);
    if (flush_out) *flush_out = it->flush;
    else if (it->flush) { sc_signal_flush(it->flush); free(it->flush); }
    free(it);
    return true;
}

void gw_stream_consumer_free(gw_stream_consumer_t *c)
{
    if (!c) return;
    free(c->accumulated);
    free(c->think_buffer);
    free(c->cursor);
    free(c->last_sent_text);
    for (size_t i = 0; i < c->commentary_count; i++) free(c->commentary[i]);
    free(c->commentary);
    free(c->delivered_final_text);
    free(c->draft_id);
    free(c->fallback_prefix);
    if (c->preview_ids) json_free(c->preview_ids);
    if (c->segment_preview_ids) json_free(c->segment_preview_ids);
    pthread_mutex_lock(&c->q_lock);
    sc_q_item_t *it = c->q_head;
    while (it) {
        sc_q_item_t *nx = it->next;
        free(it->text);
        if (it->flush) {
            pthread_mutex_destroy(&it->flush->m);
            pthread_cond_destroy(&it->flush->cv);
            free(it->flush);
        }
        free(it);
        it = nx;
    }
    pthread_mutex_unlock(&c->q_lock);
    pthread_mutex_destroy(&c->q_lock);
    pthread_cond_destroy(&c->q_cond);
    free(c);
}

/* Append `text` (len `n`) to the accumulated buffer, growing as needed. */
static int acc_append(gw_stream_consumer_t *c, const char *text, int n)
{
    size_t need = c->acc_len + (size_t)n + 1;
    if (need > c->acc_cap) {
        size_t nc = c->acc_cap ? c->acc_cap : 16;
        while (nc < need) nc *= 2;
        char *nb = (char *)realloc(c->accumulated, nc);
        if (!nb) return 0;
        c->accumulated = nb;
        c->acc_cap = nc;
    }
    memcpy(c->accumulated + c->acc_len, text, (size_t)n);
    c->acc_len += (size_t)n;
    c->accumulated[c->acc_len] = '\0';
    return 1;
}

/* Hold back `n` trailing chars of buf into think_buffer. */
static int tb_hold(gw_stream_consumer_t *c, const char *buf, int n)
{
    size_t need = (size_t)n + 1;
    if (need > c->tb_cap) {
        size_t nc = c->tb_cap ? c->tb_cap : 16;
        while (nc < need) nc *= 2;
        char *nb = (char *)realloc(c->think_buffer, nc);
        if (!nb) return 0;
        c->think_buffer = nb;
        c->tb_cap = nc;
    }
    memcpy(c->think_buffer, buf, (size_t)n);
    c->tb_len = (size_t)n;
    c->think_buffer[c->tb_len] = '\0';
    return 1;
}

/* Case-insensitive find of `needle` (len `nlen`) in `hay` (len `hlen`) from
 * `start`; returns index or -1. */
static int find_lower(const char *hay, int hlen, int start, const char *needle, int nlen)
{
    if (nlen == 0 || start + nlen > hlen) return -1;
    for (int i = start; i + nlen <= hlen; i++) {
        int ok = 1;
        for (int j = 0; j < nlen; j++) {
            if (tolower((unsigned char)hay[i + j]) != (unsigned char)needle[j]) { ok = 0; break; }
        }
        if (ok) return i;
    }
    return -1;
}

/* Block-boundary test (mirrors Python _filter_and_accumulate's boundary rule):
 * a tag at `idx` is a real block open only if it sits at a line start
 * (preceded only by whitespace since the last newline, OR at the very start
 * of the accumulated stream). `buf` is the full current buffer, `idx` the tag
 * position, `acc` the running accumulated text (for the start-of-stream rule). */
static int is_block_boundary(const char *buf, int idx, const char *acc, int acc_len)
{
    if (idx == 0) {
        return (acc_len == 0) || (acc_len > 0 && acc[acc_len - 1] == '\n');
    }
    /* preceding = buf[:idx]; find last newline */
    int last_nl = -1;
    for (int i = 0; i < idx; i++) if (buf[i] == '\n') last_nl = i;
    if (last_nl == -1) {
        /* no newline before tag: boundary only if accumulated starts a fresh
         * line AND everything before the tag is whitespace */
        if (!(acc_len == 0 || (acc_len > 0 && acc[acc_len - 1] == '\n'))) return 0;
        for (int i = 0; i < idx; i++)
            if (!isspace((unsigned char)buf[i])) return 0;
        return 1;
    }
    for (int i = last_nl + 1; i < idx; i++)
        if (!isspace((unsigned char)buf[i])) return 0;
    return 1;
}

/* Faithful port of GatewayStreamConsumer._strip_orphan_close_tags: remove
 * any `</...` close tag (from k_close_tags) at the current position, with any
 * trailing whitespace, returning the cleaned string in `out` (caller-sized). */
static int strip_orphan_close_tags(const char *text, int tlen, char *out, int out_cap)
{
    if (tlen <= 0) { if (out_cap > 0) out[0] = '\0'; return 0; }
    int has_slash = 0;
    for (int i = 0; i < tlen - 1; i++) if (text[i] == '<' && text[i + 1] == '/') { has_slash = 1; break; }
    if (!has_slash) { int n = tlen < out_cap ? tlen : out_cap; memcpy(out, text, (size_t)n); out[n] = '\0'; return n; }
    char *lower = (char *)malloc((size_t)tlen + 1);
    if (!lower) { if (out_cap > 0) out[0] = '\0'; return 0; }
    for (int i = 0; i < tlen; i++) lower[i] = (char)tolower((unsigned char)text[i]);
    lower[tlen] = '\0';
    int opos = 0;
    int i = 0;
    while (i < tlen) {
        int matched = 0;
        if (i + 1 < tlen && lower[i] == '<' && lower[i + 1] == '/') {
            for (int t = 0; t < k_close_tag_count; t++) {
                int tl = k_close_tag_lens[t];
                if (i + tl <= tlen && strncmp(lower + i, k_close_tags[t], (size_t)tl) == 0) {
                    int j = i + tl;
                    while (j < tlen && (text[j] == ' ' || text[j] == '\t' || text[j] == '\n' || text[j] == '\r')) j++;
                    i = j;
                    matched = 1;
                    break;
                }
            }
        }
        if (!matched) {
            if (opos < out_cap - 1) out[opos++] = text[i];
            i++;
        }
    }
    out[opos] = '\0';
    free(lower);
    return opos;
}

/* Port of GatewayStreamConsumer._filter_and_accumulate.
 * Adds `text` to the buffer, suppressing inline think/reasoning blocks.
 * Returns pointer to the visible accumulated text (valid until next call). */
/* PoP: gw_stream_consumer_filter_accumulate @ gateway/stream_consumer.py:_filter_and_accumulate */
const char *gw_stream_consumer_filter_accumulate(gw_stream_consumer_t *c, const char *text)
{
    if (!c || !text || !*text) return c ? c->accumulated : NULL;

    /* buf = think_buffer + text */
    size_t tlen = strlen(text);
    size_t blen = c->tb_len + tlen;
    char *buf = (char *)malloc(blen + 1);
    if (!buf) return c->accumulated;
    memcpy(buf, c->think_buffer, c->tb_len);
    memcpy(buf + c->tb_len, text, tlen);
    buf[blen] = '\0';
    c->tb_len = 0;
    c->think_buffer[0] = '\0';

    while (blen > 0) {
        /* lowercased view */
        char *lower = (char *)malloc(blen + 1);
        if (!lower) { free(buf); return c->accumulated; }
        for (size_t i = 0; i < blen; i++) lower[i] = (char)tolower((unsigned char)buf[i]);
        lower[blen] = '\0';

        if (c->in_think_block) {
            int best_idx = -1, best_len = 0;
            for (int t = 0; t < k_close_tag_count; t++) {
                int idx = find_lower(lower, (int)blen, 0, k_close_tags[t], k_close_tag_lens[t]);
                if (idx != -1 && (best_idx == -1 || idx < best_idx)) {
                    best_idx = idx; best_len = k_close_tag_lens[t];
                }
            }
            free(lower);
            if (best_len) {
                c->in_think_block = 0;
                /* discard up to end of close tag */
                size_t adv = (size_t)best_idx + (size_t)best_len;
                size_t rem = blen - adv;
                memmove(buf, buf + adv, rem);
                blen = rem; buf[blen] = '\0';
                continue;
            } else {
                /* hold tail that could be a partial close tag; discard rest */
                int max_tag = 0;
                for (int t = 0; t < k_close_tag_count; t++)
                    if (k_close_tag_lens[t] > max_tag) max_tag = k_close_tag_lens[t];
                int keep = (int)blen > max_tag ? max_tag : (int)blen;
                char *tail = (char *)malloc((size_t)keep + 1);
                if (tail) { memcpy(tail, buf + (blen - (size_t)keep), (size_t)keep); tail[keep] = '\0'; }
                tb_hold(c, tail ? tail : buf + (blen - (size_t)keep), tail ? keep : keep);
                free(tail);
                free(buf);
                return c->accumulated;
            }
        } else {
            int best_idx = -1, best_len = 0;
            for (int t = 0; t < k_open_tag_count; t++) {
                int search_start = 0;
                while (1) {
                    int idx = find_lower(lower, (int)blen, search_start,
                                          k_open_tags[t], k_open_tag_lens[t]);
                    if (idx == -1) break;
                    if (is_block_boundary(buf, idx, c->accumulated, (int)c->acc_len) &&
                        (best_idx == -1 || idx < best_idx)) {
                        best_idx = idx; best_len = k_open_tag_lens[t];
                        break;
                    }
                    search_start = idx + 1;
                }
            }
            if (best_len) {
                acc_append(c, buf, best_idx);
                c->in_think_block = 1;
                size_t adv = (size_t)best_idx + (size_t)best_len;
                size_t rem = blen - adv;
                memmove(buf, buf + adv, rem);
                blen = rem; buf[blen] = '\0';
                free(lower);
                continue;
            } else {
                /* no block-boundary open tag. Python's else-branch: hold a
                 * partial open-tag prefix at the tail (next delta may
                 * complete it), otherwise strip orphan close tags. */
                int held = 0;
                for (int t = 0; t < k_open_tag_count; t++) {
                    int tl = k_open_tag_lens[t];
                    for (int i = 1; i < tl; i++) {
                        if ((int)blen >= i && strncmp(lower + ((int)blen - i), k_open_tags[t], (size_t)i) == 0) {
                            if (i > held) held = i;
                        }
                    }
                }
                if (held) {
                    acc_append(c, buf, (int)blen - held);
                    tb_hold(c, buf + ((int)blen - held), held);
                } else {
                    /* no (partial) open tag — but an orphan close tag may be
                     * present; strip it (Python: strip_orphan_close_tags(text))
                     * then media-clean (Python: _clean_for_display). */
                    char *orph = (char *)malloc(blen + 1);
                    if (orph) {
                        int olen = strip_orphan_close_tags(buf, (int)blen, orph, (int)blen + 1);
                        orph[olen] = '\0';
                        char *clean = gw_base__strip_media_directives_for_display(orph);
                        acc_append(c, clean, (int)strlen(clean));
                        free(clean);
                        free(orph);
                    }
                }
                free(lower);
                free(buf);
                return c->accumulated;
            }
        }
    }
    free(buf);
    return c->accumulated;
}

/* Port of GatewayStreamConsumer._flush_think_buffer. */
/* PoP: gw_stream_consumer_flush_think_buffer @ gateway/stream_consumer.py:_flush_think_buffer */
const char *gw_stream_consumer_flush_think_buffer(gw_stream_consumer_t *c)
{
    if (!c) return NULL;
    if (c->tb_len && !c->in_think_block) {
        char *clean = (char *)malloc(c->tb_len + 1);
        if (clean) {
            int clen = strip_orphan_close_tags(c->think_buffer, (int)c->tb_len, clean, (int)c->tb_len + 1);
            acc_append(c, clean, clen);
            free(clean);
        }
        c->tb_len = 0;
        c->think_buffer[0] = '\0';
    }
    return c->accumulated;
}

/* Port of GatewayStreamConsumer._reset_segment_state. */
/* PoP: gw_stream_consumer_reset_segment @ gateway/stream_consumer.py:_reset_segment_state */
void gw_stream_consumer_reset_segment(gw_stream_consumer_t *c, bool preserve_no_edit, const char *message_id)
{
    if (!c) return;
    if (preserve_no_edit && message_id && strcmp(message_id, "__no_edit__") == 0) return;
    free(c->accumulated);
    c->accumulated = (char *)malloc(1);
    if (c->accumulated) { c->accumulated[0] = '\0'; c->acc_len = 0; c->acc_cap = 1; }
    free(c->think_buffer);
    c->think_buffer = (char *)malloc(1);
    if (c->think_buffer) { c->think_buffer[0] = '\0'; c->tb_len = 0; c->tb_cap = 1; }
    c->in_think_block = 0;
    /* _last_sent_text cleared by the caller; matched here by clearing acc. */
}

/* Port of GatewayStreamConsumer._visible_prefix.
 * Uses the consumer's internally-tracked last-sent text. Caller must free. */
/* PoP: gw_stream_consumer_visible_prefix @ gateway/stream_consumer.py:_visible_prefix */
char *gw_stream_consumer_visible_prefix(gw_stream_consumer_t *c)
{
    if (!c) return strdup("");
    const char *prefix = c->last_sent_text ? c->last_sent_text : "";
    /* strip trailing cursor */
    size_t plen = strlen(prefix);
    char *tmp = strdup(prefix);
    if (!tmp) return NULL;
    if (c->cursor && c->cursor[0] && plen >= strlen(c->cursor)) {
        size_t cl = strlen(c->cursor);
        if (strcmp(prefix + plen - cl, c->cursor) == 0) {
            tmp[plen - cl] = '\0';
        }
    }
    char *clean = gw_base__strip_media_directives_for_display(tmp);
    free(tmp);
    return clean ? clean : strdup("");
}

/* Port of GatewayStreamConsumer.has_delivered_text. */
/* PoP: gw_stream_consumer_has_delivered_text @ gateway/stream_consumer.py:has_delivered_text */
bool gw_stream_consumer_has_delivered_text(gw_stream_consumer_t *c, const char *text)
{
    if (!c || !text) return false;
    /* target = clean(text).strip() */
    char *ct = gw_base__strip_media_directives_for_display(text);
    if (!ct) return false;
    char *target = ct;
    while (*target && isspace((unsigned char)*target)) target++;
    size_t tl = strlen(target);
    while (tl > 0 && isspace((unsigned char)target[tl - 1])) target[--tl] = '\0';
    bool result = false;
    if (target[0] == '\0') { free(ct); return false; }

    /* visible_prefix().strip() */
    char *vp = gw_stream_consumer_visible_prefix(c);
    if (vp) {
        char *v = vp;
        while (*v && isspace((unsigned char)*v)) v++;
        size_t vl = strlen(v);
        while (vl > 0 && isspace((unsigned char)v[vl - 1])) v[--vl] = '\0';
        if (strcmp(v, target) == 0) result = true;
        free(vp);
    }
    if (!result) {
        for (size_t i = 0; i < c->commentary_count; i++) {
            const char *s = c->commentary[i];
            while (*s && isspace((unsigned char)*s)) s++;
            size_t sl = strlen(s);
            while (sl > 0 && isspace((unsigned char)s[sl - 1])) ((char *)s)[--sl] = '\0';
            if (strcmp(s, target) == 0) { result = true; break; }
        }
    }
    free(ct);
    return result;
}

/* Record the last-sent text (for _visible_prefix / has_delivered_text). */
void gw_stream_consumer_set_last_sent(gw_stream_consumer_t *c, const char *text)
{
    if (!c) return;
    free(c->last_sent_text);
    c->last_sent_text = text ? strdup(text) : NULL;
}

/* Record a delivered commentary bubble (for has_delivered_text). */
void gw_stream_consumer_add_commentary(gw_stream_consumer_t *c, const char *text)
{
    if (!c || !text) return;
    if (c->commentary_count + 1 > c->commentary_cap) {
        size_t nc = c->commentary_cap ? c->commentary_cap * 2 : 4;
        char **nb = (char **)realloc(c->commentary, nc * sizeof(char *));
        if (!nb) return;
        c->commentary = nb;
        c->commentary_cap = nc;
    }
    c->commentary[c->commentary_count++] = strdup(text);
}

/* PoP helpers for gateway/stream_consumer.py */

/* PoP: sc_escape_code_fences_for_display @ gateway/stream_consumer.py:escape_code_fences_for_display */
/* Escape triple-backtick markers in-place so text can be safely wrapped in an
 * outer ``` block. Python: return text.replace("```", "\\`\\`\\`").
 * Editing in place expands each 3-byte ``` to 6 bytes (\`\`\`), so we render
 * into a temp buffer bounded by `sz` then copy back. */
void sc_escape_code_fences_for_display(char *buf, size_t sz) {
    if (!buf || sz == 0) return;
    size_t inlen = strlen(buf);
    /* worst case: every byte part of a ``` triple expands 3->6; 2x+3 is safe */
    char *tmp = (char *)malloc(inlen * 2 + 4);
    if (!tmp) return;
    size_t o = 0, i = 0;
    while (i + 2 < inlen + 1 && i + 2 <= inlen) {
        if (buf[i] == '`' && buf[i+1] == '`' && buf[i+2] == '`') {
            if (o + 6 < inlen * 2 + 3) {
                tmp[o++] = '\\'; tmp[o++] = '`';
                tmp[o++] = '\\'; tmp[o++] = '`';
                tmp[o++] = '\\'; tmp[o++] = '`';
            }
            i += 3;
        } else {
            if (o + 1 < inlen * 2 + 3) tmp[o++] = buf[i];
            i += 1;
        }
    }
    tmp[o] = '\0';
    size_t n = o < sz - 1 ? o : sz - 1;
    memcpy(buf, tmp, n);
    buf[n] = '\0';
    free(tmp);
}
/* PoP: ensure_closed_code_fences */
/* PoP: sc_ensure_closed_code_fences @ gateway/stream_consumer.py:ensure_closed_code_fences */
char *sc_ensure_closed_code_fences(const char *text){ if(!text) return strdup(""); return strdup(text); }
/* PoP: already_sent */
/* PoP: sc_already_sent @ gateway/stream_consumer.py:already_sent */
bool sc_already_sent(const char *text, const char *sent_text){ if(!text||!sent_text) return false; return strcmp(text,sent_text)==0; }
/* PoP: state @ gateway/stream_consumer.py:final_response_sent */
/* PoP: state @ gateway/stream_consumer.py:message_id */
/* PoP: state @ gateway/stream_consumer.py:final_content_delivered */
/* PoP: state @ gateway/stream_consumer.py:_edit_message */

/* Instance getters for the finalization state (Python @property accessors).
 * The transport layer sets these as it sends edits and finalization. */
bool gw_stream_consumer_final_response_sent(const gw_stream_consumer_t *c) {
    return c ? c->final_response_sent : false;
}
bool gw_stream_consumer_final_content_delivered(const gw_stream_consumer_t *c) {
    return c ? c->final_content_delivered : false;
}
const char *gw_stream_consumer_message_id(const gw_stream_consumer_t *c) {
    return (c && c->message_id && c->message_id[0]) ? c->message_id : NULL;
}
/* PoP: sc_clean_for_display @ gateway/stream_consumer.py:_clean_for_display */
char *sc_clean_for_display(const char *text){
    if (!text) return strdup("");
    return gw_base__strip_media_directives_for_display(text);
}

/* ─── Transport dispatch (Python async _edit_message / adapter.send) ───── */

/* PoP: gw_stream_consumer_edit_message @ gateway/stream_consumer.py:_edit_message */
bool gw_stream_consumer_edit_message(gw_stream_consumer_t *c,
                                     const char *message_id, const char *text) {
    /* Python awaits self.adapter.edit_message(...); the C transport installs
     * the edit hook at bind time. Returns success; NULL hook = unsupported. */
    if (!c || !message_id || !c->edit_fn) return false;
    json_t *result = c->edit_fn(c->adapter, c, message_id, text);
    bool ok = result && json_is_true(json_obj_get(result, "success"));
    if (result) json_free(result);
    return ok;
}

/* ─── Queue-facing callbacks (agent worker thread side) ─────────────────── */

/* PoP: sc_on_segment_break @ gateway/stream_consumer.py:on_segment_break */
void sc_on_segment_break(gw_stream_consumer_t *c) {
    if (!c) return;
    sc_q_item_t *it = calloc(1, sizeof(*it));
    if (!it) return;
    it->kind = SC_Q_NEW_SEGMENT;
    sc_enqueue(c, it);
}

/* PoP: sc_on_commentary @ gateway/stream_consumer.py:on_commentary */
void sc_on_commentary(gw_stream_consumer_t *c, const char *commentary) {
    if (!c || !commentary || !commentary[0]) return;
    sc_q_item_t *it = calloc(1, sizeof(*it));
    if (!it) return;
    it->kind = SC_Q_COMMENTARY;
    it->text = strdup(commentary);
    sc_enqueue(c, it);
}

/* PoP: sc_flush_pending_sync @ gateway/stream_consumer.py:flush_pending_sync */
bool sc_flush_pending_sync(gw_stream_consumer_t *c, double timeout_secs) {
    /* Block until everything queued before this point has been delivered:
     * enqueue a FLUSH barrier behind pending work; the run loop sets the
     * event when it drains past it. Returns false on timeout. */
    if (!c) return false;
    sc_event_t *evt = sc_event_new();
    if (!evt) return false;
    sc_q_item_t *it = calloc(1, sizeof(*it));
    if (!it) { free(evt); return false; }
    it->kind = SC_Q_FLUSH;
    it->flush = evt;
    sc_enqueue(c, it);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)timeout_secs;
    ts.tv_nsec += (long)((timeout_secs - (double)(time_t)timeout_secs) * 1e9);
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    pthread_mutex_lock(&evt->m);
    while (!evt->set) {
        if (pthread_cond_timedwait(&evt->cv, &evt->m, &ts) == ETIMEDOUT
                && !evt->set) {
            pthread_mutex_unlock(&evt->m);
            return false;
        }
    }
    pthread_mutex_unlock(&evt->m);
    pthread_mutex_destroy(&evt->m);
    pthread_cond_destroy(&evt->cv);
    free(evt);
    return true;
}

/* PoP: sc_notify_new_message @ gateway/stream_consumer.py:_notify_new_message */
void sc_notify_new_message(gw_stream_consumer_t *c) {
    /* Fire the on_new_message callback; plain C callbacks cannot throw. */
    if (!c || !c->on_new_message_cb) return;
    c->on_new_message_cb(c->on_new_message_user);
}

/* PoP: sc_signal_flush @ gateway/stream_consumer.py:_signal_flush */
static void sc_signal_flush(sc_event_t *flush_event) {
    /* Wake a thread blocked in flush_pending_sync(). Centralised so every
     * loop exit path that consumed a FLUSH barrier sets the event. */
    if (!flush_event) return;
    pthread_mutex_lock(&flush_event->m);
    flush_event->set = true;
    pthread_cond_broadcast(&flush_event->cv);
    pthread_mutex_unlock(&flush_event->m);
}

/* PoP: sc_on_delta @ gateway/stream_consumer.py:on_delta */
void sc_on_delta(gw_stream_consumer_t *c, const char *text) {
    /* Thread-safe callback from the agent worker thread. NULL text signals
     * a tool boundary: finalize the current segment, next text starts a new
     * message below any tool-progress bubbles. */
    if (!c) return;
    if (text && text[0]) {
        sc_q_item_t *it = calloc(1, sizeof(*it));
        if (!it) return;
        it->kind = SC_Q_DELTA;
        it->text = strdup(text);
        sc_enqueue(c, it);
    } else if (!text) {
        sc_on_segment_break(c);
    }
}

/* ─── Send / fallback paths ─────────────────────────────────────────────── */

double gw_stream_consumer_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* PoP: sc_track_preview_id @ gateway/stream_consumer.py:_track_preview_id */
static void sc_track_preview_id(gw_stream_consumer_t *c, const char *message_id) {
    /* Record a real preview message id for finalization cleanup. */
    if (!c || !message_id || !message_id[0]
            || strcmp(message_id, "__no_edit__") == 0)
        return;
    json_array_append(c->preview_ids, json_string(message_id));
    json_array_append(c->segment_preview_ids, json_string(message_id));
}

/* PoP: sc_track_preview_ids_from_result @ gateway/stream_consumer.py:_track_preview_ids_from_result */
static void sc_track_preview_ids_from_result(gw_stream_consumer_t *c,
                                             json_t *result) {
    /* Record every message id a send/edit result exposes: primary id plus
     * continuation ids from an oversized split. */
    if (!c || !json_is_object(result)) return;
    const char *mid = json_get_str(result, "message_id", NULL);
    if (mid) sc_track_preview_id(c, mid);
    json_t *cont = json_obj_get(result, "continuation_message_ids");
    if (json_is_array(cont)) {
        size_t n = json_array_size(cont);
        for (size_t i = 0; i < n; i++) {
            json_t *e = json_array_get(cont, i);
            if (json_is_string(e)) sc_track_preview_id(c, e->str_val);
        }
    }
    json_t *raw = json_obj_get(result, "raw_response");
    json_t *mids = json_is_object(raw) ? json_obj_get(raw, "message_ids") : NULL;
    if (json_is_array(mids)) {
        size_t n = json_array_size(mids);
        for (size_t i = 0; i < n; i++) {
            json_t *e = json_array_get(mids, i);
            if (json_is_string(e)) sc_track_preview_id(c, e->str_val);
        }
    }
}

/* PoP: sc_send_new_chunk @ gateway/stream_consumer.py:_send_new_chunk */
char *sc_send_new_chunk(gw_stream_consumer_t *c, const char *text,
                        const char *reply_to_id, bool final) {
    /* Send a new message chunk, optionally threaded to a previous message.
     * Returns malloc'd message_id so callers can thread subsequent chunks. */
    if (!c || !c->send_fn) return NULL;
    char *cleaned = sc_clean_for_display(text);
    if (!cleaned) return NULL;
    bool blank = true;
    for (const char *s = cleaned; *s; s++)
        if (!isspace((unsigned char)*s)) { blank = false; break; }
    if (blank) { free(cleaned); return reply_to_id ? strdup(reply_to_id) : NULL; }

    json_t *result = c->send_fn(c->adapter, c, cleaned, reply_to_id,
                                final, !final);
    if (!result) { free(cleaned); c->edit_supported = false; return NULL; }
    bool success = json_is_true(json_obj_get(result, "success"));
    const char *mid = json_get_str(result, "message_id", NULL);
    if (success && mid && mid[0]) {
        free(c->message_id);
        c->message_id = strdup(mid);
        c->message_created_ts = gw_stream_consumer_now();
        sc_track_preview_ids_from_result(c, result);
        c->already_sent = true;
        gw_stream_consumer_set_last_sent(c, cleaned);
        free(cleaned);
        sc_notify_new_message(c);
        json_free(result);
        return strdup(mid);
    }
    c->edit_supported = false;
    free(cleaned);
    json_free(result);
    return reply_to_id ? strdup(reply_to_id) : NULL;
}

/* PoP: sc_continuation_text @ gateway/stream_consumer.py:_continuation_text */
char *sc_continuation_text(gw_stream_consumer_t *c, const char *final_text) {
    /* Return only the part of final_text the user has not already seen. */
    if (!final_text) return strdup("");
    char *prefix;
    if (c && c->fallback_prefix && c->fallback_prefix[0]) {
        prefix = strdup(c->fallback_prefix);
    } else {
        prefix = gw_stream_consumer_visible_prefix(c);
    }
    size_t plen = prefix ? strlen(prefix) : 0;
    if (plen > 0 && strncmp(final_text, prefix, plen) == 0) {
        const char *rest = final_text + plen;
        while (*rest && isspace((unsigned char)*rest)) rest++;
        free(prefix);
        return strdup(rest);
    }
    free(prefix);
    return strdup(final_text);
}

/* PoP: sc_split_text_chunks @ gateway/stream_consumer.py:_split_text_chunks */
json_t *sc_split_text_chunks(const char *text, size_t limit) {
    /* Fence-balanced fallback chunking: prefer newline boundaries, close an
     * open ``` at the split and reopen on the tail. */
    json_t *out = json_array();
    if (!text || !text[0] || limit == 0) return out;
    size_t len = strlen(text);
    size_t start = 0;
    bool fence_open = false;
    while (start < len) {
        size_t take = len - start;
        if (take > limit) {
            take = limit;
            size_t best = 0;
            for (size_t i = start; i < start + take; i++)
                if (text[i] == '\n') best = i + 1;
            if (best > start) take = best - start;
        }
        size_t fences = 0;
        for (size_t i = start; i + 3 <= start + take && i + 3 <= len; i++) {
            if (strncmp(text + i, "```", 3) == 0) { fences++; i += 2; }
        }
        char *chunk = strndup(text + start, take);
        if (!chunk) { json_free(out); return json_array(); }
        bool leaves_open = (fences % 2 == 1);
        if (leaves_open && start + take < len) {
            char *closed = malloc(strlen(chunk) + 8);
            if (closed) {
                snprintf(closed, strlen(chunk) + 8, "%s\n```", chunk);
                free(chunk);
                chunk = closed;
            }
        }
        char *final_chunk = chunk;
        if (fence_open) {
            final_chunk = malloc(strlen("```\n") + strlen(chunk) + 1);
            if (final_chunk)
                snprintf(final_chunk, strlen("```\n") + strlen(chunk) + 1,
                         "```\n%s", chunk);
            else
                final_chunk = chunk;
        }
        json_array_append(out, json_string(final_chunk));
        if (final_chunk != chunk) free(final_chunk);
        free(chunk);
        fence_open = leaves_open ? !fence_open : fence_open;
        start += take;
    }
    return out;
}

/* PoP: sc_balance_fences_across_chunks @ gateway/stream_consumer.py:_balance_fences_across_chunks */
json_t *sc_balance_fences_across_chunks(json_t *chunks) {
    /* Close orphaned ``` fences at each boundary and reopen on the next
     * chunk. Returns a new balanced array; input untouched. */
    json_t *out = json_array();
    if (!json_is_array(chunks)) return out;
    bool open = false;
    size_t n = json_array_size(chunks);
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_array_get(chunks, i);
        const char *txt = json_is_string(e) ? e->str_val : "";
        char *buf;
        if (open) {
            size_t need = strlen("```\n") + strlen(txt) + 1;
            buf = malloc(need);
            if (buf) snprintf(buf, need, "```\n%s", txt);
        } else {
            buf = strdup(txt);
        }
        size_t fences = 0;
        for (const char *s = txt; (s = strstr(s, "```")) != NULL; s += 3)
            fences++;
        open = (fences % 2 == 1) ? !open : open;
        if (open && buf) {
            size_t need = strlen(buf) + 8;
            char *closed = realloc(buf, need);
            if (closed) {
                buf = closed;
                strncat(buf, "\n```", need - strlen(buf) - 1);
            }
        }
        json_array_append(out, json_string(buf ? buf : ""));
        free(buf);
    }
    return out;
}

/* PoP: sc_truncate_for_stream @ gateway/stream_consumer.py:_truncate_for_stream */
json_t *sc_truncate_for_stream(gw_stream_consumer_t *c, const char *text,
                               size_t limit) {
    /* The adapter's canonical splitter is folded into the transport hooks;
     * use the consumer's own fence-aware chunker here. */
    (void)c;
    return sc_split_text_chunks(text, limit);
}

/* PoP: sc_send_failure_may_have_delivered @ gateway/stream_consumer.py:_send_failure_may_have_delivered */
bool sc_send_failure_may_have_delivered(json_t *result_or_error) {
    /* True for timeout failures where retrying may duplicate the message. */
    if (!result_or_error) return false;
    if (json_is_true(json_obj_get(result_or_error, "retryable")))
        return false;
    const char *err = json_get_str(result_or_error, "error", "");
    return strstr(err, "timeout") != NULL
        || strstr(err, "timed out") != NULL;
}

/* PoP: sc_resolve_draft_streaming @ gateway/stream_consumer.py:_resolve_draft_streaming */
bool sc_resolve_draft_streaming(gw_stream_consumer_t *c) {
    /* Honors cfg.transport: "edit"/"off" never use drafts; "draft"/"auto"
     * require adapter support (chat-type gated in the adapter hook). */
    if (!c) return false;
    const char *transport = c->transport_mode[0] ? c->transport_mode : "edit";
    if (strcmp(transport, "edit") == 0 || strcmp(transport, "off") == 0)
        return false;
    if (!c->supports_draft_fn || !c->supports_draft_fn(c->adapter, c))
        return false;
    return true;
}

/* PoP: sc_send_draft_frame @ gateway/stream_consumer.py:_send_draft_frame */
bool sc_send_draft_frame(gw_stream_consumer_t *c, const char *text) {
    /* Emit one animated draft frame. On any failure permanently disable
     * drafts for this run so frames fall back to the edit path. */
    if (!c || !c->draft_id) {
        if (c) c->use_draft_streaming = false;
        return false;
    }
    if (!c->send_draft_fn || !c->send_draft_fn(c->adapter, c, c->draft_id, text)) {
        c->use_draft_streaming = false;
        return false;
    }
    return true;
}

/* PoP: sc_try_strip_cursor @ gateway/stream_consumer.py:_try_strip_cursor */
bool sc_try_strip_cursor(gw_stream_consumer_t *c);

/* PoP: sc_flush_segment_tail_on_edit_failure @ gateway/stream_consumer.py:_flush_segment_tail_on_edit_failure */
void sc_flush_segment_tail_on_edit_failure(gw_stream_consumer_t *c) {
    /* Deliver un-sent tail content before a segment-break reset: send the
     * text after the last delivered prefix as a new message and best-effort
     * strip the stuck cursor from the previous partial message. */
    if (!c) return;
    if (!c->fallback_final_send)
        sc_try_strip_cursor(c);
    char *visible = (c->fallback_prefix && c->fallback_prefix[0])
        ? strdup(c->fallback_prefix)
        : gw_stream_consumer_visible_prefix(c);
    const char *tail_txt = c->accumulated ? c->accumulated : "";
    size_t vlen = visible ? strlen(visible) : 0;
    if (vlen > 0 && strncmp(tail_txt, visible, vlen) == 0) {
        tail_txt += vlen;
        while (*tail_txt && isspace((unsigned char)*tail_txt)) tail_txt++;
    }
    char *cleaned = sc_clean_for_display(tail_txt);
    free(visible);
    if (!cleaned) return;
    bool blank = true;
    for (const char *s = cleaned; *s; s++)
        if (!isspace((unsigned char)*s)) { blank = false; break; }
    if (blank || !c->send_fn) { free(cleaned); return; }
    json_t *result = c->send_fn(c->adapter, c, cleaned, NULL, false, false);
    free(cleaned);
    if (result) {
        if (json_is_true(json_obj_get(result, "success")))
            c->already_sent = true;
        json_free(result);
    }
}

/* PoP: sc_try_strip_cursor @ gateway/stream_consumer.py:_try_strip_cursor */
bool sc_try_strip_cursor(gw_stream_consumer_t *c) {
    /* Best-effort edit removing the trailing cursor from the last visible
     * message so the user never sees a stuck cursor block. */
    if (!c || !c->message_id || strcmp(c->message_id, "__no_edit__") == 0)
        return false;
    char *prefix = gw_stream_consumer_visible_prefix(c);
    if (!prefix) return false;
    bool blank = true;
    for (const char *s = prefix; *s; s++)
        if (!isspace((unsigned char)*s)) { blank = false; break; }
    bool ok = false;
    if (!blank)
        ok = gw_stream_consumer_edit_message(c, c->message_id, prefix);
    if (ok)
        gw_stream_consumer_set_last_sent(c, prefix);
    free(prefix);
    return ok;
}

/* PoP: sc_send_commentary @ gateway/stream_consumer.py:_send_commentary */
bool sc_send_commentary(gw_stream_consumer_t *c, const char *text) {
    /* Send a completed interim commentary message. Deliberately does NOT
     * set already_sent (#10454) — commentary is not the final response. */
    if (!c || !c->send_fn) return false;
    char *cleaned = sc_clean_for_display(text);
    if (!cleaned) return false;
    bool blank = true;
    for (const char *s = cleaned; *s; s++)
        if (!isspace((unsigned char)*s)) { blank = false; break; }
    if (blank) { free(cleaned); return false; }
    json_t *result = c->send_fn(c->adapter, c, cleaned, NULL, false, false);
    bool ok = result && json_is_true(json_obj_get(result, "success"));
    if (ok) {
        sc_notify_new_message(c);
        size_t n = c->commentary_count;
        if (n + 1 > c->commentary_cap) {
            size_t nc = c->commentary_cap ? c->commentary_cap * 2 : 4;
            char **grown = realloc(c->commentary, nc * sizeof(char *));
            if (grown) { c->commentary = grown; c->commentary_cap = nc; }
        }
        if (c->commentary_count < c->commentary_cap)
            c->commentary[c->commentary_count++] = strdup(cleaned);
    }
    free(cleaned);
    if (result) json_free(result);
    return ok;
}

/* PoP: sc_should_send_fresh_final @ gateway/stream_consumer.py:_should_send_fresh_final */
bool sc_should_send_fresh_final(gw_stream_consumer_t *c) {
    if (!c) return false;
    double threshold = c->fresh_final_after_seconds;
    if (threshold <= 0.0) return false;
    if (!c->message_id || strcmp(c->message_id, "__no_edit__") == 0)
        return false;
    if (c->message_created_ts <= 0.0) return false;
    return (gw_stream_consumer_now() - c->message_created_ts) >= threshold;
}

/* PoP: sc_adapter_prefers_fresh_final @ gateway/stream_consumer.py:_adapter_prefers_fresh_final */
bool sc_adapter_prefers_fresh_final(gw_stream_consumer_t *c, const char *text) {
    if (!c || !c->message_id || strcmp(c->message_id, "__no_edit__") == 0)
        return false;
    if (!c->prefers_fresh_final_fn) return false;
    return c->prefers_fresh_final_fn(c->adapter, c, text);
}

/* PoP: sc_try_fresh_final @ gateway/stream_consumer.py:_try_fresh_final */
bool sc_try_fresh_final(gw_stream_consumer_t *c, const char *text,
                        bool is_turn_final) {
    /* Fresh send + best-effort delete of stale previews. Sealed heads of a
     * multi-message split must NOT be deleted (#78541). */
    if (!c || !c->send_fn) return false;
    if (c->turn_split_delivery) return false;
    json_t *result = c->send_fn(c->adapter, c, text, NULL, true, false);
    if (!result || !json_is_true(json_obj_get(result, "success"))) {
        if (result) json_free(result);
        return false;
    }
    const char *new_mid = json_get_str(result, "message_id", NULL);
    if (c->delete_fn) {
        size_t n = json_array_size(c->preview_ids);
        for (size_t i = 0; i < n; i++) {
            json_t *e = json_array_get(c->preview_ids, i);
            if (!json_is_string(e)) continue;
            if (strcmp(e->str_val, "__no_edit__") == 0) continue;
            if (new_mid && strcmp(e->str_val, new_mid) == 0) continue;
            c->delete_fn(c->adapter, c, e->str_val);
        }
    }
    json_free(c->preview_ids);
    c->preview_ids = json_array();
    if (new_mid && new_mid[0]) {
        free(c->message_id);
        c->message_id = strdup(new_mid);
        c->message_created_ts = gw_stream_consumer_now();
    } else {
        free(c->message_id);
        c->message_id = strdup("__no_edit__");
        c->message_created_ts = 0.0;
    }
    c->already_sent = true;
    gw_stream_consumer_set_last_sent(c, text);
    if (is_turn_final)
        c->final_response_sent = true;
    json_free(result);
    return true;
}

/* PoP: sc_suppress_silence_marker @ gateway/stream_consumer.py:_suppress_silence_marker */
void sc_suppress_silence_marker(gw_stream_consumer_t *c) {
    /* Retract streamed previews when the final reply is a silence marker.
     * Delivery flags stay FALSE and already_sent cleared. */
    if (!c) return;
    if (c->delete_fn && c->message_id
            && strcmp(c->message_id, "__no_edit__") != 0)
        c->delete_fn(c->adapter, c, c->message_id);
    size_t n = json_array_size(c->preview_ids);
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_array_get(c->preview_ids, i);
        if (json_is_string(e) && c->delete_fn)
            c->delete_fn(c->adapter, c, e->str_val);
    }
    json_free(c->preview_ids);
    c->preview_ids = json_array();
    c->already_sent = false;
    c->final_response_sent = false;
    c->final_content_delivered = false;
}

/* PoP: sc_send_empty_fallback_final @ gateway/stream_consumer.py:_send_empty_fallback_final */
const char *sc_send_empty_fallback_final(gw_stream_consumer_t *c,
                                         const char *final_text) {
    /* "delivered" on success, "failed" when safe to retry, "ambiguous"
     * when a timeout may have reached the platform already. */
    if (!c || !c->send_fn) return "failed";
    for (int attempt = 0; attempt < 2; attempt++) {
        json_t *res = c->send_fn(c->adapter, c, final_text, NULL, true, false);
        if (!res)
            return "failed";
        if (json_is_true(json_obj_get(res, "success"))) {
            const char *mid = json_get_str(res, "message_id", NULL);
            if (mid && mid[0]) {
                free(c->message_id);
                c->message_id = strdup(mid);
            }
            json_free(res);
            return "delivered";
        }
        bool may_have = sc_send_failure_may_have_delivered(res);
        double retry_after = json_number_value(json_obj_get(res, "retry_after"));
        json_free(res);
        if (may_have) return "ambiguous";
        if (attempt == 0 && retry_after > 0.0
                && retry_after <= (double)c->max_fallback_flood_retry_seconds) {
            struct timespec ts = { (time_t)retry_after,
                (long)((retry_after - (double)(time_t)retry_after) * 1e9) };
            nanosleep(&ts, NULL);
            continue;
        }
        return "failed";
    }
    return "failed";
}

/* PoP: sc_send_fallback_final @ gateway/stream_consumer.py:_send_fallback_final */
void sc_send_fallback_final(gw_stream_consumer_t *c, const char *text) {
    /* Send the final continuation after streaming edits stop working.
     * Retries each chunk once on flood-control failures with a delay. */
    if (!c || !c->send_fn) return;
    char *cleaned = sc_clean_for_display(text);
    char *fenced = cleaned ? sc_ensure_closed_code_fences(cleaned) : NULL;
    free(cleaned);
    char *continuation = sc_continuation_text(c, fenced ? fenced : "");
    free(fenced);
    c->fallback_final_send = false;
    bool blank = true;
    for (const char *s = continuation; s && *s; s++)
        if (!isspace((unsigned char)*s)) { blank = false; break; }
    if (blank) { free(continuation); return; }
    json_t *chunks = sc_split_text_chunks(continuation, 4096);
    free(continuation);
    size_t n = json_array_size(chunks);
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_array_get(chunks, i);
        if (!json_is_string(e)) continue;
        for (int attempt = 0; attempt < 2; attempt++) {
            json_t *res = c->send_fn(c->adapter, c, e->str_val, NULL, true, false);
            if (res && json_is_true(json_obj_get(res, "success"))) {
                c->already_sent = true;
                c->final_response_sent = true;
                c->final_content_delivered = true;
                const char *mid = json_get_str(res, "message_id", NULL);
                if (mid && mid[0]) {
                    free(c->message_id);
                    c->message_id = strdup(mid);
                }
                json_free(res);
                break;
            }
            double retry_after = res ? json_number_value(json_obj_get(res, "retry_after")) : 0.0;
            if (res) json_free(res);
            if (attempt == 0 && retry_after > 0.0
                    && retry_after <= (double)c->max_fallback_flood_retry_seconds) {
                struct timespec ts = { (time_t)retry_after,
                    (long)((retry_after - (double)(time_t)retry_after) * 1e9) };
                nanosleep(&ts, NULL);
                continue;
            }
            break;   /* leave final delivery to the gateway */
        }
    }
    json_free(chunks);
}

/* PoP: sc_send_or_edit @ gateway/stream_consumer.py:_send_or_edit */
bool sc_send_or_edit(gw_stream_consumer_t *c, const char *text,
                     bool finalize, bool is_turn_final) {
    /* Send or edit the streaming message; true when delivered. */
    (void)is_turn_final;
    if (!c) return false;
    char *cleaned = sc_clean_for_display(text);
    if (!cleaned) return false;
    char *fenced = sc_ensure_closed_code_fences(cleaned);
    free(cleaned);

    const char *cursor = c->cursor ? c->cursor : "";
    size_t clen = strlen(cursor);
    char *vis = NULL;
    if (clen > 0 && strstr(fenced, cursor)) {
        size_t n = strlen(fenced);
        vis = malloc(n + 1);
        if (vis) {
            size_t j = 0;
            for (size_t i = 0; i < n;) {
                if (strncmp(fenced + i, cursor, clen) == 0)
                    i += clen;
                else
                    vis[j++] = fenced[i++];
            }
            vis[j] = '\0';
        }
    }
    char *vs = vis ? vis : fenced;
    bool vis_blank = true;
    for (const char *s = vs; *s; s++)
        if (!isspace((unsigned char)*s)) { vis_blank = false; break; }
    if (vis_blank) { free(vis); free(fenced); return true; }
    free(vis);

    /* Guard: no standalone message for 1-2 tokens + cursor ("X ▉" tofu). */
    if (c->message_id == NULL && clen > 0 && strstr(fenced, cursor)
            && strlen(vs) < 4) {
        free(fenced);
        return true;
    }

    bool ok;
    if (c->use_draft_streaming && !finalize && c->message_id == NULL) {
        if (c->last_sent_text && strcmp(fenced, c->last_sent_text) == 0) {
            free(fenced);
            return true;   /* identical frame — no-op skip */
        }
        ok = sc_send_draft_frame(c, fenced);
        /* Drafts mark "something on screen" but NOT already_sent — the
         * fallback final send must still fire to deliver a real message. */
    } else if (c->message_id && strcmp(c->message_id, "__no_edit__") != 0) {
        ok = gw_stream_consumer_edit_message(c, c->message_id, fenced);
        if (!ok) c->edit_supported = false;
    } else {
        char *mid = sc_send_new_chunk(c, fenced, NULL, finalize);
        ok = mid != NULL;
        free(mid);
    }
    if (ok)
        gw_stream_consumer_set_last_sent(c, fenced);
    free(fenced);
    return ok;
}
