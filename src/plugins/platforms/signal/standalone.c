/*
 * standalone.c -- real C11 port of the out-of-process Signal sender of upstream
 * Hermes plugins/platforms/signal/standalone.py.
 *
 * The live function is a retry loop around one JSON-RPC shape: batch the
 * attachments, meter each batch through the process-wide scheduler, retry a
 * rate-limited batch once (recalibrating the scheduler from the server's
 * Retry-After), and report per-batch losses. The transport is the only part
 * this port cannot own, so ``post`` is scripted by the caller: everything that
 * decides *what* goes on the wire -- chunking, the recipient shape, the attempt
 * budget, rate-limit classification, the returned payload -- is ported here.
 *
 * What stays upstream: httpx itself, the RPC payload's id/timestamp, the
 * inline pacing notice's text (agent.i18n), markdown_to_signal()'s styling, and
 * _error()'s secret redaction (the messages this port builds carry none).
 *
 * PoP: w20_standalone_ced4f0_send @ plugins/platforms/signal/standalone.py:_send_signal
 * PoP: w20_standalone_ced4f0_is_rate_limit @ plugins/platforms/signal/signal_rate_limit.py:_is_signal_rate_limit_error
 * PoP: w20_standalone_ced4f0_retry_after @ plugins/platforms/signal/signal_rate_limit.py:_extract_retry_after_seconds
 * PoP: w20_standalone_ced4f0_send_timeout @ plugins/platforms/signal/signal_rate_limit.py:_signal_send_timeout
 * PoP: w20_standalone_ced4f0_chat_id @ plugins/platforms/signal/standalone.py:_send_signal
 */
#include "hermes_json.h"
#include "noc_w20_standalone_ced4f0.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* --- growable text buffer (Python repr of an error value) --------------- */

typedef struct { char *buf; size_t len, cap; int oom; } sc_ced_buf;

static void c4f_put(sc_ced_buf *b, const char *s, size_t n) {
    if (b->oom) return;
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 128;
        char *nb;
        while (cap < b->len + n + 1) cap *= 2;
        nb = (char *)realloc(b->buf, cap);
        if (!nb) { b->oom = 1; return; }
        b->buf = nb;
        b->cap = cap;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void c4f_puts(sc_ced_buf *b, const char *s) { c4f_put(b, s, strlen(s)); }
static void c4f_putc(sc_ced_buf *b, char c) { c4f_put(b, &c, 1); }

static char *c4f_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static void c4f_repr(sc_ced_buf *b, const json_t *v);

/* repr(str): single-quoted, with Python's escapes for the characters that matter. */
static void c4f_repr_str(sc_ced_buf *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    char esc[8];
    c4f_putc(b, '\'');
    while (*p) {
        unsigned char c = *p;
        if (c == '\'')      { c4f_puts(b, "\\'"); p++; }
        else if (c == '\\') { c4f_puts(b, "\\\\"); p++; }
        else if (c == '\n') { c4f_puts(b, "\\n"); p++; }
        else if (c == '\r') { c4f_puts(b, "\\r"); p++; }
        else if (c == '\t') { c4f_puts(b, "\\t"); p++; }
        else if (c < 0x20 || c == 0x7f) { snprintf(esc, sizeof(esc), "\\x%02x", c); c4f_puts(b, esc); p++; }
        else { c4f_putc(b, (char)c); p++; }
    }
    c4f_putc(b, '\'');
}

static void c4f_repr_num(sc_ced_buf *b, double v) {
    char tmp[64];
    int p;
    if (v == floor(v) && fabs(v) < 9.0e15) {
        snprintf(tmp, sizeof(tmp), "%lld", (long long)v);
        c4f_puts(b, tmp);
        return;
    }
    for (p = 1; p <= 17; p++) {
        snprintf(tmp, sizeof(tmp), "%.*g", p, v);
        if (strtod(tmp, NULL) == v) break;
    }
    c4f_puts(b, tmp);
}

static void c4f_repr(sc_ced_buf *b, const json_t *v) {
    size_t i;
    if (!v) { c4f_puts(b, "None"); return; }
    switch (v->type) {
    case JSON_NULL:   c4f_puts(b, "None"); break;
    case JSON_BOOL:   c4f_puts(b, v->bool_val ? "True" : "False"); break;
    case JSON_NUMBER: c4f_repr_num(b, v->num_val); break;
    case JSON_STRING: c4f_repr_str(b, v->str_val); break;
    case JSON_ARRAY:
        c4f_putc(b, '[');
        for (i = 0; i < v->c.count; i++) {
            if (i) c4f_puts(b, ", ");
            c4f_repr(b, v->c.items[i]);
        }
        c4f_putc(b, ']');
        break;
    case JSON_OBJECT:
        c4f_putc(b, '{');
        for (i = 0; i < v->c.count; i++) {
            if (i) c4f_puts(b, ", ");
            c4f_repr_str(b, v->c.keys[i]);
            c4f_puts(b, ": ");
            c4f_repr(b, v->c.items[i]);
        }
        c4f_putc(b, '}');
        break;
    }
}

/* --- signal_rate_limit helpers ----------------------------------------- */

/* _error_message(err): the RPC error's "message", or str(err) for a bare value. */
static const char *c4f_error_message(const json_t *err) {
    if (err && err->type == JSON_OBJECT) {
        json_t *m = json_obj_get(err, "message");
        if (m && m->type == JSON_STRING && m->str_val) return m->str_val;
        return "";
    }
    if (err && err->type == JSON_STRING && err->str_val) return err->str_val;
    return "";
}

static int c4f_contains_ci(const char *hay, const char *needle) {
    size_t n = strlen(needle), i;
    if (!n) return 1;
    for (i = 0; hay[i]; i++) {
        size_t j;
        for (j = 0; j < n; j++) {
            char a = hay[i + j], b = needle[j];
            if (!a) return 0;
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
        }
        if (!needle[j]) return 1;
    }
    return 0;
}

int w20_standalone_ced4f0_is_rate_limit(const json_t *err) {
    const char *message;
    if (err && err->type == JSON_OBJECT) {
        json_t *code = json_obj_get(err, "code");
        if (code && code->type == JSON_NUMBER && code->num_val == -5.0) return 1;
    }
    message = c4f_error_message(err);
    if (strstr(message, "[429]")) return 1;
    return c4f_contains_ci(message, "ratelimit") ||
           c4f_contains_ci(message, "retrylaterexception") ||
           c4f_contains_ci(message, "retry after");
}

/* parse_retry_after_seconds(value): numeric or numeric string, clamped at 0. */
static int c4f_parse_seconds(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val < 0.0 ? 0.0 : v->num_val; return 1; }
    if (v->type == JSON_STRING && v->str_val) {
        char *end = NULL;
        double d = strtod(v->str_val, &end);
        if (end != v->str_val && end && *end == '\0') {
            *out = d < 0.0 ? 0.0 : d;
            return 1;
        }
    }
    return 0;
}

/* _RETRY_AFTER_RE = r"Retry after (\d+(?:\.\d+)?)\s*second" (IGNORECASE) */
static int c4f_retry_after_from_text(const char *text, double *out) {
    size_t i;
    for (i = 0; text[i]; i++) {
        if (!c4f_contains_ci(text + i, "retry after ")) continue;
        {
            const char *p = text + i + 12;
            const char *start = p;
            int digits = 0;
            double d;
            while (*p >= '0' && *p <= '9') { p++; digits = 1; }
            if (!digits) continue;
            if (*p == '.') {
                p++;
                while (*p >= '0' && *p <= '9') p++;
            }
            {
                char *tmp = (char *)malloc((size_t)(p - start) + 1);
                if (!tmp) return 0;
                memcpy(tmp, start, (size_t)(p - start));
                tmp[p - start] = '\0';
                while (*p == ' ' || *p == '\t') p++;
                if (!c4f_contains_ci(p, "second")) { free(tmp); continue; }
                d = strtod(tmp, NULL);
                free(tmp);
                *out = d < 0.0 ? 0.0 : d;
                return 1;
            }
        }
    }
    return 0;
}

int w20_standalone_ced4f0_retry_after(const json_t *err, double *out) {
    double best = 0.0;
    int found = 0;
    if (err && err->type == JSON_OBJECT) {
        json_t *data = json_obj_get(err, "data");
        json_t *response = (data && data->type == JSON_OBJECT) ? json_obj_get(data, "response") : NULL;
        json_t *results = (response && response->type == JSON_OBJECT)
                              ? json_obj_get(response, "results") : NULL;
        size_t i;
        for (i = 0; results && results->type == JSON_ARRAY && i < results->c.count; i++) {
            json_t *r = results->c.items[i];
            json_t *ra;
            double d;
            if (!r || r->type != JSON_OBJECT) continue;
            ra = json_obj_get(r, "retryAfterSeconds");
            if (!ra) continue;                       /* `if r.get(...)` truthiness */
            if (ra->type == JSON_NULL) continue;
            if (ra->type == JSON_NUMBER && ra->num_val == 0.0) continue;
            if (ra->type == JSON_STRING && (!ra->str_val || !ra->str_val[0])) continue;
            if (!c4f_parse_seconds(ra, &d)) continue;
            if (!found || d > best) best = d;
            found = 1;
        }
    }
    if (found) { *out = best; return 1; }
    if (c4f_retry_after_from_text(c4f_error_message(err), &best)) { *out = best; return 1; }
    return 0;
}

double w20_standalone_ced4f0_send_timeout(int num_attachments) {
    double scaled;
    if (num_attachments <= 0) return 30.0;
    scaled = 5.0 * (double)num_attachments;
    return scaled > 60.0 ? scaled : 60.0;
}

char *w20_standalone_ced4f0_chat_id(const char *chat_id) {
    const char *s = chat_id ? chat_id : "";
    if (!strncmp(s, "group:", 6)) return c4f_dup("group:***");
    return c4f_dup(s);
}

/* --- the send ----------------------------------------------------------- */

static json_t *c4f_error(const char *message) {
    json_t *out = json_object();
    json_set(out, "error", json_string(message ? message : ""));
    return out;
}

static json_t *c4f_call(const char *kind, size_t batch, size_t n) {
    json_t *c = json_object();
    json_set(c, "kind", json_string(kind));
    json_set(c, "batch", json_number((double)batch));
    json_set(c, "n", json_number((double)n));
    return c;
}

/* os.path.exists(path) */
static int c4f_exists(const char *path) {
    struct stat st;
    if (!path || !path[0]) return 0;
    return stat(path, &st) == 0;
}

json_t *w20_standalone_ced4f0_send(const json_t *extra, const char *chat_id, const json_t *media,
                                   const json_t *post_script, const json_t *estimates,
                                   int httpx_available) {
    json_t *out = json_object(), *result = NULL, *batches = json_array(),
           *calls = json_array(), *feedback = json_array(), *acquired = json_array(),
           *paths = json_array(), *failed = json_array(), *warnings = NULL;
    const char *account = json_get_str(extra, "account", "");
    const char *chat = chat_id ? chat_id : "";
    size_t nmedia = (media && media->type == JSON_ARRAY) ? media->c.count : 0;
    size_t npaths, nb, b, ps = 0, est_i = 0, i;

    if (!httpx_available) {
        json_set(out, "result", c4f_error("httpx not installed"));
        json_set(out, "batches", batches);
        json_set(out, "calls", calls);
        json_set(out, "feedback", feedback);
        json_set(out, "acquired", acquired);
        return out;
    }
    if (!account || !account[0]) {
        json_set(out, "result", c4f_error("Signal account not configured"));
        json_set(out, "batches", batches);
        json_set(out, "calls", calls);
        json_set(out, "feedback", feedback);
        json_set(out, "acquired", acquired);
        return out;
    }

    /* for media_path, _is_voice in valid_media: if os.path.exists(...) */
    for (i = 0; i < nmedia; i++) {
        json_t *entry = media->c.items[i];
        json_t *path;
        if (!entry || entry->type != JSON_ARRAY || entry->c.count < 1) continue;
        path = entry->c.items[0];
        if (!path || path->type != JSON_STRING || !path->str_val) continue;
        if (c4f_exists(path->str_val)) json_append(paths, json_string(path->str_val));
    }
    npaths = paths->c.count;

    /* att_batches = [...] or [[]] -- no attachments is still one text batch */
    nb = (npaths + W20_STANDALONE_CED4F0_MAX_ATTACHMENTS - 1) / W20_STANDALONE_CED4F0_MAX_ATTACHMENTS;
    if (nb == 0) nb = 1;
    for (b = 0; b < nb; b++) {
        json_t *bucket = json_array();
        for (i = b * W20_STANDALONE_CED4F0_MAX_ATTACHMENTS;
             i < npaths && i < (b + 1) * W20_STANDALONE_CED4F0_MAX_ATTACHMENTS; i++)
            json_append(bucket, json_copy(paths->c.items[i]));
        json_append(batches, bucket);
    }

    for (b = 0; b < nb && !result; b++) {
        json_t *batch = batches->c.items[b];
        size_t n = batch->c.count;
        int lost = 0, done = 0;
        int attempt;

        if (n > 0) {
            /* estimate_wait(n) >= SIGNAL_BATCH_PACING_NOTICE_THRESHOLD */
            double est = 0.0;
            if (estimates && estimates->type == JSON_ARRAY && est_i < estimates->c.count &&
                estimates->c.items[est_i]->type == JSON_NUMBER)
                est = estimates->c.items[est_i]->num_val;
            est_i++;
            if (est >= W20_STANDALONE_CED4F0_NOTICE_THRESHOLD) {
                /* One best-effort notice RPC, before the batch is attempted: a
                 * text-only RPC, so it carries no attachments and no batch. */
                json_t *call = json_object();
                json_set(call, "kind", json_string("notice"));
                json_set(call, "batch", json_null());
                json_set(call, "n", json_number(0));
                json_append(calls, call);
            }
        }

        for (attempt = 1; attempt <= W20_STANDALONE_CED4F0_MAX_ATTEMPTS && !done; attempt++) {
            json_t *entry = NULL;
            json_append(acquired, json_number((double)n));
            if (post_script && post_script->type == JSON_ARRAY && ps < post_script->c.count)
                entry = post_script->c.items[ps];
            ps++;
            /* One send RPC per attempt (a retried batch posts twice). */
            json_append(calls, c4f_call("send", b, n));

            if (entry && entry->type == JSON_OBJECT && json_obj_get(entry, "raise")) {
                /* except Exception: retry while attempts remain, else lose the batch */
                if (attempt >= W20_STANDALONE_CED4F0_MAX_ATTEMPTS) { lost = 1; done = 1; }
                continue;
            }
            if (entry && entry->type == JSON_OBJECT && json_obj_get(entry, "error")) {
                json_t *err = json_obj_get(entry, "error");
                if (!w20_standalone_ced4f0_is_rate_limit(err)) {
                    /* Non-rate-limit RPC error: the whole send fails here. */
                    sc_ced_buf msg = {NULL, 0, 0, 0};
                    char head[96];
                    char *text;
                    snprintf(head, sizeof(head), "Signal RPC error on batch %zu/%zu: ", b + 1, nb);
                    c4f_puts(&msg, head);
                    c4f_repr(&msg, err);
                    text = msg.oom ? NULL : msg.buf;
                    result = c4f_error(text ? text : "Signal RPC error");
                    free(msg.buf);
                    done = 1;
                    break;
                }
                {
                    double ra = 0.0;
                    int have = w20_standalone_ced4f0_retry_after(err, &ra);
                    json_t *fb = json_array();
                    json_append(fb, have ? json_number(ra) : json_null());
                    json_append(fb, json_number((double)n));
                    json_append(feedback, fb);
                }
                if (attempt >= W20_STANDALONE_CED4F0_MAX_ATTEMPTS) { lost = 1; done = 1; }
                continue;
            }
            /* No "error" in data: the batch landed. */
            done = 1;
        }
        if (lost && !result) json_append(failed, json_number((double)(b + 1)));
    }

    if (!result) {
        warnings = json_array();
        if (npaths < nmedia)
            json_append(warnings, json_string("Some media files were skipped (not found on disk)"));
        if (failed->c.count) {
            sc_ced_buf w = {NULL, 0, 0, 0};
            char head[64];
            snprintf(head, sizeof(head), "Signal rate-limited %zu batch(es) (#", failed->c.count);
            c4f_puts(&w, head);
            for (i = 0; i < failed->c.count; i++) {
                char num[32];
                if (i) c4f_puts(&w, ", #");
                snprintf(num, sizeof(num), "%lld", (long long)failed->c.items[i]->num_val);
                c4f_puts(&w, num);
            }
            c4f_puts(&w, ")");
            json_append(warnings, json_string(w.oom ? "" : w.buf));
            free(w.buf);
        }
        if (failed->c.count && failed->c.count == nb) {
            sc_ced_buf e = {NULL, 0, 0, 0};
            char text[96];
            snprintf(text, sizeof(text),
                     "Signal: every batch (%zu) hit rate limit; no attachments delivered", nb);
            c4f_puts(&e, text);
            result = c4f_error(e.oom ? "" : e.buf);
            free(e.buf);
        } else {
            json_t *success = json_object();
            char *safe = w20_standalone_ced4f0_chat_id(chat);
            json_set(success, "success", json_bool(1));
            json_set(success, "platform", json_string("signal"));
            json_set(success, "chat_id", json_string(safe ? safe : ""));
            free(safe);
            if (warnings->c.count) json_set(success, "warnings", json_copy(warnings));
            result = success;
        }
        json_free(warnings);
    }

    json_set(out, "result", result);
    json_set(out, "batches", batches);
    json_set(out, "calls", calls);
    json_set(out, "feedback", feedback);
    json_set(out, "acquired", acquired);
    json_free(paths);
    json_free(failed);
    return out;
}

