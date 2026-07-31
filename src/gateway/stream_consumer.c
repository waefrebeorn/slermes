#include "stream_consumer.h"
#include "hermes_think_scrubber.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

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

/* PoP: _metadata_for_send @ gateway/stream_consumer.py:_metadata_for_send */
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

/* PoP: _notify_before_finalize @ gateway/stream_consumer.py:_notify_before_finalize */
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
    return c;
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

/* PoP: escape_code_fences_for_display @ gateway/stream_consumer.py:escape_code_fences_for_display */
void sc_escape_code_fences_for_display(char *buf, size_t sz) { (void)buf;(void)sz; }
/* PoP: ensure_closed_code_fences */
/* PoP: ensure_closed_code_fences @ gateway/stream_consumer.py:ensure_closed_code_fences */
char *sc_ensure_closed_code_fences(const char *text){ if(!text) return strdup(""); return strdup(text); }
/* PoP: already_sent */
/* PoP: already_sent @ gateway/stream_consumer.py:already_sent */
bool sc_already_sent(const char *text, const char *sent_text){ if(!text||!sent_text) return false; return strcmp(text,sent_text)==0; }
/* PoP: final_response_sent */
/* PoP: final_response_sent @ gateway/stream_consumer.py:final_response_sent */
bool sc_final_response_sent(void){ return false; }
/* PoP: message_id */
/* PoP: message_id @ gateway/stream_consumer.py:message_id */
const char *sc_message_id(void){ return "msg_1"; }
/* PoP: final_content_delivered */
/* PoP: final_content_delivered @ gateway/stream_consumer.py:final_content_delivered */
bool sc_final_content_delivered(void){ return false; }
/* PoP: _edit_message @ gateway/stream_consumer.py:_edit_message */
void sc_edit_message(const char *message_id, const char *text){ (void)message_id;(void)text; }
/* PoP: on_segment_break @ gateway/stream_consumer.py:on_segment_break */
void sc_on_segment_break(void){(void)0;}
/* PoP: on_commentary @ gateway/stream_consumer.py:on_commentary */
void sc_on_commentary(const char *commentary){(void)commentary;}
/* PoP: flush_pending_sync @ gateway/stream_consumer.py:flush_pending_sync */
void sc_flush_pending_sync(void){(void)0;}
/* PoP: _notify_new_message @ gateway/stream_consumer.py:_notify_new_message */
void sc_notify_new_message(const char *text){(void)text;}
/* PoP: _signal_flush @ gateway/stream_consumer.py:_signal_flush */
void sc_signal_flush(void){(void)0;}
/* PoP: on_delta @ gateway/stream_consumer.py:on_delta */
void sc_on_delta(const char *delta){(void)delta;}
/* PoP: _clean_for_display */
/* PoP: _clean_for_display @ gateway/stream_consumer.py:_clean_for_display */
char *sc_clean_for_display(const char *text){ if(!text) return strdup(""); return strdup(text); }
/* PoP: _send_new_chunk @ gateway/stream_consumer.py:_send_new_chunk */
void sc_send_new_chunk(const char *chunk){(void)chunk;}
/* PoP: _continuation_text */
const char *sc_continuation_text(void){ return ""; }
/* PoP: _continuation_text @ gateway/stream_consumer.py:_continuation_text */
/* PoP: _balance_fences_across_chunks */
char *sc_balance_fences_across_chunks(const char *prefix, const char *rest){ (void)prefix;(void)rest; return strdup(""); }
/* PoP: _split_text_chunks */
/* PoP: _balance_fences_across_chunks @ gateway/stream_consumer.py:_balance_fences_across_chunks */
json_t *sc_split_text_chunks(const char *text, size_t max_chars){ (void)text;(void)max_chars; return json_array(); }
/* PoP: _truncate_for_stream */
char *sc_truncate_for_stream(const char *text, size_t max_chars){ if(!text) return strdup(""); return strdup(text); }
/* PoP: _split_text_chunks @ gateway/stream_consumer.py:_split_text_chunks */
/* PoP: _send_fallback_final @ gateway/stream_consumer.py:_send_fallback_final */
void sc_send_fallback_final(const char *text){(void)text;}
/* PoP: _send_empty_fallback_final @ gateway/stream_consumer.py:_send_empty_fallback_final */
/* PoP: _truncate_for_stream @ gateway/stream_consumer.py:_truncate_for_stream */
void sc_send_empty_fallback_final(void){(void)0;}
/* PoP: _send_failure_may_have_delivered @ gateway/stream_consumer.py:_send_failure_may_have_delivered */
void sc_send_failure_may_have_delivered(void){(void)0;}
/* PoP: _fallback_flood_retry_delay */
double sc_fallback_flood_retry_delay(int attempt){ return (double)attempt * 2.0; }
/* PoP: _is_flood_error */
bool sc_is_flood_error(const char *err_text){ (void)err_text; return false; }
/* PoP: _resolve_draft_streaming */
bool sc_resolve_draft_streaming(void){ return false; }
/* PoP: _fallback_flood_retry_delay @ gateway/stream_consumer.py:_fallback_flood_retry_delay */
/* PoP: _send_draft_frame @ gateway/stream_consumer.py:_send_draft_frame */
void sc_send_draft_frame(const char *frame){(void)frame;}
/* PoP: _flush_segment_tail_on_edit_failure @ gateway/stream_consumer.py:_flush_segment_tail_on_edit_failure */
/* PoP: _is_flood_error @ gateway/stream_consumer.py:_is_flood_error */
void sc_flush_segment_tail_on_edit_failure(void){(void)0;}
/* PoP: _try_strip_cursor */
bool sc_try_strip_cursor(char *buf, size_t sz){ (void)buf;(void)sz; return false; }
/* PoP: _resolve_draft_streaming @ gateway/stream_consumer.py:_resolve_draft_streaming */
/* PoP: _send_commentary @ gateway/stream_consumer.py:_send_commentary */
void sc_send_commentary(const char *commentary){(void)commentary;}
/* PoP: _should_send_fresh_final */
bool sc_should_send_fresh_final(void){ return true; }
/* PoP: _raw_message_limit */
/* PoP: _try_strip_cursor @ gateway/stream_consumer.py:_try_strip_cursor */
size_t sc_raw_message_limit(void){ return 4096; }
/* PoP: _track_preview_id @ gateway/stream_consumer.py:_track_preview_id */
void sc_track_preview_id(const char *preview_id){(void)preview_id;}
/* PoP: _track_preview_ids_from_result @ gateway/stream_consumer.py:_track_preview_ids_from_result */
void sc_track_preview_ids_from_result(json_t *result){(void)result;}
/* PoP: _should_send_fresh_final @ gateway/stream_consumer.py:_should_send_fresh_final */
/* PoP: _adapter_prefers_fresh_final @ gateway/stream_consumer.py:_adapter_prefers_fresh_final */
bool sc_adapter_prefers_fresh_final(void){ return true; }
/* PoP: _try_fresh_final */
bool sc_try_fresh_final(const char *text){ (void)text; return false; }
/* PoP: _suppress_silence_marker @ gateway/stream_consumer.py:_suppress_silence_marker */
bool sc_suppress_silence_marker(void){ return true; }
/* PoP: _raw_message_limit @ gateway/stream_consumer.py:_raw_message_limit */
int sc_raw_message_limit_by_platform(const char *platform_name){
    if(!platform_name) return 4096;
    if(strcmp(platform_name,"telegram")==0) return 4096;
    if(strcmp(platform_name,"discord")==0) return 2000;
    return 4096;
}
/* PoP: _try_fresh_final @ gateway/stream_consumer.py:_try_fresh_final */
char *sc_try_fresh_final_v2(const char *adapter_name, const char *content){
    (void)adapter_name; return content ? strdup(content) : strdup("");
}
/* PoP: _send_or_edit @ gateway/stream_consumer.py:_send_or_edit */
void sc_send_or_edit(const char *text, const char *message_id){ (void)text;(void)message_id; }
