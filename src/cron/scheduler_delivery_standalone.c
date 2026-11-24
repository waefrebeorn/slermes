/*
 * scheduler_delivery_standalone.c - real C11 port of upstream Hermes
 *   cron/scheduler_delivery_standalone.py.
 *
 * The two entry points are the lane's decisions: what the out-of-process send
 * answers (result / error / in-flight ambiguity) and whether a payload the live
 * lane rejected retryably goes to the delivery ledger instead. The transport
 * itself (asyncio.run, the sender, the ledger store) is the seam: it arrives as
 * an outcome and leaves as a recorded request.
 *
 * PoP: w19_schedulerd_4551a7_standalone_send @ cron/scheduler_delivery_standalone.py:_standalone_send
 * PoP: w19_schedulerd_4551a7_queue_for_reconnect @ cron/scheduler_delivery_standalone.py:_queue_for_live_reconnect
 */
#include "hermes_json.h"
#include "noc_w19_schedulerd_4551a7.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Live-lane rejections an adapter returns BEFORE sending anything. */
static const char *const PRE_SEND_LIVE_REJECTIONS[] = { "Not connected", NULL };

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static void lower_copy(const char *s, char *out, size_t n) {
    size_t i = 0;
    if (!s) s = "";
    for (; s[i] && i + 1 < n; i++) {
        char c = s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[i] = '\0';
}

/* is_reconnect_only(error): str(error or "").strip().lower() == "send_path_degraded" */
static int is_reconnect_only(const char *error) {
    char buf[512];
    lower_copy(error ? error : "", buf, sizeof buf);
    char *p = buf;
    while (*p && is_ws(*p)) p++;
    size_t len = strlen(p);
    while (len && is_ws(p[len - 1])) len--;
    p[len] = '\0';
    return !strcmp(p, "send_path_degraded");
}

static int in_pre_send_rejections(const char *error) {
    if (!error) return 0;
    for (int i = 0; PRE_SEND_LIVE_REJECTIONS[i]; i++)
        if (!strcmp(error, PRE_SEND_LIVE_REJECTIONS[i])) return 1;
    return 0;
}

/* content.strip() is empty */
static int is_blank(const char *s) {
    if (!s) return 1;
    while (*s && is_ws(*s)) s++;
    return *s == '\0';
}

/* ---------- SHA-256 (compute_obligation_id's digest) ---------- */

typedef struct {
    unsigned int h[8];
    unsigned long long total;
    unsigned char buf[64];
    size_t used;
} sha256_ctx;

static const unsigned int SHA_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

#define ROR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_init(sha256_ctx *c) {
    c->h[0] = 0x6a09e667u; c->h[1] = 0xbb67ae85u; c->h[2] = 0x3c6ef372u; c->h[3] = 0xa54ff53au;
    c->h[4] = 0x510e527fu; c->h[5] = 0x9b05688cu; c->h[6] = 0x1f83d9abu; c->h[7] = 0x5be0cd19u;
    c->total = 0; c->used = 0;
}

static void sha256_block(sha256_ctx *c, const unsigned char *p) {
    unsigned int w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((unsigned int)p[i * 4] << 24) | ((unsigned int)p[i * 4 + 1] << 16) |
               ((unsigned int)p[i * 4 + 2] << 8) | (unsigned int)p[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
        unsigned int s0 = ROR32(w[i - 15], 7) ^ ROR32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        unsigned int s1 = ROR32(w[i - 2], 17) ^ ROR32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    unsigned int a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3];
    unsigned int e = c->h[4], f = c->h[5], g = c->h[6], hh = c->h[7];
    for (int i = 0; i < 64; i++) {
        unsigned int S1 = ROR32(e, 6) ^ ROR32(e, 11) ^ ROR32(e, 25);
        unsigned int ch = (e & f) ^ ((~e) & g);
        unsigned int t1 = hh + S1 + ch + SHA_K[i] + w[i];
        unsigned int S0 = ROR32(a, 2) ^ ROR32(a, 13) ^ ROR32(a, 22);
        unsigned int maj = (a & b) ^ (a & cc) ^ (b & cc);
        unsigned int t2 = S0 + maj;
        hh = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d;
    c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += hh;
}

static void sha256_update(sha256_ctx *c, const unsigned char *data, size_t len) {
    c->total += (unsigned long long)len;
    while (len) {
        size_t room = 64 - c->used;
        size_t take = len < room ? len : room;
        memcpy(c->buf + c->used, data, take);
        c->used += take;
        data += take;
        len -= take;
        if (c->used == 64) { sha256_block(c, c->buf); c->used = 0; }
    }
}

static void sha256_final(sha256_ctx *c, unsigned char out[32]) {
    unsigned long long bits = c->total * 8;
    unsigned char pad = 0x80;
    unsigned char zero = 0;
    unsigned char lenbuf[8];
    sha256_update(c, &pad, 1);
    while (c->used != 56) sha256_update(c, &zero, 1);
    for (int i = 0; i < 8; i++) lenbuf[i] = (unsigned char)(bits >> (56 - 8 * i));
    sha256_update(c, lenbuf, 8);
    for (int i = 0; i < 8; i++) {
        out[i * 4] = (unsigned char)(c->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(c->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(c->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)(c->h[i]);
    }
}

/* compute_obligation_id: sha256(f"{session_key}|{message_ref}|{content}")[:24] */
static void obligation_id(const char *session_key, const char *message_ref, const char *content,
                          char out[25]) {
    sha256_ctx c;
    static const char hexd[] = "0123456789abcdef";
    unsigned char digest[32];
    const char *parts[5];
    parts[0] = session_key ? session_key : "";
    parts[1] = "|";
    parts[2] = message_ref ? message_ref : "";
    parts[3] = "|";
    parts[4] = content ? content : "";
    sha256_init(&c);
    for (int i = 0; i < 5; i++) sha256_update(&c, (const unsigned char *)parts[i], strlen(parts[i]));
    sha256_final(&c, digest);
    for (int i = 0; i < 12; i++) {
        out[i * 2] = hexd[(digest[i] >> 4) & 0xF];
        out[i * 2 + 1] = hexd[digest[i] & 0xF];
    }
    out[24] = '\0';
}

/* ---------- exported decisions ---------- */

json_t *w19_schedulerd_4551a7_standalone_send(const char *where, const char *content, int media_count,
                                              double send_timeout, double base_timeout,
                                              int shutting_down, const char *outcome,
                                              const char *exc_text, int request_never_left,
                                              int in_flight0, int shutdown_on_retry,
                                              json_t *ok_result) {
    const char *w = where ? where : "";
    json_t *out = json_object();
    json_t *result = NULL;
    char err[1400];
    int in_flight = in_flight0 ? 1 : 0;
    err[0] = '\0';

    if (shutting_down) {
        snprintf(err, sizeof err, "delivery to %s skipped \xe2\x80\x94 interpreter is shutting down", w);
    } else if (is_blank(content) && media_count <= 0) {
        snprintf(err, sizeof err, "standalone send skipped (empty text and no media) for %s", w);
    } else if (!outcome || !strcmp(outcome, "ok")) {
        result = ok_result ? json_copy(ok_result) : json_null();
    } else if (!strcmp(outcome, "timeout")) {
        snprintf(err, sizeof err,
                 "standalone send to %s timed out after %gs (the send may still be in flight)", w, send_timeout);
        in_flight = 1;
    } else if (!strcmp(outcome, "error")) {
        snprintf(err, sizeof err, "delivery to %s failed: %s", w, exc_text ? exc_text : "");
        if (!request_never_left) in_flight = 1;
    } else if (!strcmp(outcome, "runtime_ok")) {
        if (shutdown_on_retry)
            snprintf(err, sizeof err, "delivery to %s skipped \xe2\x80\x94 interpreter is shutting down", w);
        else
            result = ok_result ? json_copy(ok_result) : json_null();
    } else if (!strcmp(outcome, "runtime_timeout")) {
        double seconds = 30.0 + send_timeout - base_timeout;
        snprintf(err, sizeof err,
                 "standalone send to %s timed out after %gs (the send may still be in flight)", w, seconds);
        in_flight = 1;
    } else if (!strcmp(outcome, "runtime_error")) {
        if (shutdown_on_retry)
            snprintf(err, sizeof err, "delivery to %s skipped \xe2\x80\x94 interpreter is shutting down", w);
        else {
            snprintf(err, sizeof err, "delivery to %s failed: %s", w, exc_text ? exc_text : "");
            if (!request_never_left) in_flight = 1;
        }
    } else {
        result = ok_result ? json_copy(ok_result) : json_null();
    }

    json_set(out, "result", result ? result : json_null());
    json_set(out, "error", err[0] ? json_string(err) : json_null());
    json_set(out, "in_flight", json_bool(in_flight ? true : false));
    return out;
}

json_t *w19_schedulerd_4551a7_queue_for_reconnect(const char *where, const char *platform_name,
                                                 const char *chat_id, const char *thread_id,
                                                 const char *job_id, const char *content,
                                                 int media_count, const char *live_error,
                                                 int live_retryable, int ledger_enabled,
                                                 int has_loop, int has_schedule, int record_raises,
                                                 const char *adapter_profile) {
    json_t *out = json_object();
    const char *w = where ? where : "";
    const char *plat = platform_name ? platform_name : "";
    const char *thread = (thread_id && thread_id[0]) ? thread_id : NULL;
    int queued = 0;
    char session_key[1024];
    char oid[32];
    char note[1200];
    int scheduled = 0;

    session_key[0] = '\0';
    oid[0] = '\0';
    note[0] = '\0';

    int retryable_rejection = is_reconnect_only(live_error) ||
                             (live_retryable && in_pre_send_rejections(live_error));
    if (retryable_rejection && ledger_enabled) {
        if (thread)
            snprintf(session_key, sizeof session_key, "cron:%s:%s:%s", plat, chat_id ? chat_id : "", thread);
        else
            snprintf(session_key, sizeof session_key, "cron:%s:%s", plat, chat_id ? chat_id : "");
        {
            char msg_ref[256];
            snprintf(msg_ref, sizeof msg_ref, "job:%s", job_id ? job_id : "");
            obligation_id(session_key, msg_ref, content ? content : "", oid);
        }
        if (!record_raises) {
            /* record_obligation + mark_failed landed; arm the in-process drain if the adapter allows */
            scheduled = (has_loop && has_schedule) ? 1 : 0;
            queued = 1;
            if (media_count > 0)
                snprintf(note, sizeof note,
                         "queued text for %s for redelivery once the live adapter reconnects "
                         "(%d attachment(s) not queued)", w, media_count);
            else
                snprintf(note, sizeof note,
                         "queued text for %s for redelivery once the live adapter reconnects", w);
        }
    }
    json_set(out, "queued", json_bool(queued ? true : false));
    json_set(out, "scheduled", json_bool(scheduled ? true : false));
    json_set(out, "session_key", (queued && session_key[0]) ? json_string(session_key) : json_null());
    json_set(out, "obligation_id", (queued && oid[0]) ? json_string(oid) : json_null());
    json_set(out, "note", note[0] ? json_string(note) : json_null());
    json_set(out, "mark_error", queued ? (live_error ? json_string(live_error) : json_null()) : json_null());
    json_set(out, "adapter_profile", (queued && adapter_profile && adapter_profile[0])
                                        ? json_string(adapter_profile) : json_null());
    return out;
}
