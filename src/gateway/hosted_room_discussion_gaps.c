#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/hosted_room_discussion.py: discussion + event + task cluster ── */

/* PoP: append_kwargs @ gateway/hosted_room_discussion.py:append_kwargs */
char *append_kwargs(const char *room_id) {
    char *kwargs = malloc(256);
    snprintf(kwargs, 256, "{\"room_id\":\"%s\"}", room_id ? room_id : "");
    return kwargs;
}

/* PoP: zero_based_int @ gateway/hosted_room_discussion.py:_zero_based_int */
int zero_based_int(int value, int maximum) {
    return (value >= 0 && value <= maximum) ? value : 0;
}

char *_exact_fields(const char *value) {
    return value ? strdup(value) : strdup("{}");
}

char *validate_user_payload(const char *payload) {
    if (!payload) return strdup("{}");
    return strdup(payload);
}

char *validate_roster(const char *value) {
    if (!value) return strdup("[]");
    return strdup(value);
}

char *validate_room(const char *value) {
    if (!value) return strdup("{}");
    return strdup(value);
}

/* PoP: is_pass_text @ gateway/hosted_room_discussion.py:is_pass_text */
int is_pass_text(const char *value) {
    if (!value) return 1;
    /* Check if text is empty or "(pass)" */
    while (*value == ' ') value++;
    if (!*value) return 1;
    if (strncasecmp(value, "(pass)", 6) == 0) return 1;
    if (strncasecmp(value, "pass", 4) == 0) return 1;
    return 0;
}

char *validate_event(const char *event) {
    return event ? strdup(event) : strdup("{}");
}

void validate_turn_coordinates(const char *payload) {
    (void)payload;
}

void validate_member_message(const char *payload) {
    (void)payload;
}

void validate_terminal_event(const char *kind, const char *payload) {
    (void)kind; (void)payload;
}

char *validated_events(const char *events) {
    return events ? strdup(events) : strdup("[]");
}

char *discussion_user_events(const char *events) {
    return events ? strdup(events) : strdup("[]");
}

char *message_events(const char *events) {
    return events ? strdup(events) : strdup("[]");
}

char *derive_member_watermarks(const char *room_json);

/* PoP: _derive_member_watermarks @ gateway/hosted_room_discussion.py:_derive_member_watermarks */
char *_derive_member_watermarks(const char *room_json) {
    return derive_member_watermarks(room_json);
}


char *_rotate(const char *value) {
    return value ? strdup(value) : strdup("");
}

/* PoP: truncate_utf8_text @ gateway/hosted_room_discussion.py:_truncate_utf8_text */
char *truncate_utf8_text(const char *text, int max_bytes) {
    if (!text) return strdup("");
    size_t len = strlen(text);
    if (len <= (size_t)max_bytes) return strdup(text);
    char *truncated = malloc(max_bytes + 1);
    memcpy(truncated, text, max_bytes);
    truncated[max_bytes] = '\0';
    return truncated;
}

char *_turn_id(const char *room_id, const char *thread_id, int round_index) {
    char *id = malloc(256);
    snprintf(id, 256, "%s:%s:t%d", room_id ? room_id : "", thread_id ? thread_id : "", round_index);
    return id;
}

char *_task_id(const char *room_id, const char *thread_id, int round_index, int member_index) {
    char *id = malloc(256);
    snprintf(id, 256, "%s:%s:t%d:m%d", room_id ? room_id : "", thread_id ? thread_id : "", round_index, member_index);
    return id;
}

char *terminal_text(const char *text) {
    return text ? strdup(text) : strdup("");
}

/* ── rest_revive4 revival: discussion policy ports ─────────────────────────
 * Faithful ports of the nine listed PoP entries. Fixed stub signatures take
 * their Python arguments from the serialized JSON work object passed as the
 * char* parameter (extra args for validate_member_target ride g_hrd_ctx).
 * Heavy collaborators (validate_room, _validated_events, the router/lead
 * routing/driver/context seams) are injected through g_hrd_io; the small
 * pure helpers (_identifier, _exact_fields, _member_digest, _rotate,
 * _pending_discussion, _thread_messages, _effective_watermarks,
 * _derive_member_watermarks, _scheduled_job, _first_round_responders,
 * _requires_direct_reply, _event_attachments, _terminal_text,
 * _truncate_utf8_text, is_pass_text, _member_actor, _turn_coordinates,
 * _reconstructed_lead_routing and the four terminal effect builders) are
 * ported directly and run for real on BOTH sides of the oracle.
 *
 * Return contract: char* = malloc'd JSON serialization of the Python return
 * ("null" for None); NULL only with a raise spec parked in
 * g_hrd_pending_raise. No channel installed -> fail closed.
 *
 * Models (documented projections, identical on both sides):
 * - DiscussionMember/Room/_ValidatedEvent/TaskPlan/Decision/EventPlan/
 *   PublicationPlan dataclasses are JSON objects with the dataclass fields;
 *   members always carry display_name (default "") and target (default null).
 * - tuple-keyed watermark dicts {(thread, member): seq} are arrays of
 *   [[thread, member], seq] pairs in insertion order.
 * - HostedTaskIdentity is {room_id, task_id, thread_id, turn_id}; fixture
 *   identities are pre-stripped (the dataclass __post_init__ strip is
 *   applied by the port when MINTING identities).
 * - router.ROUTE_PENDING is the sentinel object {"__sentinel__":
 *   "ROUTE_PENDING"}; the oracle returns the live singleton for it.
 * - JSON integral numbers model Python ints, fractional ones floats
 *   (a literal 5.0 reads back as int 5 — the libjson identity exception).
 *
 * Seam names:
 *   validate_room {room_value, local_profiles} -> room
 *   validated_events {events, room} -> [event]
 *   validate_context {purpose, notes, routing, lead_member_id} -> context
 *   router.continue_count {validated, discussion_event_id} -> int
 *   router.continued_through {validated} -> {discussion_event_id: seq}
 *   router.routed_responders {room, discussion, validated, context}
 *       -> [member] | null | ROUTE_PENDING sentinel
 *   router.resume_round {validated, discussion_event_id, member_messages} -> int
 *   lead_round {room, discussion, validated, context}
 *       -> null | {task, routing_task_id, responders, lead}
 *   bounded_task_delta {thread_messages, watermark, member} -> [delta, attachments]
 *   build_prompt {room, member, messages, watermark, seen_through_seq,
 *       context, addressed_by_job, reply_required, assigned_by, routed} -> str
 *   scheduled_post_kind {actor} -> null | "notice" | other label
 *   validate_task_input {value} -> dict
 *   validate_bound_task_manifest {value} -> [attachment]
 *   lead_routing.is_routing_task {task} -> bool
 *   lead_routing.assignment_effect {result, task, room, status} -> [EventPlan]
 *   all_failure_reasons {} -> [reason] (frozenset projected to array)
 *   import_classify_agent_error / classify_agent_error {error_text} -> str
 */

typedef struct hrd_io {
    json_t *(*call)(void *io, const char *name, json_t *args);
} hrd_io_t;

static hrd_io_t *g_hrd_io;
static json_t *g_hrd_ctx;            /* borrowed extra call-context */
static json_t *g_hrd_pending_raise;  /* owned parked raise spec */

static const char *hrd_raise_kind(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return NULL;
    json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : NULL;
}

static int hrd_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0 ||
                    strcmp(kind, "KeyboardInterrupt") == 0 ||
                    strcmp(kind, "GeneratorExit") == 0 ||
                    strcmp(kind, "CancelledError") == 0);
}

static json_t *hrd_raise_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

static void hrd_park(json_t *spec) {
    json_free(g_hrd_pending_raise);
    g_hrd_pending_raise = spec;
}

/* args OWNED by this wrapper, freed after the call; dispatcher borrows for
 * the trace copy. */
static json_t *hrd_seam(const char *name, json_t *args) {
    if (!g_hrd_io || !g_hrd_io->call) {
        json_free(args);
        char msg[192];
        snprintf(msg, sizeof msg, "no seam channel installed for %s", name);
        return hrd_raise_spec("RuntimeError", msg);
    }
    json_t *r = g_hrd_io->call(g_hrd_io, name, args);
    json_free(args);
    return r;
}

static int hrd_truth(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static char *hrd_ret(json_t *owned) {
    if (!owned) return strdup("null");
    char *s = json_serialize(owned);
    json_free(owned);
    return s ? s : strdup("null");
}

/* Deep Python == over JSON values (bool == integral number, 5 == 5.0). */
static int hrd_json_eq(const json_t *a, const json_t *b) {
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    int an = a->type == JSON_NUMBER || a->type == JSON_BOOL;
    int bn = b->type == JSON_NUMBER || b->type == JSON_BOOL;
    if (an && bn) {
        double x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type != b->type) return 0;
    if (a->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY) {
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++)
            if (!hrd_json_eq(a->c.items[i], b->c.items[i])) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++) {
            json_t *v = json_obj_get(b, a->c.keys[i]);
            if (!v || !hrd_json_eq(a->c.items[i], v)) return 0;
        }
        return 1;
    }
    return 0;
}

/* str() of a JSON scalar. */
static char *hrd_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && fabs(d) < 1e18)
            snprintf(buf, sizeof buf, "%.0f", d);
        else
            snprintf(buf, sizeof buf, "%.17g", d);
        return strdup(buf);
    }
    return strdup("");
}

/* int() coercion (number truncates toward zero; bool is an int). */
static int hrd_py_int(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) {
        hrd_park(hrd_raise_spec("TypeError", "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'"));
        return -1;
    }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 0; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 0; }
    if (v->type != JSON_STRING) {
        hrd_park(hrd_raise_spec("TypeError", "int() argument must be a string, a bytes-like object or a real number"));
        return -1;
    }
    const char *p = v->str_val;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
    char *end = NULL;
    long long n = strtoll(p, &end, 10);
    if (end == p) {
        hrd_park(hrd_raise_spec("ValueError", "invalid literal for int() with base 10"));
        return -1;
    }
    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '\f' || *end == '\v') end++;
    if (*end != '\0') {
        hrd_park(hrd_raise_spec("ValueError", "invalid literal for int() with base 10"));
        return -1;
    }
    *out = n;
    return 0;
}

/* Python str.strip() / re \s White_Space set (incl. 0x1C-0x1F). */
static int hrd_ws_cp(uint32_t cp) {
    return (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F) ||
           cp == 0x20 || cp == 0x85 || cp == 0xA0 || cp == 0x1680 ||
           (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029 ||
           cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

static size_t hrd_utf8_decode(const char *s, size_t len, size_t i, uint32_t *cp) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) { *cp = c; return 1; }
    if ((c & 0xE0) == 0xC0 && i + 1 < len) {
        *cp = ((uint32_t)(c & 0x1F) << 6) | ((unsigned char)s[i+1] & 0x3F); return 2;
    }
    if ((c & 0xF0) == 0xE0 && i + 2 < len) {
        *cp = ((uint32_t)(c & 0x0F) << 12) | (((unsigned char)s[i+1] & 0x3F) << 6) |
              ((unsigned char)s[i+2] & 0x3F); return 3;
    }
    if ((c & 0xF8) == 0xF0 && i + 3 < len) {
        *cp = ((uint32_t)(c & 0x07) << 18) | (((unsigned char)s[i+1] & 0x3F) << 12) |
              (((unsigned char)s[i+2] & 0x3F) << 6) | ((unsigned char)s[i+3] & 0x3F); return 4;
    }
    *cp = c; return 1;
}

static char *hrd_strip(const char *s) {
    if (!s) return strdup("");
    size_t len = strlen(s), a = 0, b = len;
    while (a < b) {
        uint32_t cp; size_t n = hrd_utf8_decode(s, len, a, &cp);
        if (!hrd_ws_cp(cp)) break;
        a += n;
    }
    while (b > a) {
        size_t i = b - 1;
        while (i > a && ((unsigned char)s[i] & 0xC0) == 0x80) i--;
        uint32_t cp; size_t n = hrd_utf8_decode(s, len, i, &cp);
        if (!hrd_ws_cp(cp) || i + n != b) break;
        b = i;
    }
    char *out = malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}

/* ── string builder ── */
typedef struct { char *b; size_t len, cap; } hrd_sb;
static void hrd_sb_init(hrd_sb *s) { s->cap = 128; s->len = 0; s->b = malloc(s->cap); if (s->b) s->b[0] = '\0'; }
static void hrd_sb_need(hrd_sb *s, size_t extra) {
    if (s->len + extra + 1 > s->cap) {
        while (s->len + extra + 1 > s->cap) s->cap *= 2;
        char *nb = realloc(s->b, s->cap);
        if (nb) s->b = nb;
    }
}
static void hrd_sb_puts(hrd_sb *s, const char *t) {
    size_t n = strlen(t);
    hrd_sb_need(s, n);
    if (s->b) { memcpy(s->b + s->len, t, n); s->len += n; s->b[s->len] = '\0'; }
}
static void hrd_sb_putc(hrd_sb *s, char c) {
    hrd_sb_need(s, 1);
    if (s->b) { s->b[s->len++] = c; s->b[s->len] = '\0'; }
}

/* ── SHA-256 (self-contained; keeps the gate libjson-only) ── */
static uint32_t hrd_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static char *hrd_sha256_hex(const unsigned char *data, size_t len) {
    static const uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t H[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    size_t padded = ((len + 8) / 64 + 1) * 64;
    unsigned char *buf = calloc(padded, 1);
    if (!buf) return NULL;
    if (len) memcpy(buf, data, len);
    buf[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) buf[padded - 1 - i] = (unsigned char)(bits >> (8 * i));
    for (size_t off = 0; off < padded; off += 64) {
        uint32_t W[64];
        for (int i = 0; i < 16; i++)
            W[i] = ((uint32_t)buf[off + 4*i] << 24) | ((uint32_t)buf[off + 4*i + 1] << 16) |
                   ((uint32_t)buf[off + 4*i + 2] << 8) | (uint32_t)buf[off + 4*i + 3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = hrd_rotr(W[i-15],7) ^ hrd_rotr(W[i-15],18) ^ (W[i-15] >> 3);
            uint32_t s1 = hrd_rotr(W[i-2],17) ^ hrd_rotr(W[i-2],19) ^ (W[i-2] >> 10);
            W[i] = W[i-16] + s0 + W[i-7] + s1;
        }
        uint32_t a=H[0],b=H[1],c=H[2],d=H[3],e=H[4],f=H[5],g=H[6],h=H[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = hrd_rotr(e,6) ^ hrd_rotr(e,11) ^ hrd_rotr(e,25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t t1 = h + S1 + ch + K[i] + W[i];
            uint32_t S0 = hrd_rotr(a,2) ^ hrd_rotr(a,13) ^ hrd_rotr(a,22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        H[0]+=a; H[1]+=b; H[2]+=c; H[3]+=d; H[4]+=e; H[5]+=f; H[6]+=g; H[7]+=h;
    }
    free(buf);
    char *hex = malloc(65);
    if (!hex) return NULL;
    for (int i = 0; i < 8; i++) snprintf(hex + 8*i, 9, "%08x", (unsigned)H[i]);
    hex[64] = '\0';
    return hex;
}

/* ── canonical JSON: json.dumps(sort_keys=True, separators=(",",":")) ── */
static void hrd_canon_str(hrd_sb *s, const char *t, int ensure_ascii) {
    hrd_sb_putc(s, '"');
    size_t len = strlen(t), i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)t[i];
        if (c == '"') { hrd_sb_puts(s, "\\\""); i++; continue; }
        if (c == '\\') { hrd_sb_puts(s, "\\\\"); i++; continue; }
        if (c == '\b') { hrd_sb_puts(s, "\\b"); i++; continue; }
        if (c == '\f') { hrd_sb_puts(s, "\\f"); i++; continue; }
        if (c == '\n') { hrd_sb_puts(s, "\\n"); i++; continue; }
        if (c == '\r') { hrd_sb_puts(s, "\\r"); i++; continue; }
        if (c == '\t') { hrd_sb_puts(s, "\\t"); i++; continue; }
        if (c < 0x20) {
            char esc[8];
            snprintf(esc, sizeof esc, "\\u%04x", (unsigned)c);
            hrd_sb_puts(s, esc); i++; continue;
        }
        if (c < 0x80 || !ensure_ascii) { hrd_sb_putc(s, (char)c); i++; continue; }
        uint32_t cp; size_t n = hrd_utf8_decode(t, len, i, &cp);
        char esc[16];
        if (cp >= 0x10000) {
            uint32_t v = cp - 0x10000;
            snprintf(esc, sizeof esc, "\\u%04x\\u%04x",
                     (unsigned)(0xD800 + (v >> 10)), (unsigned)(0xDC00 + (v & 0x3FF)));
        } else {
            snprintf(esc, sizeof esc, "\\u%04x", (unsigned)cp);
        }
        hrd_sb_puts(s, esc);
        i += n;
    }
    hrd_sb_putc(s, '"');
}

static int hrd_key_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void hrd_canon_node(hrd_sb *s, const json_t *n, int ensure_ascii) {
    if (!n || n->type == JSON_NULL) { hrd_sb_puts(s, "null"); return; }
    if (n->type == JSON_BOOL) { hrd_sb_puts(s, n->bool_val ? "true" : "false"); return; }
    if (n->type == JSON_NUMBER) {
        char tmp[64];
        double d = n->num_val;
        if (d == (double)(long long)d && fabs(d) < 1e18) {
            snprintf(tmp, sizeof tmp, "%.0f", d);
        } else {
            /* shortest round-trip decimal (Python float repr approximation) */
            int prec;
            for (prec = 1; prec <= 17; prec++) {
                snprintf(tmp, sizeof tmp, "%.*g", prec, d);
                if (strtod(tmp, NULL) == d) break;
            }
        }
        hrd_sb_puts(s, tmp);
        return;
    }
    if (n->type == JSON_STRING) { hrd_canon_str(s, n->str_val ? n->str_val : "", ensure_ascii); return; }
    if (n->type == JSON_ARRAY) {
        hrd_sb_putc(s, '[');
        for (size_t i = 0; i < n->c.count; i++) {
            if (i) hrd_sb_putc(s, ',');
            hrd_canon_node(s, n->c.items[i], ensure_ascii);
        }
        hrd_sb_putc(s, ']');
        return;
    }
    /* object: sorted keys (UTF-8 byte order == codepoint order) */
    size_t cnt = n->c.count;
    const char **keys = malloc((cnt ? cnt : 1) * sizeof(char *));
    for (size_t i = 0; i < cnt; i++) keys[i] = n->c.keys[i];
    if (cnt > 1) qsort(keys, cnt, sizeof(char *), hrd_key_cmp);
    hrd_sb_putc(s, '{');
    for (size_t i = 0; i < cnt; i++) {
        if (i) hrd_sb_putc(s, ',');
        hrd_canon_str(s, keys[i], ensure_ascii);
        hrd_sb_putc(s, ':');
        hrd_canon_node(s, json_obj_get(n, keys[i]), ensure_ascii);
    }
    hrd_sb_putc(s, '}');
    free(keys);
}

static char *hrd_compact_json(const json_t *n, int ensure_ascii) {
    hrd_sb s; hrd_sb_init(&s);
    hrd_canon_node(&s, n, ensure_ascii);
    return s.b ? s.b : strdup("null");
}

/* ── common.identifier / _task_identifier ── */
static int hrd_ident_shape(const char *s) {
    /* ^[A-Za-z0-9][A-Za-z0-9._:-]*$ */
    size_t n = strlen(s);
    if (!n) return 0;
    unsigned char c0 = (unsigned char)s[0];
    if (!((c0 >= 'A' && c0 <= 'Z') || (c0 >= 'a' && c0 <= 'z') || (c0 >= '0' && c0 <= '9')))
        return 0;
    for (size_t i = 1; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == ':' || c == '-'))
            return 0;
    }
    return 1;
}

/* identifier(value, label=..., error=DiscussionValidationError, max_chars=128).
 * Returns malloc'd stripped string or NULL with raise parked. */
static char *hrd_identifier(const json_t *value, const char *label, const char *errcls) {
    if (!value || value->type != JSON_STRING) {
        char msg[256];
        snprintf(msg, sizeof msg, "%s must be a string", label);
        hrd_park(hrd_raise_spec(errcls, msg));
        return NULL;
    }
    char *v = hrd_strip(value->str_val);
    if (!v[0] || strlen(v) > 128 || !hrd_ident_shape(v)) {
        char msg[256];
        snprintf(msg, sizeof msg, "invalid %s", label);
        hrd_park(hrd_raise_spec(errcls, msg));
        free(v);
        return NULL;
    }
    return v;
}

/* common.exact_fields: required (+optional) keys exactly; offenders sorted. */
static int hrd_str_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static void hrd_join_sorted(hrd_sb *s, const char **items, size_t n) {
    const char **tmp = malloc((n ? n : 1) * sizeof(char *));
    for (size_t i = 0; i < n; i++) tmp[i] = items[i];
    if (n > 1) qsort(tmp, n, sizeof(char *), hrd_str_cmp);
    for (size_t i = 0; i < n; i++) {
        if (i) hrd_sb_puts(s, ", ");
        hrd_sb_puts(s, tmp[i]);
    }
    free(tmp);
}
/* returns 0 ok (value is an object with exactly the fields), -1 parked. */
static int hrd_exact_fields(const json_t *value, const char *label,
                            const char *const *required, size_t nreq,
                            const char *const *optional, size_t nopt) {
    if (!value || value->type != JSON_OBJECT) {
        char msg[256];
        snprintf(msg, sizeof msg, "%s must be an object", label);
        hrd_park(hrd_raise_spec("DiscussionValidationError", msg));
        return -1;
    }
    const char *missing[32]; size_t nm = 0;
    for (size_t i = 0; i < nreq; i++)
        if (!json_obj_get(value, required[i]) && nm < 32) missing[nm++] = required[i];
    if (nm) {
        hrd_sb s; hrd_sb_init(&s);
        hrd_sb_puts(&s, label); hrd_sb_puts(&s, " is missing fields: ");
        hrd_join_sorted(&s, missing, nm);
        hrd_park(hrd_raise_spec("DiscussionValidationError", s.b));
        free(s.b);
        return -1;
    }
    const char *unknown[64]; size_t nu = 0;
    for (size_t i = 0; i < value->c.count; i++) {
        const char *k = value->c.keys[i];
        int ok = 0;
        for (size_t j = 0; j < nreq; j++) if (strcmp(k, required[j]) == 0) ok = 1;
        for (size_t j = 0; j < nopt; j++) if (strcmp(k, optional[j]) == 0) ok = 1;
        if (!ok && nu < 64) unknown[nu++] = k;
    }
    if (nu) {
        hrd_sb s; hrd_sb_init(&s);
        hrd_sb_puts(&s, label); hrd_sb_puts(&s, " has unknown fields: ");
        hrd_join_sorted(&s, unknown, nu);
        hrd_park(hrd_raise_spec("DiscussionValidationError", s.b));
        free(s.b);
        return -1;
    }
    return 0;
}

/* common.bounded_int: reject bools/non-ints/out-of-range. JSON integral
 * numbers model ints; fractional numbers model floats (rejected). */
static int hrd_bounded_int(const json_t *value, const char *message,
                           long long low, int has_high, long long high,
                           long long *out) {
    int is_int = value && value->type == JSON_NUMBER &&
                 value->num_val == (double)(long long)value->num_val;
    if (!is_int || value->num_val < (double)low ||
        (has_high && value->num_val > (double)high)) {
        hrd_park(hrd_raise_spec("DiscussionValidationError", message));
        return -1;
    }
    *out = (long long)value->num_val;
    return 0;
}

/* ── member model ── */
static json_t *hrd_member_norm(const json_t *m) {
    json_t *o = json_object();
    json_set_hdr(o, "member_id", json_get_str(m, "member_id", ""));
    json_set_hdr(o, "profile", json_get_str(m, "profile", ""));
    json_set_hdr(o, "handle", json_get_str(m, "handle", ""));
    json_set_hdr(o, "display_name", json_get_str(m, "display_name", ""));
    json_t *t = json_obj_get(m, "target");
    json_set(o, "target", t ? json_copy(t) : json_null());
    return o;
}

static int hrd_member_eq(const json_t *a, const json_t *b) {
    json_t *na = hrd_member_norm(a), *nb = hrd_member_norm(b);
    int eq = hrd_json_eq(na, nb);
    json_free(na); json_free(nb);
    return eq;
}

/* _member_digest: sha256(member_id\0profile\0handle\0compact_json(target or
 * {kind:local,profile}, ensure_ascii=False))[:24] */
static char *hrd_member_digest(const json_t *member) {
    json_t *target = json_obj_get(member, "target");
    json_t *t;
    if (hrd_truth(target)) {
        t = json_copy(target);
    } else {
        t = json_object();
        json_set_hdr(t, "kind", "local");
        json_set_hdr(t, "profile", json_get_str(member, "profile", ""));
    }
    char *cj = hrd_compact_json(t, 0);
    json_free(t);
    const char *mid = json_get_str(member, "member_id", "");
    const char *prof = json_get_str(member, "profile", "");
    const char *handle = json_get_str(member, "handle", "");
    size_t n = strlen(mid) + strlen(prof) + strlen(handle) + strlen(cj) + 4;
    char *buf = malloc(n);
    size_t k = 0;
    memcpy(buf + k, mid, strlen(mid)); k += strlen(mid); buf[k++] = '\0';
    memcpy(buf + k, prof, strlen(prof)); k += strlen(prof); buf[k++] = '\0';
    memcpy(buf + k, handle, strlen(handle)); k += strlen(handle); buf[k++] = '\0';
    memcpy(buf + k, cj, strlen(cj)); k += strlen(cj);
    char *hex = hrd_sha256_hex((unsigned char *)buf, k);
    free(buf); free(cj);
    if (!hex) return NULL;
    hex[24] = '\0';
    return hex;
}

/* ── _MENTION_RE: @([A-Za-z0-9][A-Za-z0-9._:-]*) IGNORECASE ── */
static int hrd_mention_char(unsigned char c, int first) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) return 1;
    if (first) return 0;
    return c == '.' || c == '_' || c == ':' || c == '-';
}

/* ASCII casefold (fixtures use ASCII handles; Unicode casefold is the
 * documented exception). malloc'd lowercased copy. */
static char *hrd_casefold(const char *s) {
    char *o = strdup(s);
    for (char *p = o; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    return o;
}

/* resolve_mentions(texts, members, default_all) -> array of normalized
 * members in roster order. */
static json_t *hrd_resolve_mentions(const json_t *texts, const json_t *members, int default_all) {
    size_t nm = members && members->type == JSON_ARRAY ? members->c.count : 0;
    /* by_handle: last member per handle wins */
    char **handles = malloc((nm ? nm : 1) * sizeof(char *));
    size_t *owner = malloc((nm ? nm : 1) * sizeof(size_t));
    for (size_t i = 0; i < nm; i++) {
        handles[i] = hrd_casefold(json_get_str(members->c.items[i], "handle", ""));
        owner[i] = i;
    }
    /* mentioned set as flags over roster indices (dedup by handle) */
    int *mentioned = calloc(nm ? nm : 1, sizeof(int));
    int everyone = 0, any_mentioned = 0;
    size_t nt = texts && texts->type == JSON_ARRAY ? texts->c.count : 0;
    for (size_t ti = 0; ti < nt; ti++) {
        char *text = hrd_py_str(texts->c.items[ti]); /* str(text or "") */
        if (!hrd_truth(texts->c.items[ti])) { free(text); text = strdup(""); }
        size_t len = strlen(text);
        for (size_t i = 0; i < len; i++) {
            if (text[i] != '@') continue;
            size_t j = i + 1;
            if (j >= len || !hrd_mention_char((unsigned char)text[j], 1)) continue;
            j++;
            while (j < len && hrd_mention_char((unsigned char)text[j], 0)) j++;
            char *handle = malloc(j - i);
            memcpy(handle, text + i + 1, j - i - 1);
            handle[j - i - 1] = '\0';
            char *fold = hrd_casefold(handle);
            free(handle);
            if (strcmp(fold, "all") == 0 || strcmp(fold, "everyone") == 0) {
                everyone = 1;
            } else {
                for (size_t mi = 0; mi < nm; mi++) {
                    if (strcmp(handles[mi], fold) == 0) {
                        mentioned[mi] = 1;
                        any_mentioned = 1;
                    }
                }
            }
            free(fold);
            i = j - 1;
        }
        free(text);
    }
    json_t *out = json_array();
    int all = everyone || (default_all && !any_mentioned);
    for (size_t mi = 0; mi < nm; mi++) {
        if (all || mentioned[mi])
            json_append(out, hrd_member_norm(members->c.items[mi]));
        free(handles[mi]);
    }
    free(handles); free(owner); free(mentioned);
    return out;
}

/* _unaddressed_member_mentions(messages, room) */
static json_t *hrd_unaddressed(const json_t *messages, const json_t *room) {
    json_t *members = json_obj_get(room, "members");
    size_t nm = members && members->type == JSON_ARRAY ? members->c.count : 0;
    /* cited_at / last_post_at keyed by member_id string */
    typedef struct { char *id; long long seq; } hrd_mark;
    hrd_mark *cited = calloc(nm ? nm : 1, sizeof(hrd_mark));
    hrd_mark *posted = calloc(nm ? nm : 1, sizeof(hrd_mark));
    size_t nc = 0, np = 0;
    size_t nmsg = messages && messages->type == JSON_ARRAY ? messages->c.count : 0;
    for (size_t i = 0; i < nmsg; i++) {
        json_t *ev = messages->c.items[i];
        if (strcmp(json_get_str(ev, "kind", ""), "message.member") != 0) continue;
        json_t *payload = json_obj_get(ev, "payload");
        char *speaker = hrd_py_str(json_obj_get(payload, "member_id"));
        long long seq = (long long)json_get_num(ev, "seq", 0);
        /* last_post_at[speaker] = seq */
        int found = 0;
        for (size_t k = 0; k < np; k++)
            if (strcmp(posted[k].id, speaker) == 0) { posted[k].seq = seq; found = 1; }
        if (!found) { posted[np].id = strdup(speaker); posted[np].seq = seq; np++; }
        json_t *texts = json_array();
        char *txt = hrd_py_str(json_obj_get(payload, "text"));
        json_append(texts, json_string(txt));
        free(txt);
        json_t *resolved = hrd_resolve_mentions(texts, members, 0);
        json_free(texts);
        for (size_t k = 0; k < resolved->c.count; k++) {
            const char *mid = json_get_str(resolved->c.items[k], "member_id", "");
            if (strcmp(mid, speaker) == 0) continue;
            int f2 = 0;
            for (size_t q = 0; q < nc; q++)
                if (strcmp(cited[q].id, mid) == 0) { cited[q].seq = seq; f2 = 1; }
            if (!f2) { cited[nc].id = strdup(mid); cited[nc].seq = seq; nc++; }
        }
        json_free(resolved);
        free(speaker);
    }
    json_t *out = json_array();
    for (size_t mi = 0; mi < nm; mi++) {
        json_t *m = members->c.items[mi];
        const char *mid = json_get_str(m, "member_id", "");
        long long cat = -1;
        for (size_t q = 0; q < nc; q++)
            if (strcmp(cited[q].id, mid) == 0) cat = cited[q].seq;
        if (cat < 0) continue;
        long long last = 0;
        for (size_t q = 0; q < np; q++)
            if (strcmp(posted[q].id, mid) == 0) last = posted[q].seq;
        if (last <= cat) json_append(out, hrd_member_norm(m));
    }
    for (size_t q = 0; q < nc; q++) free(cited[q].id);
    for (size_t q = 0; q < np; q++) free(posted[q].id);
    free(cited); free(posted);
    return out;
}

/* _member_by_id(room, member_id) */
static json_t *hrd_member_by_id(const json_t *room, const json_t *member_id) {
    char *norm = hrd_identifier(member_id, "member_id", "DiscussionValidationError");
    if (!norm) return NULL;
    json_t *members = json_obj_get(room, "members");
    size_t nm = members && members->type == JSON_ARRAY ? members->c.count : 0;
    for (size_t i = 0; i < nm; i++) {
        if (strcmp(json_get_str(members->c.items[i], "member_id", ""), norm) == 0) {
            json_t *out = hrd_member_norm(members->c.items[i]);
            free(norm);
            return out;
        }
    }
    char msg[256];
    snprintf(msg, sizeof msg, "unknown Discussion member '%s'", norm);
    hrd_park(hrd_raise_spec("DiscussionValidationError", msg));
    free(norm);
    return NULL;
}

/* _validate_member_target(value, profile=..., known_profiles=..., index=...) */
static json_t *hrd_validate_member_target(const json_t *value, const char *profile,
                                          const json_t *known_profiles, long long index) {
    static const char *const local_fields[] = { "kind", "profile" };
    static const char *const peer_fields[] = {
        "kind", "peer_id", "installation_id", "profile", "capability_digest"
    };
    int known = 0;
    if (known_profiles && known_profiles->type == JSON_ARRAY)
        for (size_t i = 0; i < known_profiles->c.count; i++) {
            json_t *p = known_profiles->c.items[i];
            if (p && p->type == JSON_STRING && strcmp(p->str_val, profile) == 0) known = 1;
        }
    char msg[320];
    if (!value || value->type == JSON_NULL) {
        if (!known) {
            snprintf(msg, sizeof msg, "member %lld profile '%s' is not local to this gateway", index, profile);
            hrd_park(hrd_raise_spec("DiscussionValidationError", msg));
            return NULL;
        }
        json_t *o = json_object();
        json_set_hdr(o, "kind", "local");
        json_set_hdr(o, "profile", profile);
        return o;
    }
    if (value->type != JSON_OBJECT) {
        snprintf(msg, sizeof msg, "member %lld target must be an object", index);
        hrd_park(hrd_raise_spec("DiscussionValidationError", msg));
        return NULL;
    }
    const char *kind = json_get_str(value, "kind", "");
    int is_local = strcmp(kind, "local") == 0;
    int is_peer = strcmp(kind, "peer") == 0;
    if (!is_local && !is_peer) {
        snprintf(msg, sizeof msg, "member %lld target kind must be local or peer", index);
        hrd_park(hrd_raise_spec("DiscussionValidationError", msg));
        return NULL;
    }
    char label[160];
    snprintf(label, sizeof label, "member %lld %s target", index, kind);
    if (hrd_exact_fields(value, label,
                         is_local ? local_fields : peer_fields,
                         is_local ? 2 : 5, NULL, 0) < 0)
        return NULL;
    char plabel[160];
    snprintf(plabel, sizeof plabel, "member %lld target profile", index);
    char *target_profile = hrd_identifier(json_obj_get(value, "profile"), plabel,
                                          "DiscussionValidationError");
    if (!target_profile) return NULL;
    if (is_local) {
        if (strcmp(target_profile, profile) != 0) {
            snprintf(msg, sizeof msg, "member %lld local target profile does not match member profile", index);
            hrd_park(hrd_raise_spec("DiscussionValidationError", msg));
            free(target_profile);
            return NULL;
        }
        if (!known) {
            snprintf(msg, sizeof msg, "member %lld profile '%s' is not local to this gateway", index, profile);
            hrd_park(hrd_raise_spec("DiscussionValidationError", msg));
            free(target_profile);
            return NULL;
        }
        json_t *o = json_object();
        json_set_hdr(o, "kind", "local");
        json_set_hdr(o, "profile", profile);
        free(target_profile);
        return o;
    }
    if (strcmp(target_profile, profile) != 0) {
        snprintf(msg, sizeof msg, "member %lld peer target profile does not match member profile", index);
        hrd_park(hrd_raise_spec("DiscussionValidationError", msg));
        free(target_profile);
        return NULL;
    }
    json_t *digest = json_obj_get(value, "capability_digest");
    int ok = digest && digest->type == JSON_STRING && strlen(digest->str_val) == 64;
    if (ok)
        for (int i = 0; i < 64; i++) {
            char c = digest->str_val[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) { ok = 0; break; }
        }
    if (!ok) {
        snprintf(msg, sizeof msg, "member %lld capability_digest must be a sha256 digest", index);
        hrd_park(hrd_raise_spec("DiscussionValidationError", msg));
        free(target_profile);
        return NULL;
    }
    char lbl2[160];
    snprintf(lbl2, sizeof lbl2, "member %lld peer_id", index);
    char *peer_id = hrd_identifier(json_obj_get(value, "peer_id"), lbl2, "DiscussionValidationError");
    if (!peer_id) { free(target_profile); return NULL; }
    snprintf(lbl2, sizeof lbl2, "member %lld installation_id", index);
    char *install = hrd_identifier(json_obj_get(value, "installation_id"), lbl2, "DiscussionValidationError");
    if (!install) { free(target_profile); free(peer_id); return NULL; }
    json_t *o = json_object();
    json_set_hdr(o, "kind", "peer");
    json_set_hdr(o, "peer_id", peer_id);
    json_set_hdr(o, "installation_id", install);
    json_set_hdr(o, "profile", target_profile);
    json_set_hdr(o, "capability_digest", digest->str_val);
    free(peer_id); free(install); free(target_profile);
    return o;
}

/* ── watermarks: {(thread, member): seq} modeled as [[t, m], seq] arrays ── */
static long long hrd_wm_get(const json_t *wm, const char *t, const char *m) {
    if (!wm || wm->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < wm->c.count; i++) {
        json_t *pair = wm->c.items[i];
        if (!pair || pair->type != JSON_ARRAY || pair->c.count != 2) continue;
        json_t *key = pair->c.items[0];
        if (!key || key->type != JSON_ARRAY || key->c.count != 2) continue;
        const char *kt = key->c.items[0] && key->c.items[0]->type == JSON_STRING ? key->c.items[0]->str_val : "";
        const char *km = key->c.items[1] && key->c.items[1]->type == JSON_STRING ? key->c.items[1]->str_val : "";
        if (strcmp(kt, t) == 0 && strcmp(km, m) == 0)
            return (long long)(pair->c.items[1] && pair->c.items[1]->type == JSON_NUMBER ? pair->c.items[1]->num_val : 0);
    }
    return 0;
}

static void hrd_wm_set(json_t *wm, const char *t, const char *m, long long v) {
    for (size_t i = 0; i < wm->c.count; i++) {
        json_t *pair = wm->c.items[i];
        if (!pair || pair->type != JSON_ARRAY || pair->c.count != 2) continue;
        json_t *key = pair->c.items[0];
        if (!key || key->type != JSON_ARRAY || key->c.count != 2) continue;
        const char *kt = key->c.items[0] && key->c.items[0]->type == JSON_STRING ? key->c.items[0]->str_val : "";
        const char *km = key->c.items[1] && key->c.items[1]->type == JSON_STRING ? key->c.items[1]->str_val : "";
        if (strcmp(kt, t) == 0 && strcmp(km, m) == 0) {
            json_array_set(pair, 1, json_number((double)v));
            return;
        }
    }
    json_t *key = json_array();
    json_append(key, json_string(t));
    json_append(key, json_string(m));
    json_t *pair = json_array();
    json_append(pair, key);
    json_append(pair, json_number((double)v));
    json_append(wm, pair);
}

static int hrd_is_terminal_kind(const char *kind) {
    return strcmp(kind, "turn.settled") == 0 || strcmp(kind, "turn.failed") == 0 ||
           strcmp(kind, "turn.cancelled") == 0 || strcmp(kind, "turn.deferred") == 0;
}

/* KeyError spec with Python repr of a string key. */
static json_t *hrd_keyerror(const char *key) {
    char msg[256];
    snprintf(msg, sizeof msg, "'%s'", key);
    return hrd_raise_spec("KeyError", msg);
}

/* payload[key] with KeyError parking; returns borrowed node or NULL. */
static json_t *hrd_payload_req(json_t *payload, const char *key) {
    json_t *v = payload ? json_obj_get(payload, key) : NULL;
    if (!v) hrd_park(hrd_keyerror(key));
    return v;
}

/* _derive_member_watermarks(events) -> watermark array (insertion order). */
static json_t *hrd_derive_watermarks(const json_t *events) {
    size_t ne = events && events->type == JSON_ARRAY ? events->c.count : 0;
    /* messages_by_id: event_id -> event (message.member) */
    json_t *wm = json_array();
    /* terminal_by_task as parallel arrays */
    char **tids = malloc((ne ? ne : 1) * sizeof(char *));
    json_t **tevs = malloc((ne ? ne : 1) * sizeof(json_t *));
    size_t nt = 0;
    for (size_t i = 0; i < ne; i++) {
        json_t *ev = events->c.items[i];
        const char *kind = json_get_str(ev, "kind", "");
        if (!hrd_is_terminal_kind(kind)) continue;
        json_t *payload = json_obj_get(ev, "payload");
        json_t *tidv = hrd_payload_req(payload, "task_id");
        if (!tidv) goto fail;
        char *tid = hrd_py_str(tidv);
        json_t *prev = NULL;
        for (size_t k = 0; k < nt; k++)
            if (strcmp(tids[k], tid) == 0) { prev = tevs[k]; break; }
        if (prev) {
            const char *pk = json_get_str(prev, "kind", "");
            if (strcmp(pk, "turn.deferred") != 0) {
                char msg[320];
                snprintf(msg, sizeof msg, "task '%s' has more than one terminal room event", tid);
                hrd_park(hrd_raise_spec("DiscussionValidationError", msg));
                free(tid); goto fail;
            }
            if (strcmp(kind, "turn.deferred") == 0) {
                json_t *gv = hrd_payload_req(payload, "execution_generation");
                if (!gv) { free(tid); goto fail; }
                json_t *pgv = hrd_payload_req(json_obj_get(prev, "payload"), "execution_generation");
                if (!pgv) { free(tid); goto fail; }
                long long g, pg;
                if (hrd_py_int(gv, &g) < 0 || hrd_py_int(pgv, &pg) < 0) { free(tid); goto fail; }
                if (g <= pg) {
                    char msg[320];
                    snprintf(msg, sizeof msg, "task '%s' deferral generation did not advance", tid);
                    hrd_park(hrd_raise_spec("DiscussionValidationError", msg));
                    free(tid); goto fail;
                }
            }
        }
        /* terminal_by_task[tid] = ev */
        int found = 0;
        for (size_t k = 0; k < nt; k++)
            if (strcmp(tids[k], tid) == 0) { tevs[k] = ev; found = 1; break; }
        if (!found) { tids[nt] = tid; tevs[nt] = ev; nt++; tid = NULL; }
        free(tid);
        json_t *thv = hrd_payload_req(payload, "thread_id");
        if (!thv) goto fail;
        json_t *midv = hrd_payload_req(payload, "member_id");
        if (!midv) goto fail;
        char *th = hrd_py_str(thv), *mid = hrd_py_str(midv);
        json_t *sv = hrd_payload_req(payload, "seen_through_seq");
        if (!sv) { free(th); free(mid); goto fail; }
        long long watermark;
        if (hrd_py_int(sv, &watermark) < 0) { free(th); free(mid); goto fail; }
        if (strcmp(kind, "turn.settled") == 0) {
            json_t *passed = hrd_payload_req(payload, "passed");
            if (!passed) { free(th); free(mid); goto fail; }
            if (!hrd_truth(passed)) {
                json_t *mev = hrd_payload_req(payload, "message_event_id");
                if (!mev) { free(th); free(mid); goto fail; }
                char *meid = hrd_py_str(mev);
                json_t *message = NULL;
                for (size_t k = 0; k < ne; k++) {
                    json_t *cand = events->c.items[k];
                    if (strcmp(json_get_str(cand, "kind", ""), "message.member") == 0 &&
                        strcmp(json_get_str(cand, "event_id", ""), meid) == 0)
                        message = cand;
                }
                free(meid);
                int mismatch = !message;
                if (!mismatch) {
                    static const char *const fs[] = { "task_id", "member_id", "thread_id" };
                    json_t *mp = json_obj_get(message, "payload");
                    for (int f = 0; f < 3; f++) {
                        json_t *a = mp ? json_obj_get(mp, fs[f]) : NULL;
                        json_t *b = json_obj_get(payload, fs[f]);
                        if (!hrd_json_eq(a, b)) { mismatch = 1; break; }
                    }
                }
                if (mismatch) {
                    hrd_park(hrd_raise_spec("DiscussionValidationError",
                                            "turn.settled references no matching member message"));
                    free(th); free(mid); goto fail;
                }
                json_t *deid = hrd_payload_req(payload, "discussion_event_id");
                if (!deid) { free(th); free(mid); goto fail; }
                long long discussion_seq = -1;
                for (size_t k = 0; k < ne; k++) {
                    json_t *cand = events->c.items[k];
                    if (hrd_json_eq(json_obj_get(cand, "event_id"), deid)) {
                        discussion_seq = (long long)json_get_num(cand, "seq", 0);
                        break;
                    }
                }
                if (discussion_seq < 0) {
                    hrd_park(hrd_raise_spec("DiscussionValidationError",
                                            "turn.settled references no matching user discussion"));
                    free(th); free(mid); goto fail;
                }
                if (watermark >= discussion_seq) {
                    long long mseq = (long long)json_get_num(message, "seq", 0);
                    if (mseq > watermark) watermark = mseq;
                }
            }
        }
        long long old = hrd_wm_get(wm, th, mid);
        hrd_wm_set(wm, th, mid, watermark > old ? watermark : old);
        free(th); free(mid);
    }
    for (size_t k = 0; k < nt; k++) free(tids[k]);
    free(tids); free(tevs);
    return wm;
fail:
    for (size_t k = 0; k < nt; k++) free(tids[k]);
    free(tids); free(tevs);
    json_free(wm);
    return NULL;
}

/* _effective_watermarks(validated, initial_watermarks) */
static json_t *hrd_effective_watermarks(const json_t *validated, const json_t *initial) {
    json_t *wm = json_array();
    if (initial && initial->type == JSON_ARRAY) {
        for (size_t i = 0; i < initial->c.count; i++) {
            json_t *pair = initial->c.items[i];
            if (!pair || pair->type != JSON_ARRAY || pair->c.count != 2) continue;
            json_t *key = pair->c.items[0];
            if (!key || key->type != JSON_ARRAY || key->c.count != 2) continue;
            char *t = hrd_py_str(key->c.items[0]);
            char *m = hrd_py_str(key->c.items[1]);
            long long v;
            if (hrd_py_int(pair->c.items[1], &v) < 0) { free(t); free(m); json_free(wm); return NULL; }
            if (v >= 0) hrd_wm_set(wm, t, m, v);
            free(t); free(m);
        }
    }
    json_t *derived = hrd_derive_watermarks(validated);
    if (!derived) { json_free(wm); return NULL; }
    for (size_t i = 0; i < derived->c.count; i++) {
        json_t *pair = derived->c.items[i];
        json_t *key = pair->c.items[0];
        const char *t = key->c.items[0]->str_val;
        const char *m = key->c.items[1]->str_val;
        long long v = (long long)pair->c.items[1]->num_val;
        long long old = hrd_wm_get(wm, t, m);
        hrd_wm_set(wm, t, m, v > old ? v : old);
    }
    json_free(derived);
    return wm;
}

/* _rotate(members, round_index) */
static json_t *hrd_rotate(const json_t *members, long long round_index) {
    json_t *out = json_array();
    size_t n = members && members->type == JSON_ARRAY ? members->c.count : 0;
    if (!n) return out;
    long long shift = round_index % (long long)n;
    if (shift < 0) shift += (long long)n;
    for (size_t i = 0; i < n; i++) {
        size_t idx = (size_t)(((long long)i + shift) % (long long)n);
        json_append(out, json_copy(members->c.items[idx]));
    }
    return out;
}

/* router.continued_through seam result: {discussion_event_id: seq} */
static long long hrd_continued_at(const json_t *continued, const char *id) {
    json_t *v = continued ? json_obj_get(continued, id) : NULL;
    return v && v->type == JSON_NUMBER ? (long long)v->num_val : 0;
}

/* _pending_discussion(validated) — router.continued_through is a seam. */
static json_t *hrd_pending_discussion(const json_t *validated) {
    size_t ne = validated && validated->type == JSON_ARRAY ? validated->c.count : 0;
    long long stopped = 0;
    for (size_t i = 0; i < ne; i++) {
        json_t *ev = validated->c.items[i];
        if (strcmp(json_get_str(ev, "kind", ""), "room.stop_requested") == 0) {
            long long s = (long long)json_get_num(ev, "seq", 0);
            if (s > stopped) stopped = s;
        }
    }
    json_t *cargs = json_object();
    json_set(cargs, "validated", json_copy(validated));
    json_t *continued = hrd_seam("router.continued_through", cargs);
    if (hrd_raise_kind(continued)) { hrd_park(continued); return NULL; }
    /* completed_discussion_ids */
    char **completed = malloc((ne ? ne : 1) * sizeof(char *));
    size_t ncomp = 0;
    for (size_t i = 0; i < ne; i++) {
        json_t *ev = validated->c.items[i];
        if (strcmp(json_get_str(ev, "kind", ""), "room.activity") != 0) continue;
        json_t *payload = json_obj_get(ev, "payload");
        const char *status = json_get_str(payload, "status", "");
        if (strcmp(status, "settled") != 0 && strcmp(status, "bounded") != 0) continue;
        char *deid = hrd_py_str(json_obj_get(payload, "discussion_event_id"));
        long long seq = (long long)json_get_num(ev, "seq", 0);
        if (seq > hrd_continued_at(continued, deid)) {
            int dup = 0;
            for (size_t k = 0; k < ncomp; k++) if (strcmp(completed[k], deid) == 0) dup = 1;
            if (!dup) completed[ncomp++] = deid; else free(deid);
        } else free(deid);
    }
    json_free(continued);
    /* latest_by_thread: first-appearance key order, last event wins */
    char **threads = malloc((ne ? ne : 1) * sizeof(char *));
    json_t **latest = malloc((ne ? ne : 1) * sizeof(json_t *));
    size_t nl = 0;
    for (size_t i = 0; i < ne; i++) {
        json_t *ev = validated->c.items[i];
        if (strcmp(json_get_str(ev, "kind", ""), "message.user") != 0) continue;
        char *th = hrd_py_str(json_obj_get(json_obj_get(ev, "payload"), "thread_id"));
        int found = 0;
        for (size_t k = 0; k < nl; k++)
            if (strcmp(threads[k], th) == 0) { latest[k] = ev; found = 1; break; }
        if (!found) { threads[nl] = th; latest[nl] = ev; nl++; }
        else free(th);
    }
    /* stable sort by seq (insertion sort) */
    for (size_t i = 1; i < nl; i++) {
        char *tk = threads[i]; json_t *vk = latest[i];
        long long sk = (long long)json_get_num(vk, "seq", 0);
        size_t j = i;
        while (j > 0 && (long long)json_get_num(latest[j-1], "seq", 0) > sk) {
            threads[j] = threads[j-1]; latest[j] = latest[j-1]; j--;
        }
        threads[j] = tk; latest[j] = vk;
    }
    json_t *result = NULL;
    for (size_t i = 0; i < nl; i++) {
        long long seq = (long long)json_get_num(latest[i], "seq", 0);
        const char *eid = json_get_str(latest[i], "event_id", "");
        int comp = 0;
        for (size_t k = 0; k < ncomp; k++) if (strcmp(completed[k], eid) == 0) comp = 1;
        if (seq > stopped && !comp) { result = latest[i]; break; }
    }
    for (size_t k = 0; k < ncomp; k++) free(completed[k]);
    for (size_t k = 0; k < nl; k++) free(threads[k]);
    free(completed); free(threads); free(latest);
    return result ? json_copy(result) : json_null();
}

/* _thread_messages(validated, discussion) -> [thread, since, member] */
static json_t *hrd_thread_messages(const json_t *validated, const json_t *discussion) {
    size_t ne = validated && validated->type == JSON_ARRAY ? validated->c.count : 0;
    char *thread_id = hrd_py_str(json_obj_get(json_obj_get(discussion, "payload"), "thread_id"));
    long long dseq = (long long)json_get_num(discussion, "seq", 0);
    const char *deid = json_get_str(discussion, "event_id", "");
    /* committed member message ids */
    char **committed = malloc((ne ? ne : 1) * sizeof(char *));
    size_t ncm = 0;
    for (size_t i = 0; i < ne; i++) {
        json_t *ev = validated->c.items[i];
        if (strcmp(json_get_str(ev, "kind", ""), "turn.settled") != 0) continue;
        json_t *meid = json_obj_get(json_obj_get(ev, "payload"), "message_event_id");
        if (meid && meid->type != JSON_NULL) committed[ncm++] = hrd_py_str(meid);
    }
    json_t *thread = json_array(), *since = json_array(), *member = json_array();
    for (size_t i = 0; i < ne; i++) {
        json_t *ev = validated->c.items[i];
        json_t *payload = json_obj_get(ev, "payload");
        json_t *th = payload ? json_obj_get(payload, "thread_id") : NULL;
        json_t *thstr = json_string(thread_id);
        int same_thread = hrd_json_eq(th, thstr);
        json_free(thstr);
        if (!same_thread) continue;
        const char *kind = json_get_str(ev, "kind", "");
        int is_user = strcmp(kind, "message.user") == 0;
        int is_member = strcmp(kind, "message.member") == 0;
        int committed_ok = 0;
        if (is_member) {
            const char *eid = json_get_str(ev, "event_id", "");
            for (size_t k = 0; k < ncm; k++) if (strcmp(committed[k], eid) == 0) committed_ok = 1;
        }
        if (!is_user && !(is_member && committed_ok)) continue;
        json_append(thread, json_copy(ev));
        if ((long long)json_get_num(ev, "seq", 0) >= dseq)
            json_append(since, json_copy(ev));
        if (is_member) {
            json_t *pdeid = payload ? json_obj_get(payload, "discussion_event_id") : NULL;
            json_t *want = json_string(deid);
            if (hrd_json_eq(pdeid, want)) json_append(member, json_copy(ev));
            json_free(want);
        }
    }
    for (size_t k = 0; k < ncm; k++) free(committed[k]);
    free(committed); free(thread_id);
    json_t *out = json_array();
    json_append(out, thread);
    json_append(out, since);
    json_append(out, member);
    return out;
}

/* scheduled_post_kind(actor) seam; returns borrowed-kind string copy. */
static char *hrd_scheduled_post_kind(const json_t *actor) {
    json_t *args = json_object();
    json_set(args, "actor", json_copy(actor));
    json_t *r = hrd_seam("scheduled_post_kind", args);
    if (hrd_raise_kind(r)) { hrd_park(r); return NULL; }
    char *out = NULL;
    if (r && r->type == JSON_STRING) out = strdup(r->str_val);
    json_free(r);
    return out; /* NULL models None */
}

/* _scheduled_job(event) -> str label or NULL (None). */
static char *hrd_scheduled_job(const json_t *event) {
    if (!event || event->type == JSON_NULL) return NULL;
    if (strcmp(json_get_str(event, "kind", ""), "message.user") != 0) return NULL;
    char *kind = hrd_scheduled_post_kind(json_obj_get(event, "actor"));
    if (!kind) return NULL; /* None -> None (a raise parks and returns NULL too;
                               callers check pending) */
    free(kind);
    json_t *actor = json_obj_get(event, "actor");
    json_t *dn = actor ? json_obj_get(actor, "display_name") : NULL;
    json_t *id = actor ? json_obj_get(actor, "id") : NULL;
    return hrd_py_str(hrd_truth(dn) ? dn : id);
}

/* _first_round_responders(discussion, room) */
static json_t *hrd_first_round_responders(const json_t *discussion, const json_t *room) {
    char *sched = hrd_scheduled_post_kind(json_obj_get(discussion, "actor"));
    if (g_hrd_pending_raise) return NULL;
    if (sched && strcmp(sched, "notice") == 0) { free(sched); return json_array(); }
    json_t *texts = json_array();
    char *txt = hrd_py_str(json_obj_get(json_obj_get(discussion, "payload"), "text"));
    json_append(texts, json_string(txt));
    free(txt);
    json_t *out = hrd_resolve_mentions(texts, json_obj_get(room, "members"), sched == NULL);
    json_free(texts);
    free(sched);
    return out;
}

/* member in resolve_mentions(...) — dataclass membership by field equality */
static int hrd_member_in_resolved(const json_t *texts, const json_t *members,
                                  int default_all, const json_t *member) {
    json_t *resolved = hrd_resolve_mentions(texts, members, default_all);
    int found = 0;
    for (size_t i = 0; i < resolved->c.count; i++)
        if (hrd_member_eq(resolved->c.items[i], member)) { found = 1; break; }
    json_free(resolved);
    return found;
}

/* _requires_direct_reply(room, source, member, round_index) */
static int hrd_requires_direct_reply(const json_t *room, const json_t *source,
                                     const json_t *member, long long round_index) {
    if (round_index != 0 || !source || source->type == JSON_NULL) return 0;
    if (strcmp(json_get_str(source, "kind", ""), "message.user") != 0) return 0;
    json_t *texts = json_array();
    char *txt = hrd_py_str(json_obj_get(json_obj_get(source, "payload"), "text"));
    json_append(texts, json_string(txt));
    free(txt);
    int found = hrd_member_in_resolved(texts, json_obj_get(room, "members"), 0, member);
    json_free(texts);
    return found;
}

/* _event_attachments(event, member) -> array of attachment dicts */
static json_t *hrd_event_attachments(const json_t *event, const json_t *member) {
    json_t *out = json_array();
    json_t *payload = json_obj_get(event, "payload");
    const char *kind = json_get_str(event, "kind", "");
    if (strcmp(kind, "message.member") == 0) {
        json_t *mid = json_obj_get(payload, "member_id");
        json_t *want = json_string(json_get_str(member, "member_id", ""));
        int same = hrd_json_eq(mid, want);
        json_free(want);
        if (same) return out;
        json_t *recipients = payload ? json_obj_get(payload, "recipient_member_ids") : NULL;
        if (recipients && recipients->type != JSON_NULL) {
            int in = 0;
            if (recipients->type == JSON_ARRAY)
                for (size_t i = 0; i < recipients->c.count; i++) {
                    json_t *r = recipients->c.items[i];
                    if (r && r->type == JSON_STRING &&
                        strcmp(r->str_val, json_get_str(member, "member_id", "")) == 0) in = 1;
                }
            if (!in) return out;
        }
    }
    json_t *atts = payload ? json_obj_get(payload, "attachments") : NULL;
    if (atts && atts->type == JSON_ARRAY) {
        for (size_t i = 0; i < atts->c.count; i++) {
            json_t *item = json_copy(atts->c.items[i]);
            if (item && item->type == JSON_OBJECT)
                json_set_hdr(item, "event_id", json_get_str(event, "event_id", ""));
            json_append(out, item);
        }
    }
    return out;
}

/* _terminal_text(result, field=..., fallback=...) */
static char *hrd_terminal_text(const json_t *result, const char *field, const char *fallback) {
    const json_t *v = result;
    if (result && result->type == JSON_OBJECT) {
        json_t *err = json_obj_get(result, "error");
        if (strcmp(field, "error") != 0 || (err && err->type != JSON_NULL))
            v = json_obj_get(result, field);
        else
            v = json_obj_get(result, "text");
    }
    char *s = hrd_truth(v) ? hrd_py_str(v) : strdup("");
    char *stripped = hrd_strip(s);
    free(s);
    if (stripped[0]) return stripped;
    free(stripped);
    return strdup(fallback);
}

/* _truncate_utf8_text(value, max_bytes=..., suffix=...) */
static char *hrd_truncate_utf8(const json_t *value, long long max_bytes, const char *suffix) {
    char *text = hrd_truth(value) ? hrd_py_str(value) : strdup("");
    size_t len = strlen(text);
    if ((long long)len <= max_bytes) return text;
    size_t slen = strlen(suffix);
    long long cut = max_bytes - (long long)slen;
    if (cut < 0) cut = 0;
    /* decode the valid prefix, dropping an incomplete trailing sequence */
    size_t i = 0;
    while (i < (size_t)cut) {
        uint32_t cp;
        size_t n = hrd_utf8_decode(text, len, i, &cp);
        if (i + n > (size_t)cut) break;
        i += n;
    }
    if (i == 0) {
        free(text);
        return hrd_strip(suffix);
    }
    char *out = malloc(i + slen + 1);
    memcpy(out, text, i);
    memcpy(out + i, suffix, slen);
    out[i + slen] = '\0';
    free(text);
    return out;
}

/* is_pass_text: str(value or "").strip() then fullmatch
 * r"\(?\s*pass\s*\)?\.?" IGNORECASE */
static int hrd_is_pass_text(const json_t *value) {
    char *s = hrd_truth(value) ? hrd_py_str(value) : strdup("");
    char *t = hrd_strip(s);
    free(s);
    if (!t[0]) { free(t); return 1; }
    size_t i = 0, n = strlen(t);
    if (t[i] == '(') i++;
    while (i < n) {
        uint32_t cp; size_t k = hrd_utf8_decode(t, n, i, &cp);
        if (!hrd_ws_cp(cp)) break;
        i += k;
    }
    if (i + 4 > n) { free(t); return 0; }
    static const char *pass = "pass";
    for (int k = 0; k < 4; k++) {
        char c = t[i + k];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if (c != pass[k]) { free(t); return 0; }
    }
    i += 4;
    while (i < n) {
        uint32_t cp; size_t k = hrd_utf8_decode(t, n, i, &cp);
        if (!hrd_ws_cp(cp)) break;
        i += k;
    }
    if (i < n && t[i] == ')') i++;
    if (i < n && t[i] == '.') i++;
    int ok = i == n;
    free(t);
    return ok;
}

static char *hrd_strndup(const char *s, size_t n) {
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}

/* _TURN_ID_RE fullmatch; groups: source, round, position, seen, member.
 * Returns 1 on match with malloc'd group strings in g[5]. */
static int hrd_turn_id_match(const char *s, char *g[5]) {
    size_t i = 0, n = strlen(s);
    for (int k = 0; k < 5; k++) g[k] = NULL;
    if (i >= n || s[i] != 'd') return 0;
    i++;
    size_t start = i;
    if (i >= n || s[i] < '1' || s[i] > '9') return 0;
    while (i < n && s[i] >= '0' && s[i] <= '9') i++;
    g[0] = hrd_strndup(s + start, i - start);
    if (i + 2 > n || s[i] != '.' || s[i+1] != 'r') goto fail;
    i += 2;
    start = i;
    if (i < n && s[i] == '0') { i++; }
    else {
        if (i >= n || s[i] < '1' || s[i] > '9') goto fail;
        while (i < n && s[i] >= '0' && s[i] <= '9') i++;
    }
    g[1] = hrd_strndup(s + start, i - start);
    if (i + 2 > n || s[i] != '.' || s[i+1] != 'p') goto fail;
    i += 2;
    if (i >= n || s[i] < '0' || s[i] > '5') goto fail;
    g[2] = hrd_strndup(s + i, 1);
    i++;
    if (i + 2 > n || s[i] != '.' || s[i+1] != 's') goto fail;
    i += 2;
    start = i;
    if (i >= n || s[i] < '1' || s[i] > '9') goto fail;
    while (i < n && s[i] >= '0' && s[i] <= '9') i++;
    g[3] = hrd_strndup(s + start, i - start);
    if (i + 2 > n || s[i] != '.' || s[i+1] != 'm') goto fail;
    i += 2;
    if (i + 24 > n) goto fail;
    for (int k = 0; k < 24; k++) {
        char c = s[i + k];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) goto fail;
    }
    g[4] = hrd_strndup(s + i, 24);
    i += 24;
    if (i != n) goto fail;
    return 1;
fail:
    for (int k = 0; k < 5; k++) { free(g[k]); g[k] = NULL; }
    return 0;
}

/* _member_actor(member, display_name=True) */
static json_t *hrd_member_actor(const json_t *member) {
    json_t *actor = json_object();
    json_set_hdr(actor, "kind", "member");
    json_set_hdr(actor, "id", json_get_str(member, "member_id", ""));
    json_set_hdr(actor, "profile", json_get_str(member, "profile", ""));
    json_t *target = json_obj_get(member, "target");
    if (hrd_truth(target) && strcmp(json_get_str(target, "kind", ""), "peer") == 0) {
        json_t *pid = json_obj_get(target, "peer_id");
        if (pid && pid->type != JSON_NULL)
            json_set(actor, "connection_id", json_copy(pid));
    }
    const char *dn = json_get_str(member, "display_name", "");
    if (dn[0]) json_set_hdr(actor, "display_name", dn);
    return actor;
}

/* _turn_coordinates(task) */
static json_t *hrd_turn_coordinates(const json_t *task) {
    json_t *identity = json_obj_get(task, "identity");
    json_t *member = json_obj_get(task, "member");
    json_t *o = json_object();
    json_set(o, "discussion_event_id", json_copy(json_obj_get(task, "discussion_event_id")));
    json_set_hdr(o, "member_id", json_get_str(member, "member_id", ""));
    json_set(o, "member_index", json_copy(json_obj_get(task, "member_index")));
    json_set(o, "round_index", json_copy(json_obj_get(task, "round_index")));
    json_set_hdr(o, "task_id", json_get_str(identity, "task_id", ""));
    json_set_hdr(o, "thread_id", json_get_str(identity, "thread_id", ""));
    json_set_hdr(o, "turn_id", json_get_str(identity, "turn_id", ""));
    return o;
}

/* _reconstructed_lead_routing(payload, match, source_event_seq) */
static int hrd_reconstructed_lead_routing(const json_t *payload, char *g[5],
                                          long long source_event_seq, int *out) {
    *out = 0;
    if (!json_obj_get(payload, "lead_routing")) return 0;
    json_t *lr = json_obj_get(payload, "lead_routing");
    /* int(match.group("seen")) — the regex guarantees digits */
    char *end = NULL;
    long long seen_v = strtoll(g[3], &end, 10);
    int bad = !(lr && lr->type == JSON_BOOL && lr->bool_val) ||
              strcmp(g[1], "0") != 0 || strcmp(g[2], "0") != 0 ||
              seen_v != source_event_seq - 1 ||
              json_obj_get(payload, "input_context") != NULL ||
              json_obj_get(payload, "attachments") != NULL;
    if (bad) {
        hrd_park(hrd_raise_spec("DiscussionReconstructionError",
                                "lead routing task does not match its turn_id"));
        return -1;
    }
    *out = 1;
    return 0;
}

/* ── _make_task_plan core (shared by the public port and reconstruct) ──
 * input_context and attachments are OWNED (NULL models None). */
static json_t *hrd_make_task_plan(const json_t *room, const json_t *discussion_event,
                                  const json_t *member, long long member_index,
                                  long long round_index, long long seen,
                                  const char *prompt, json_t *input_context,
                                  json_t *attachments, int lead_routing) {
    json_t *result = NULL, *seed = NULL, *identity = NULL, *payload = NULL;
    char *digest = NULL, *cj = NULL, *hex = NULL, *turn_id = NULL;
    char *rid = NULL, *tid = NULL, *thid = NULL, *tuid = NULL;
    long long dseq = (long long)json_get_num(discussion_event, "seq", 0);
    digest = hrd_member_digest(member);
    if (!digest) goto done;
    {
        char buf[256];
        snprintf(buf, sizeof buf, "d%lld.r%lld.p%lld.s%lld.m%s",
                 dseq, round_index, member_index, seen, digest);
        turn_id = strdup(buf);
    }
    if (hrd_truth(attachments)) {
        json_t *args = json_object();
        json_set(args, "value", json_copy(attachments));
        json_t *r = hrd_seam("validate_bound_task_manifest", args);
        if (hrd_raise_kind(r)) { hrd_park(r); goto done; }
        json_free(attachments);
        attachments = r;
    }
    seed = json_object();
    json_set(seed, "discussion_event_id", json_copy(json_obj_get(discussion_event, "event_id")));
    json_set_hdr(seed, "member_id", json_get_str(member, "member_id", ""));
    json_set(seed, "member_index", json_number((double)member_index));
    if (!lead_routing) {
        char *ph = hrd_sha256_hex((const unsigned char *)prompt, strlen(prompt));
        if (!ph) goto done;
        json_set_hdr(seed, "prompt_sha256", ph);
        free(ph);
    }
    json_set_hdr(seed, "room_id", json_get_str(room, "room_id", ""));
    json_set(seed, "round_index", json_number((double)round_index));
    json_set(seed, "seen_through_seq", json_number((double)seen));
    json_set(seed, "source_event_seq", json_number((double)dseq));
    {
        json_t *th = hrd_payload_req(json_obj_get(discussion_event, "payload"), "thread_id");
        if (!th) goto done;
        json_set(seed, "thread_id", json_copy(th));
    }
    if (input_context) json_set(seed, "input_context", json_copy(input_context));
    if (hrd_truth(attachments)) json_set(seed, "attachments", json_copy(attachments));
    if (lead_routing) json_set(seed, "lead_routing", json_bool(true));
    cj = hrd_compact_json(seed, 1);
    hex = hrd_sha256_hex((const unsigned char *)cj, strlen(cj));
    if (!hex) goto done;
    {
        char task_id[64];
        snprintf(task_id, sizeof task_id, "dtask:%.48s", hex);
        /* HostedTaskIdentity(__post_init__ strips each field) */
        rid = hrd_identifier(json_obj_get(room, "room_id"), "room_id", "DriverValidationError");
        if (!rid) goto done;
        json_t *tidnode = json_string(task_id);
        tid = hrd_identifier(tidnode, "task_id", "DriverValidationError");
        json_free(tidnode);
        if (!tid) goto done;
        json_t *th = json_obj_get(json_obj_get(discussion_event, "payload"), "thread_id");
        char *ths = hrd_py_str(th);
        json_t *thnode = json_string(ths);
        free(ths);
        thid = hrd_identifier(thnode, "thread_id", "DriverValidationError");
        json_free(thnode);
        if (!thid) goto done;
        json_t *tuidnode = json_string(turn_id);
        tuid = hrd_identifier(tuidnode, "turn_id", "DriverValidationError");
        json_free(tuidnode);
        if (!tuid) goto done;
        identity = json_object();
        json_set_hdr(identity, "room_id", rid);
        json_set_hdr(identity, "task_id", tid);
        json_set_hdr(identity, "thread_id", thid);
        json_set_hdr(identity, "turn_id", tuid);
    }
    payload = json_object();
    json_set_hdr(payload, "target_member_id", json_get_str(member, "member_id", ""));
    json_set_hdr(payload, "target_profile", json_get_str(member, "profile", ""));
    json_set_hdr(payload, "prompt", prompt);
    json_set(payload, "source_event_seq", json_number((double)dseq));
    if (hrd_truth(attachments)) {
        json_t *list = json_array();
        if (attachments->type == JSON_ARRAY)
            for (size_t i = 0; i < attachments->c.count; i++)
                json_append(list, json_copy(attachments->c.items[i]));
        json_set(payload, "attachments", list);
    }
    if (input_context) json_set(payload, "input_context", json_copy(input_context));
    if (lead_routing) json_set(payload, "lead_routing", json_bool(true));
    result = json_object();
    json_set(result, "identity", identity); identity = NULL;
    json_set(result, "payload", payload); payload = NULL;
    json_set(result, "discussion_event_id", json_copy(json_obj_get(discussion_event, "event_id")));
    json_set(result, "member", hrd_member_norm(member));
    json_set(result, "member_index", json_number((double)member_index));
    json_set(result, "round_index", json_number((double)round_index));
    json_set(result, "seen_through_seq", json_number((double)seen));
done:
    free(digest); free(cj); free(hex); free(turn_id);
    free(rid); free(tid); free(thid); free(tuid);
    json_free(seed); json_free(identity); json_free(payload);
    json_free(input_context); json_free(attachments);
    return result;
}

/* ── terminal effect builders ── */
static json_t *hrd_event_plan(const char *event_id, const char *kind,
                              json_t *actor, json_t *payload, const json_t *room) {
    json_t *o = json_object();
    json_set_hdr(o, "event_id", event_id);
    json_set_hdr(o, "kind", kind);
    json_set(o, "actor", actor);     /* adopts */
    json_set(o, "payload", payload); /* adopts */
    json_set_hdr(o, "authority_gateway_id", json_get_str(room, "gateway_id", ""));
    json_set(o, "authority_epoch", json_copy(json_obj_get(room, "authority_epoch")));
    return o;
}

#define HRD_MAX_MEMBER_TEXT_BYTES 65536
static const char HRD_TRUNC_NOTICE[] =
    "\n\n[Reply truncated. Ask the Bot to share the full result as a file.]";

/* _settled_effects: extra + effects (both owned outputs). */
static void hrd_settled_effects(const json_t *result, const json_t *task,
                                const json_t *room, const char *message_event_id,
                                json_t **extra, json_t **effects) {
    char *tt = hrd_terminal_text(result, "text", "");
    json_t *ttn = json_string(tt);
    free(tt);
    char *text = hrd_truncate_utf8(ttn, HRD_MAX_MEMBER_TEXT_BYTES, HRD_TRUNC_NOTICE);
    json_free(ttn);
    *effects = json_array();
    *extra = json_object();
    json_t *textnode = json_string(text);
    if (hrd_is_pass_text(textnode)) {
        json_set(*extra, "message_event_id", json_null());
        json_set(*extra, "passed", json_bool(true));
    } else {
        json_set_hdr(*extra, "message_event_id", message_event_id);
        json_set(*extra, "passed", json_bool(false));
        json_t *payload = hrd_turn_coordinates(task);
        json_set(payload, "text", json_copy(textnode));
        json_append(*effects, hrd_event_plan(message_event_id, "message.member",
                                             hrd_member_actor(json_obj_get(task, "member")),
                                             payload, room));
    }
    json_free(textnode);
    free(text);
}

/* _failed_effects: live order is error_text -> from-import classify ->
 * supplied_reason -> _all_failure_reasons() -> conditional classify call.
 * Returns 0 ok / -1 raise parked. */
static int hrd_failed_effects(const json_t *result, json_t **extra, json_t **effects) {
    *effects = json_array();
    *extra = json_object();
    char *error_text = hrd_terminal_text(result, "error", "member turn failed");
    json_t *imp = hrd_seam("import_classify_agent_error", json_object());
    if (hrd_raise_kind(imp)) {
        hrd_park(imp);
        free(error_text);
        json_free(*extra); *extra = NULL;
        json_free(*effects); *effects = NULL;
        return -1;
    }
    json_free(imp);
    char *supplied = strdup("");
    if (result && result->type == JSON_OBJECT) {
        json_t *rc = json_obj_get(result, "reason_code");
        json_t *rn = json_obj_get(result, "reason");
        json_t *pick = hrd_truth(rc) ? rc : (hrd_truth(rn) ? rn : NULL);
        if (pick) {
            char *s = hrd_py_str(pick);
            free(supplied);
            supplied = hrd_strip(s);
            free(s);
        }
    }
    json_t *reasons = hrd_seam("all_failure_reasons", json_object());
    if (hrd_raise_kind(reasons)) {
        hrd_park(reasons);
        free(error_text); free(supplied);
        json_free(*extra); *extra = NULL;
        json_free(*effects); *effects = NULL;
        return -1;
    }
    int known = 0;
    if (reasons && reasons->type == JSON_ARRAY)
        for (size_t i = 0; i < reasons->c.count; i++) {
            json_t *r = reasons->c.items[i];
            if (r && r->type == JSON_STRING && strcmp(r->str_val, supplied) == 0) known = 1;
        }
    json_free(reasons);
    char *reason_code;
    if (known) {
        reason_code = strdup(supplied);
    } else {
        json_t *args = json_object();
        json_set_hdr(args, "error_text", error_text);
        json_t *r = hrd_seam("classify_agent_error", args);
        if (hrd_raise_kind(r)) {
            hrd_park(r);
            free(error_text); free(supplied);
            json_free(*extra); *extra = NULL;
            json_free(*effects); *effects = NULL;
            return -1;
        }
        reason_code = hrd_py_str(r);
        json_free(r);
    }
    json_set_hdr(*extra, "error", error_text);
    json_set_hdr(*extra, "reason_code", reason_code);
    free(error_text); free(supplied); free(reason_code);
    return 0;
}

/* ── plan_publication ── */
static json_t *hrd_plan_publication(const json_t *work) {
    json_t *room = NULL, *validated = NULL, *extra = NULL, *effects = NULL;
    json_t *result = NULL, *out = NULL;
    json_t *task = json_obj_get(work, "task");
    const char *status = json_get_str(work, "status", "");
    json_t *identity = json_obj_get(task, "identity");
    json_t *tpayload = json_obj_get(task, "payload");
    json_t *tmember = json_obj_get(task, "member");
    /* room + validated seams */
    json_t *rargs = json_object();
    json_set(rargs, "room_value", json_copy(json_obj_get(work, "room_value")));
    json_set(rargs, "local_profiles", json_copy(json_obj_get(work, "local_profiles")));
    room = hrd_seam("validate_room", rargs);
    if (hrd_raise_kind(room)) { hrd_park(room); return NULL; }
    json_t *vargs = json_object();
    json_set(vargs, "events", json_copy(json_obj_get(work, "events")));
    json_set(vargs, "room", json_copy(room));
    validated = hrd_seam("validated_events", vargs);
    if (hrd_raise_kind(validated)) { hrd_park(validated); json_free(room); return NULL; }
    /* the three validation conditions (all evaluated, first true raises) */
    {
        int c1 = strcmp(json_get_str(identity, "room_id", ""), json_get_str(room, "room_id", "")) != 0;
        int c2 = 1;
        json_t *members = json_obj_get(room, "members");
        if (members && members->type == JSON_ARRAY)
            for (size_t i = 0; i < members->c.count; i++)
                if (hrd_member_eq(members->c.items[i], tmember)) { c2 = 0; break; }
        int c3 = strcmp(status, "settled") != 0 && strcmp(status, "failed") != 0 &&
                 strcmp(status, "cancelled") != 0 && strcmp(status, "deferred") != 0;
        const char *msg = c1 ? "task belongs to a different room" :
                          c2 ? "task member is not in the frozen roster" :
                          c3 ? "invalid terminal publication status" : NULL;
        if (msg) {
            hrd_park(hrd_raise_spec("DiscussionValidationError", msg));
            goto fail;
        }
    }
    json_t *gen = json_obj_get(work, "execution_generation");
    if (strcmp(status, "deferred") == 0) {
        long long g;
        if (hrd_bounded_int(gen, "deferred publication requires an execution generation", 1, 0, 0, &g) < 0)
            goto fail;
    }
    /* newer_same_thread */
    int newer = 0;
    {
        json_t *sv = hrd_payload_req(tpayload, "source_event_seq");
        if (!sv) goto fail;
        long long sseq;
        if (hrd_py_int(sv, &sseq) < 0) goto fail;
        size_t ne = validated->type == JSON_ARRAY ? validated->c.count : 0;
        for (size_t i = 0; i < ne; i++) {
            json_t *ev = validated->c.items[i];
            if (strcmp(json_get_str(ev, "kind", ""), "message.user") != 0) continue;
            if ((long long)json_get_num(ev, "seq", 0) <= sseq) continue;
            json_t *th = json_obj_get(json_obj_get(ev, "payload"), "thread_id");
            json_t *want = json_string(json_get_str(identity, "thread_id", ""));
            if (hrd_json_eq(th, want)) newer = 1;
            json_free(want);
            if (newer) break;
        }
    }
    const char *effective = (newer && strcmp(status, "deferred") != 0) ? "cancelled" : status;
    /* source event */
    json_t *source = NULL;
    {
        size_t ne = validated->type == JSON_ARRAY ? validated->c.count : 0;
        for (size_t i = 0; i < ne; i++) {
            json_t *ev = validated->c.items[i];
            if (hrd_json_eq(json_obj_get(ev, "event_id"), json_obj_get(task, "discussion_event_id"))) {
                source = ev; break;
            }
        }
    }
    result = json_copy(json_obj_get(work, "result"));
    if (strcmp(effective, "settled") == 0 &&
        hrd_requires_direct_reply(room, source, tmember,
                                  (long long)json_get_num(task, "round_index", 0))) {
        char *tt = hrd_terminal_text(result, "text", "");
        json_t *ttn = json_string(tt);
        free(tt);
        int pass = hrd_is_pass_text(ttn);
        json_free(ttn);
        if (pass) {
            effective = "failed";
            json_free(result);
            result = json_object();
            char msg[320];
            snprintf(msg, sizeof msg, "@%s was directly addressed but returned no visible reply.",
                     json_get_str(tmember, "handle", ""));
            json_set_hdr(result, "error", msg);
        }
    }
    /* digest + terminal event id */
    const char *task_id = json_get_str(identity, "task_id", "");
    const char *digest = strncmp(task_id, "dtask:", 6) == 0 ? task_id + 6 : task_id;
    char terminal_event_id[256];
    if (strcmp(effective, "deferred") == 0) {
        long long g = gen && gen->type == JSON_NUMBER ? (long long)gen->num_val : 0;
        snprintf(terminal_event_id, sizeof terminal_event_id, "ddeferred:%s:g%lld", digest, g);
    } else {
        snprintf(terminal_event_id, sizeof terminal_event_id, "dterminal:%s", digest);
    }
    /* routing */
    json_t *rtargs = json_object();
    json_set(rtargs, "task", json_copy(task));
    json_t *routing_r = hrd_seam("lead_routing.is_routing_task", rtargs);
    if (hrd_raise_kind(routing_r)) { hrd_park(routing_r); goto fail; }
    int routing = routing_r && routing_r->type == JSON_BOOL && routing_r->bool_val;
    json_free(routing_r);
    /* effects builder dispatch */
    const json_t *eff_result = result;
    json_t *pass_node = NULL;
    if (routing && strcmp(effective, "settled") == 0) {
        pass_node = json_string("(pass)");
        eff_result = pass_node;
    }
    char message_event_id[256];
    snprintf(message_event_id, sizeof message_event_id, "dmessage:%s", digest);
    if (strcmp(effective, "settled") == 0) {
        hrd_settled_effects(eff_result, task, room, message_event_id, &extra, &effects);
    } else if (strcmp(effective, "failed") == 0) {
        if (hrd_failed_effects(eff_result, &extra, &effects) < 0) { json_free(pass_node); goto fail; }
    } else if (strcmp(effective, "cancelled") == 0) {
        extra = json_object();
        effects = json_array();
        if (newer) {
            json_set_hdr(extra, "reason", "superseded_by_newer_user_event");
        } else {
            char *r = hrd_terminal_text(eff_result, "reason", "member turn cancelled");
            json_set_hdr(extra, "reason", r);
            free(r);
        }
    } else { /* deferred */
        extra = json_object();
        effects = json_array();
        char *r = hrd_terminal_text(eff_result, "reason", "member_unavailable");
        json_set(extra, "execution_generation", json_copy(gen));
        json_set_hdr(extra, "reason", r);
        free(r);
    }
    json_free(pass_node);
    if (routing) {
        json_t *aeargs = json_object();
        json_set(aeargs, "result", json_copy(result));
        json_set(aeargs, "task", json_copy(task));
        json_set(aeargs, "room", json_copy(room));
        json_set_hdr(aeargs, "status", effective);
        json_t *ae = hrd_seam("lead_routing.assignment_effect", aeargs);
        if (hrd_raise_kind(ae)) { hrd_park(ae); goto fail; }
        json_t *combined = json_array();
        if (ae && ae->type == JSON_ARRAY)
            for (size_t i = 0; i < ae->c.count; i++)
                json_append(combined, json_copy(ae->c.items[i]));
        json_free(ae);
        for (size_t i = 0; i < effects->c.count; i++)
            json_append(combined, json_copy(effects->c.items[i]));
        json_free(effects);
        effects = combined;
    }
    /* terminal event */
    {
        char kind[64];
        snprintf(kind, sizeof kind, "turn.%s", effective);
        json_t *actor = json_object();
        json_set_hdr(actor, "kind", "gateway");
        json_set_hdr(actor, "id", json_get_str(room, "gateway_id", ""));
        json_t *payload = hrd_turn_coordinates(task);
        json_set(payload, "seen_through_seq", json_copy(json_obj_get(task, "seen_through_seq")));
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(payload, extra->c.keys[i], json_copy(extra->c.items[i]));
        json_append(effects, hrd_event_plan(terminal_event_id, kind, actor, payload, room));
    }
    out = json_object();
    json_set_hdr(out, "task_id", task_id);
    {
        char kind[64];
        snprintf(kind, sizeof kind, "turn.%s", effective);
        json_set_hdr(out, "terminal_kind", kind);
    }
    json_set(out, "events", effects); effects = NULL;
fail:
    json_free(room); json_free(validated); json_free(extra); json_free(effects);
    json_free(result);
    return out;
}

/* ── reconstruct_task_plan ── */
static json_t *hrd_reconstruct_task_plan(const json_t *work) {
    json_t *room = NULL, *validated = NULL, *out = NULL;
    json_t *input_context = NULL, *attachments = NULL;
    char *g[5] = {NULL, NULL, NULL, NULL, NULL};
    char *prompt_copy = NULL;
    json_t *task = json_obj_get(work, "task");
    json_t *identity = task ? json_obj_get(task, "identity") : NULL;
    json_t *payload = task ? json_obj_get(task, "payload") : NULL;
    json_t *rargs = json_object();
    json_set(rargs, "room_value", json_copy(json_obj_get(work, "room_value")));
    json_set(rargs, "local_profiles", json_copy(json_obj_get(work, "local_profiles")));
    room = hrd_seam("validate_room", rargs);
    if (hrd_raise_kind(room)) { hrd_park(room); return NULL; }
    json_t *vargs = json_object();
    json_set(vargs, "events", json_copy(json_obj_get(work, "events")));
    json_set(vargs, "room", json_copy(room));
    validated = hrd_seam("validated_events", vargs);
    if (hrd_raise_kind(validated)) { hrd_park(validated); json_free(room); return NULL; }
    if (!identity || identity->type != JSON_OBJECT || !payload || payload->type != JSON_OBJECT) {
        hrd_park(hrd_raise_spec("DiscussionReconstructionError",
                                "driver task has no valid identity or payload"));
        goto fail;
    }
    /* payload field shape */
    {
        static const char *const req[] = { "target_profile", "prompt", "source_event_seq" };
        static const char *const opt[] = { "target_member_id", "input_context", "attachments", "lead_routing" };
        int bad = 0;
        for (size_t i = 0; i < 3; i++) if (!json_obj_get(payload, req[i])) bad = 1;
        for (size_t i = 0; i < payload->c.count && !bad; i++) {
            const char *k = payload->c.keys[i];
            int ok = 0;
            for (size_t j = 0; j < 3; j++) if (strcmp(k, req[j]) == 0) ok = 1;
            for (size_t j = 0; j < 4; j++) if (strcmp(k, opt[j]) == 0) ok = 1;
            if (!ok) bad = 1;
        }
        if (bad) {
            hrd_park(hrd_raise_spec("DiscussionReconstructionError", "driver task payload shape changed"));
            goto fail;
        }
    }
    {
        json_t *turn = json_obj_get(identity, "turn_id");
        if (!turn || turn->type != JSON_STRING) {
            hrd_park(hrd_raise_spec("TypeError", "expected string or bytes-like object, got 'NoneType'"));
            goto fail;
        }
        if (!hrd_turn_id_match(turn->str_val, g)) {
            hrd_park(hrd_raise_spec("DiscussionReconstructionError", "turn_id is not a Discussion coordinate"));
            goto fail;
        }
    }
    long long source_seq = strtoll(g[0], NULL, 10);
    {
        json_t *sv = json_obj_get(payload, "source_event_seq");
        json_t *want = json_number((double)source_seq);
        int eq = hrd_json_eq(sv, want);
        json_free(want);
        if (!eq) {
            hrd_park(hrd_raise_spec("DiscussionReconstructionError", "task source event does not match turn_id"));
            goto fail;
        }
    }
    json_t *discussion = NULL;
    size_t ne = validated->type == JSON_ARRAY ? validated->c.count : 0;
    for (size_t i = 0; i < ne; i++) {
        json_t *ev = validated->c.items[i];
        if ((long long)json_get_num(ev, "seq", 0) == source_seq &&
            strcmp(json_get_str(ev, "kind", ""), "message.user") == 0) { discussion = ev; break; }
    }
    if (!discussion) {
        hrd_park(hrd_raise_spec("DiscussionReconstructionError", "task source user event is missing"));
        goto fail;
    }
    if (strcmp(json_get_str(identity, "room_id", ""), json_get_str(room, "room_id", "")) != 0 ||
        !hrd_json_eq(json_obj_get(identity, "thread_id"),
                     json_obj_get(json_obj_get(discussion, "payload"), "thread_id"))) {
        hrd_park(hrd_raise_spec("DiscussionReconstructionError", "task identity does not match its room thread"));
        goto fail;
    }
    json_t *profile = json_obj_get(payload, "target_profile");
    json_t *target_mid = json_obj_get(payload, "target_member_id");
    json_t *member = NULL;
    {
        json_t *members = json_obj_get(room, "members");
        size_t nm = members && members->type == JSON_ARRAY ? members->c.count : 0;
        for (size_t i = 0; i < nm; i++) {
            json_t *m = members->c.items[i];
            json_t *mp = json_obj_get(m, "profile");
            if (!hrd_json_eq(mp, profile)) continue;
            if (target_mid && target_mid->type != JSON_NULL) {
                json_t *mm = json_obj_get(m, "member_id");
                if (!hrd_json_eq(mm, target_mid)) continue;
            }
            member = m; break;
        }
    }
    {
        char *digest = member ? hrd_member_digest(member) : NULL;
        int bad = !member || !digest || strcmp(digest, g[4]) != 0;
        free(digest);
        if (bad) {
            hrd_park(hrd_raise_spec("DiscussionReconstructionError", "task target member does not match turn_id"));
            goto fail;
        }
    }
    {
        json_t *pv = json_obj_get(payload, "prompt");
        char *stripped = pv && pv->type == JSON_STRING ? hrd_strip(pv->str_val) : NULL;
        int bad = !pv || pv->type != JSON_STRING || !stripped || !stripped[0];
        free(stripped);
        if (bad) {
            hrd_park(hrd_raise_spec("DiscussionReconstructionError", "task prompt is missing"));
            goto fail;
        }
        if (strlen(pv->str_val) > 128 * 1024) { /* driver.MAX_PROMPT_BYTES */
            hrd_park(hrd_raise_spec("DiscussionReconstructionError", "task prompt exceeds the driver limit"));
            goto fail;
        }
        prompt_copy = strdup(pv->str_val);
    }
    if (json_obj_get(payload, "input_context")) {
        json_t *viargs = json_object();
        json_set(viargs, "value", json_copy(json_obj_get(payload, "input_context")));
        json_t *r = hrd_seam("validate_task_input", viargs);
        const char *kind = hrd_raise_kind(r);
        if (kind) {
            if (strcmp(kind, "ValueError") == 0 && !hrd_is_base_exc(kind)) {
                char *msg = strdup(json_get_str(r, "message", ""));
                json_free(r);
                hrd_park(hrd_raise_spec("DiscussionReconstructionError", msg));
                free(msg);
                goto fail;
            }
            hrd_park(r);
            goto fail;
        }
        input_context = r;
        json_t *seqs = json_obj_get(input_context, "event_seqs");
        if (!seqs) { hrd_park(hrd_keyerror("event_seqs")); goto fail; }
        if (seqs->type != JSON_ARRAY || seqs->c.count == 0) {
            hrd_park(hrd_raise_spec("IndexError", "list index out of range"));
            goto fail;
        }
        json_t *last = seqs->c.items[seqs->c.count - 1];
        long long seen = strtoll(g[3], NULL, 10);
        json_t *want = json_number((double)seen);
        int eq = hrd_json_eq(last, want);
        json_free(want);
        if (!eq) {
            hrd_park(hrd_raise_spec("DiscussionReconstructionError", "task input does not match turn_id"));
            goto fail;
        }
        /* message_seqs membership */
        for (size_t i = 0; i < seqs->c.count; i++) {
            json_t *sq = seqs->c.items[i];
            int found = 0;
            for (size_t k = 0; k < ne; k++) {
                json_t *ev = validated->c.items[k];
                const char *ek = json_get_str(ev, "kind", "");
                if (strcmp(ek, "message.user") != 0 && strcmp(ek, "message.member") != 0) continue;
                json_t *th = json_obj_get(json_obj_get(ev, "payload"), "thread_id");
                if (!hrd_json_eq(th, json_obj_get(identity, "thread_id"))) continue;
                json_t *es = json_obj_get(ev, "seq");
                if (hrd_json_eq(es, sq)) { found = 1; break; }
            }
            if (!found) {
                hrd_park(hrd_raise_spec("DiscussionReconstructionError", "task input message is missing"));
                goto fail;
            }
        }
    }
    attachments = json_array();
    if (json_obj_get(payload, "attachments")) {
        if (!input_context) {
            hrd_park(hrd_raise_spec("DiscussionReconstructionError", "attachment task requires frozen input_context"));
            goto fail;
        }
        json_t *seqs = json_obj_get(input_context, "event_seqs");
        for (size_t i = 0; i < seqs->c.count; i++) {
            json_t *sq = seqs->c.items[i];
            json_t *ev = NULL;
            for (size_t k = 0; k < ne; k++) {
                json_t *cand = validated->c.items[k];
                if (hrd_json_eq(json_obj_get(cand, "seq"), sq)) { ev = cand; break; }
            }
            if (!ev) {
                char *ks = hrd_py_str(sq);
                char msg[128];
                snprintf(msg, sizeof msg, "%s", ks);
                hrd_park(hrd_raise_spec("KeyError", msg));
                free(ks);
                goto fail;
            }
            json_t *items = hrd_event_attachments(ev, member);
            for (size_t k = 0; k < items->c.count; k++)
                json_append(attachments, json_copy(items->c.items[k]));
            json_free(items);
        }
    }
    int lead_routing = 0;
    if (hrd_reconstructed_lead_routing(payload, g, source_seq, &lead_routing) < 0) goto fail;
    out = hrd_make_task_plan(room, discussion, member,
                             strtoll(g[2], NULL, 10), strtoll(g[1], NULL, 10),
                             strtoll(g[3], NULL, 10), prompt_copy,
                             input_context, attachments, lead_routing);
    input_context = NULL; attachments = NULL; /* owned by the core now */
    if (!out) goto fail;
    /* deterministic reconstruction check */
    {
        json_t *ri = json_obj_get(out, "identity");
        int id_eq = strcmp(json_get_str(ri, "room_id", ""), json_get_str(identity, "room_id", "")) == 0 &&
                    strcmp(json_get_str(ri, "task_id", ""), json_get_str(identity, "task_id", "")) == 0 &&
                    strcmp(json_get_str(ri, "thread_id", ""), json_get_str(identity, "thread_id", "")) == 0 &&
                    strcmp(json_get_str(ri, "turn_id", ""), json_get_str(identity, "turn_id", "")) == 0;
        if (!id_eq || !hrd_json_eq(json_obj_get(out, "payload"), payload)) {
            hrd_park(hrd_raise_spec("DiscussionReconstructionError",
                                    "driver task failed deterministic reconstruction"));
            json_free(out); out = NULL;
            goto fail;
        }
    }
fail:
    for (int i = 0; i < 5; i++) free(g[i]);
    free(prompt_copy);
    json_free(room); json_free(validated);
    json_free(input_context); json_free(attachments);
    return out;
}

/* ── plan_next_task ── */
static json_t *hrd_decision(const char *status, const char *reason,
                            const json_t *discussion, const char *thread_id,
                            long long continues, json_t *task) {
    json_t *o = json_object();
    json_set_hdr(o, "status", status);
    json_set_hdr(o, "reason", reason);
    if (discussion) {
        json_set(o, "discussion_event_id", json_copy(json_obj_get(discussion, "event_id")));
        json_set(o, "source_event_seq", json_copy(json_obj_get(discussion, "seq")));
        json_set_hdr(o, "thread_id", thread_id);
    } else {
        json_set(o, "discussion_event_id", json_null());
        json_set(o, "source_event_seq", json_null());
        json_set(o, "thread_id", json_null());
    }
    json_set(o, "task", task ? task : json_null()); /* adopts */
    json_set(o, "continues", json_number((double)continues));
    return o;
}

static int hrd_is_route_pending(const json_t *r) {
    return r && r->type == JSON_OBJECT &&
           strcmp(json_get_str(r, "__sentinel__", ""), "ROUTE_PENDING") == 0;
}

static json_t *hrd_plan_next_task(const json_t *work) {
    json_t *room = NULL, *context = NULL, *validated = NULL, *discussion = NULL;
    json_t *triple = NULL, *routed = NULL, *lead_round = NULL, *watermarks = NULL;
    json_t *terminals = NULL, *out = NULL;
    char *thread_id = NULL;
    long long continues = 0, max_rounds = 3, max_messages = 10, resume_round = 0;
    /* room */
    json_t *rargs = json_object();
    json_set(rargs, "room_value", json_copy(json_obj_get(work, "room_value")));
    json_set(rargs, "local_profiles", json_copy(json_obj_get(work, "local_profiles")));
    room = hrd_seam("validate_room", rargs);
    if (hrd_raise_kind(room)) { hrd_park(room); return NULL; }
    /* context: str((room_context or {}).get(key) or default) */
    {
        json_t *rc = json_obj_get(work, "room_context");
        static const char *const keys[] = { "purpose", "notes", "routing", "lead_member_id" };
        static const char *const defs[] = { "", "", "everyone", "" };
        json_t *cargs = json_object();
        for (int i = 0; i < 4; i++) {
            json_t *v = rc && rc->type == JSON_OBJECT ? json_obj_get(rc, keys[i]) : NULL;
            char *s = hrd_truth(v) ? hrd_py_str(v) : strdup(defs[i]);
            json_set_hdr(cargs, keys[i], s);
            free(s);
        }
        context = hrd_seam("validate_context", cargs);
        const char *kind = hrd_raise_kind(context);
        if (kind) {
            if (strcmp(kind, "ValueError") == 0) {
                char *msg = strdup(json_get_str(context, "message", ""));
                json_free(context); context = NULL;
                hrd_park(hrd_raise_spec("DiscussionValidationError", msg));
                free(msg);
                goto fail;
            }
            hrd_park(context); context = NULL;
            goto fail;
        }
    }
    {
        json_t *vargs = json_object();
        json_set(vargs, "events", json_copy(json_obj_get(work, "events")));
        json_set(vargs, "room", json_copy(room));
        validated = hrd_seam("validated_events", vargs);
        if (hrd_raise_kind(validated)) { hrd_park(validated); validated = NULL; goto fail; }
    }
    discussion = hrd_pending_discussion(validated);
    if (g_hrd_pending_raise) goto fail;
    if (!discussion || discussion->type == JSON_NULL) {
        json_free(discussion); discussion = NULL;
        out = hrd_decision("idle", "no_pending_user_event", NULL, NULL, 0, NULL);
        goto fail;
    }
    thread_id = hrd_py_str(json_obj_get(json_obj_get(discussion, "payload"), "thread_id"));
    {
        json_t *cargs = json_object();
        json_set(cargs, "validated", json_copy(validated));
        json_set(cargs, "discussion_event_id", json_copy(json_obj_get(discussion, "event_id")));
        json_t *r = hrd_seam("router.continue_count", cargs);
        if (hrd_raise_kind(r)) { hrd_park(r); goto fail; }
        continues = r && r->type == JSON_NUMBER ? (long long)r->num_val : 0;
        json_free(r);
    }
    /* DiscussionLimits(__post_init__ positive-int validation) */
    {
        json_t *limits = json_obj_get(work, "limits");
        json_t *mr = limits && limits->type == JSON_OBJECT ? json_obj_get(limits, "max_rounds") : NULL;
        json_t *mm = limits && limits->type == JSON_OBJECT ? json_obj_get(limits, "max_messages") : NULL;
        json_t *mrn = mr ? mr : json_number(3);
        json_t *mmn = mm ? mm : json_number(10);
        long long v;
        if (hrd_bounded_int(mrn, "hosted_rooms.discussion.max_rounds must be a positive integer", 1, 0, 0, &v) < 0) {
            if (!mr) json_free(mrn);
            if (!mm) json_free(mmn);
            goto fail;
        }
        max_rounds = v;
        if (hrd_bounded_int(mmn, "hosted_rooms.discussion.max_messages must be a positive integer", 1, 0, 0, &v) < 0) {
            if (!mr) json_free(mrn);
            if (!mm) json_free(mmn);
            goto fail;
        }
        max_messages = v;
        if (!mr) json_free(mrn);
        if (!mm) json_free(mmn);
    }
    max_rounds *= (continues + 1);
    max_messages *= (continues + 1);
    triple = hrd_thread_messages(validated, discussion);
    {
        json_t *member_messages = triple->c.items[2];
        if ((long long)member_messages->c.count >= max_messages) {
            out = hrd_decision("bounded", "max_messages", discussion, thread_id, continues, NULL);
            goto fail;
        }
    }
    {
        json_t *rrargs = json_object();
        json_set(rrargs, "room", json_copy(room));
        json_set(rrargs, "discussion", json_copy(discussion));
        json_set(rrargs, "validated", json_copy(validated));
        json_set(rrargs, "context", json_copy(context));
        routed = hrd_seam("router.routed_responders", rrargs);
        if (hrd_raise_kind(routed)) { hrd_park(routed); routed = NULL; goto fail; }
    }
    if (hrd_is_route_pending(routed)) {
        out = hrd_decision("route", "router_pending", discussion, thread_id, continues, NULL);
        goto fail;
    }
    {
        json_t *lrargs = json_object();
        json_set(lrargs, "room", json_copy(room));
        json_set(lrargs, "discussion", json_copy(discussion));
        json_set(lrargs, "validated", json_copy(validated));
        json_set(lrargs, "context", json_copy(context));
        lead_round = hrd_seam("lead_round", lrargs);
        if (hrd_raise_kind(lead_round)) { hrd_park(lead_round); lead_round = NULL; goto fail; }
    }
    if (lead_round && lead_round->type == JSON_OBJECT &&
        hrd_truth(json_obj_get(lead_round, "task"))) {
        out = hrd_decision("task", "lead_routing", discussion, thread_id, continues,
                           json_copy(json_obj_get(lead_round, "task")));
        goto fail;
    }
    /* terminals set: (round_index, member_id) pairs */
    terminals = json_array();
    {
        json_t *routing_tid = lead_round && lead_round->type == JSON_OBJECT ?
            json_obj_get(lead_round, "routing_task_id") : NULL;
        size_t ne = validated->type == JSON_ARRAY ? validated->c.count : 0;
        for (size_t i = 0; i < ne; i++) {
            json_t *ev = validated->c.items[i];
            if (!hrd_is_terminal_kind(json_get_str(ev, "kind", ""))) continue;
            json_t *payload = json_obj_get(ev, "payload");
            if (!hrd_json_eq(json_obj_get(payload, "discussion_event_id"),
                             json_obj_get(discussion, "event_id"))) continue;
            if (hrd_json_eq(json_obj_get(payload, "task_id"), routing_tid)) continue;
            json_t *pair = json_array();
            long long ri;
            json_t *riv = hrd_payload_req(payload, "round_index");
            if (!riv) goto fail;
            if (hrd_py_int(riv, &ri) < 0) goto fail;
            json_append(pair, json_number((double)ri));
            char *mid = hrd_py_str(json_obj_get(payload, "member_id"));
            json_append(pair, json_string(mid));
            free(mid);
            json_append(terminals, pair);
        }
    }
    watermarks = hrd_effective_watermarks(validated, json_obj_get(work, "initial_watermarks"));
    if (!watermarks) goto fail;
    {
        json_t *rsargs = json_object();
        json_set(rsargs, "validated", json_copy(validated));
        json_set(rsargs, "discussion_event_id", json_copy(json_obj_get(discussion, "event_id")));
        json_set(rsargs, "member_messages", json_copy(triple->c.items[2]));
        json_t *r = hrd_seam("router.resume_round", rsargs);
        if (hrd_raise_kind(r)) { hrd_park(r); goto fail; }
        resume_round = r && r->type == JSON_NUMBER ? (long long)r->num_val : 0;
        json_free(r);
    }
    for (long long round_index = 0; round_index < max_rounds; round_index++) {
        json_t *responders = NULL;
        int owned_responders = 0;
        if (round_index == 0) {
            if (lead_round && lead_round->type == JSON_OBJECT) {
                responders = json_obj_get(lead_round, "responders");
            } else if (routed && routed->type != JSON_NULL) {
                responders = routed;
            } else {
                responders = hrd_first_round_responders(discussion, room);
                if (g_hrd_pending_raise) goto fail;
                owned_responders = 1;
            }
        } else {
            responders = hrd_unaddressed(triple->c.items[1], room);
            owned_responders = 1;
        }
        if ((!responders || responders->type != JSON_ARRAY || responders->c.count == 0) &&
            round_index == resume_round) {
            if (owned_responders) json_free(responders);
            responders = json_obj_get(room, "members");
            owned_responders = 0;
        }
        json_t *rotated = hrd_rotate(responders, round_index);
        if (owned_responders) json_free(responders);
        for (size_t mi = 0; mi < rotated->c.count; mi++) {
            json_t *member = rotated->c.items[mi];
            const char *mid = json_get_str(member, "member_id", "");
            long long watermark = hrd_wm_get(watermarks, thread_id, mid);
            json_t *dargs = json_object();
            json_set(dargs, "thread_messages", json_copy(triple->c.items[0]));
            json_set(dargs, "watermark", json_number((double)watermark));
            json_set(dargs, "member", json_copy(member));
            json_t *da = hrd_seam("bounded_task_delta", dargs);
            if (hrd_raise_kind(da)) { hrd_park(da); json_free(rotated); goto fail; }
            if (!da || da->type != JSON_ARRAY || da->c.count != 2) {
                json_free(da); json_free(rotated);
                hrd_park(hrd_raise_spec("ValueError", "not enough values to unpack (expected 2, got 0)"));
                goto fail;
            }
            json_t *delta = da->c.items[0];
            json_t *attachments = da->c.items[1];
            int terminal = 0;
            for (size_t k = 0; k < terminals->c.count; k++) {
                json_t *pair = terminals->c.items[k];
                if ((long long)pair->c.items[0]->num_val == round_index &&
                    strcmp(pair->c.items[1]->str_val, mid) == 0) terminal = 1;
            }
            if (!hrd_truth(delta) || (terminal && !hrd_truth(attachments))) {
                json_free(da);
                continue;
            }
            long long seen = (long long)json_get_num(delta->c.items[delta->c.count - 1], "seq", 0);
            /* build_prompt kwargs evaluate left-to-right: addressed_by_job
             * (_scheduled_job seam) fires before the prompt seam */
            char *addressed_by_job = hrd_scheduled_job(discussion);
            if (g_hrd_pending_raise) { free(addressed_by_job); json_free(da); json_free(rotated); goto fail; }
            int reply_required = hrd_requires_direct_reply(room, discussion, member, round_index);
            json_t *assigned_by = NULL;
            if (lead_round && lead_round->type == JSON_OBJECT && round_index == 0) {
                json_t *lead = json_obj_get(lead_round, "lead");
                if (lead && !hrd_member_eq(member, lead)) assigned_by = lead;
            }
            int routed_flag = round_index == 0 && routed && routed->type == JSON_ARRAY &&
                (long long)routed->c.count < (long long)(json_obj_get(room, "members") ? json_obj_get(room, "members")->c.count : 0);
            json_t *pargs = json_object();
            json_set(pargs, "room", json_copy(room));
            json_set(pargs, "member", json_copy(member));
            json_set(pargs, "messages", json_copy(triple->c.items[0]));
            json_set(pargs, "watermark", json_number((double)watermark));
            json_set(pargs, "seen_through_seq", json_number((double)seen));
            json_set(pargs, "context", json_copy(context));
            if (addressed_by_job) json_set_hdr(pargs, "addressed_by_job", addressed_by_job);
            else json_set(pargs, "addressed_by_job", json_null());
            json_set(pargs, "reply_required", json_bool(reply_required));
            if (assigned_by) json_set(pargs, "assigned_by", json_copy(assigned_by));
            else json_set(pargs, "assigned_by", json_null());
            json_set(pargs, "routed", json_bool(routed_flag));
            json_t *prompt_r = hrd_seam("build_prompt", pargs);
            free(addressed_by_job);
            if (hrd_raise_kind(prompt_r)) { hrd_park(prompt_r); json_free(da); json_free(rotated); goto fail; }
            char *prompt = hrd_py_str(prompt_r);
            json_free(prompt_r);
            json_t *input_context = NULL;
            int freeze = json_get_bool(work, "freeze_input_context", false);
            if (freeze || hrd_truth(attachments)) {
                json_t *vic = json_object();
                json_set(vic, "watermark", json_number((double)watermark));
                json_t *seqs = json_array();
                for (size_t k = 0; k < delta->c.count; k++)
                    json_append(seqs, json_copy(json_obj_get(delta->c.items[k], "seq")));
                json_set(vic, "event_seqs", seqs);
                json_t *viargs = json_object();
                json_set(viargs, "value", vic); /* adopts */
                input_context = hrd_seam("validate_task_input", viargs);
                if (hrd_raise_kind(input_context)) {
                    hrd_park(input_context);
                    free(prompt); json_free(da); json_free(rotated);
                    goto fail;
                }
            }
            json_t *task = hrd_make_task_plan(room, discussion, member, (long long)mi,
                                              round_index, seen, prompt, input_context,
                                              json_copy(attachments), 0);
            free(prompt);
            json_free(da);
            json_free(rotated);
            if (!task) goto fail;
            out = hrd_decision("task", "member_turn", discussion, thread_id, continues, task);
            goto fail;
        }
        json_free(rotated);
        /* silent round check */
        {
            json_t *member_messages = triple->c.items[2];
            int any = 0;
            for (size_t k = 0; k < member_messages->c.count; k++) {
                json_t *payload = json_obj_get(member_messages->c.items[k], "payload");
                json_t *riv = hrd_payload_req(payload, "round_index");
                if (!riv) goto fail;
                long long ri;
                if (hrd_py_int(riv, &ri) < 0) goto fail;
                if (ri == round_index) { any = 1; break; }
            }
            if (!any) {
                out = hrd_decision("settled", "silent_round", discussion, thread_id, continues, NULL);
                goto fail;
            }
        }
        if (round_index == max_rounds - 1) {
            out = hrd_decision("bounded", "max_rounds", discussion, thread_id, continues, NULL);
            goto fail;
        }
    }
    hrd_park(hrd_raise_spec("AssertionError", "bounded Discussion loop exhausted unexpectedly"));
fail:
    json_free(room); json_free(context); json_free(validated); json_free(discussion);
    json_free(triple); json_free(routed); json_free(lead_round); json_free(watermarks);
    json_free(terminals);
    free(thread_id);
    return out;
}

/* ── public entries (fixed stub signatures) ── */
static json_t *hrd_parse_work(const char *s) {
    json_t *w = s ? json_parse(s, NULL) : NULL;
    if (!w || w->type != JSON_OBJECT) {
        json_free(w);
        hrd_park(hrd_raise_spec("RuntimeError", "invalid work JSON"));
        return NULL;
    }
    return w;
}

/* PoP: validate_member_target @ gateway/hosted_room_discussion.py:_validate_member_target */
char *validate_member_target(const char *value, const char *profile) {
    json_free(g_hrd_pending_raise); g_hrd_pending_raise = NULL;
    json_t *v = value ? json_parse(value, NULL) : NULL;
    if (!v) v = json_null();
    json_t *known = g_hrd_ctx ? json_obj_get(g_hrd_ctx, "known_profiles") : NULL;
    long long index = g_hrd_ctx ? (long long)json_get_num(g_hrd_ctx, "index", 0) : 0;
    json_t *r = hrd_validate_member_target(v, profile ? profile : "", known, index);
    json_free(v);
    if (!r) return NULL;
    return hrd_ret(r);
}

/* PoP: resolve_mentions @ gateway/hosted_room_discussion.py:resolve_mentions */
char *resolve_mentions(const char *text) {
    json_free(g_hrd_pending_raise); g_hrd_pending_raise = NULL;
    json_t *w = hrd_parse_work(text);
    if (!w) return NULL;
    json_t *r = hrd_resolve_mentions(json_obj_get(w, "texts"), json_obj_get(w, "members"),
                                     json_get_bool(w, "default_all", true));
    json_free(w);
    return hrd_ret(r);
}

/* PoP: unaddressed_member_mentions @ gateway/hosted_room_discussion.py:_unaddressed_member_mentions */
char *unaddressed_member_mentions(const char *text) {
    json_free(g_hrd_pending_raise); g_hrd_pending_raise = NULL;
    json_t *w = hrd_parse_work(text);
    if (!w) return NULL;
    json_t *r = hrd_unaddressed(json_obj_get(w, "messages"), json_obj_get(w, "room"));
    json_free(w);
    return hrd_ret(r);
}

/* PoP: member_by_id @ gateway/hosted_room_discussion.py:_member_by_id */
char *member_by_id(const char *room_json, const char *member_id) {
    json_free(g_hrd_pending_raise); g_hrd_pending_raise = NULL;
    json_t *room = room_json ? json_parse(room_json, NULL) : NULL;
    json_t *mid = member_id ? json_parse(member_id, NULL) : NULL;
    if (!room || room->type != JSON_OBJECT) {
        json_free(room); json_free(mid);
        hrd_park(hrd_raise_spec("RuntimeError", "invalid room JSON"));
        return NULL;
    }
    json_t *r = hrd_member_by_id(room, mid);
    json_free(room); json_free(mid);
    if (!r) return NULL;
    return hrd_ret(r);
}

/* PoP: derive_member_watermarks @ gateway/hosted_room_discussion.py:derive_member_watermarks */
char *derive_member_watermarks(const char *room_json) {
    json_free(g_hrd_pending_raise); g_hrd_pending_raise = NULL;
    json_t *w = hrd_parse_work(room_json);
    if (!w) return NULL;
    json_t *rargs = json_object();
    json_set(rargs, "room_value", json_copy(json_obj_get(w, "room_value")));
    json_set(rargs, "local_profiles", json_copy(json_obj_get(w, "local_profiles")));
    json_t *room = hrd_seam("validate_room", rargs);
    if (hrd_raise_kind(room)) { hrd_park(room); json_free(w); return NULL; }
    json_t *vargs = json_object();
    json_set(vargs, "events", json_copy(json_obj_get(w, "events")));
    json_set(vargs, "room", json_copy(room));
    json_t *validated = hrd_seam("validated_events", vargs);
    json_free(room);
    if (hrd_raise_kind(validated)) { hrd_park(validated); json_free(w); return NULL; }
    json_t *r = hrd_derive_watermarks(validated);
    json_free(validated); json_free(w);
    if (!r) return NULL;
    return hrd_ret(r);
}

/* PoP: make_task_plan @ gateway/hosted_room_discussion.py:_make_task_plan */
char *make_task_plan(const char *room_json) {
    json_free(g_hrd_pending_raise); g_hrd_pending_raise = NULL;
    json_t *w = hrd_parse_work(room_json);
    if (!w) return NULL;
    json_t *ic = json_obj_get(w, "input_context");
    json_t *att = json_obj_get(w, "attachments");
    json_t *r = hrd_make_task_plan(
        json_obj_get(w, "room"), json_obj_get(w, "discussion_event"), json_obj_get(w, "member"),
        (long long)json_get_num(w, "member_index", 0),
        (long long)json_get_num(w, "round_index", 0),
        (long long)json_get_num(w, "seen_through_seq", 0),
        json_get_str(w, "prompt", ""),
        ic && ic->type != JSON_NULL ? json_copy(ic) : NULL,
        att && att->type != JSON_NULL ? json_copy(att) : NULL,
        json_get_bool(w, "lead_routing", false));
    json_free(w);
    if (!r) return NULL;
    return hrd_ret(r);
}

/* PoP: plan_next_task @ gateway/hosted_room_discussion.py:plan_next_task */
char *plan_next_task(const char *room_json) {
    json_free(g_hrd_pending_raise); g_hrd_pending_raise = NULL;
    json_t *w = hrd_parse_work(room_json);
    if (!w) return NULL;
    json_t *r = hrd_plan_next_task(w);
    json_free(w);
    if (!r) return NULL;
    return hrd_ret(r);
}

/* PoP: reconstruct_task_plan @ gateway/hosted_room_discussion.py:reconstruct_task_plan */
char *reconstruct_task_plan(const char *room_json) {
    json_free(g_hrd_pending_raise); g_hrd_pending_raise = NULL;
    json_t *w = hrd_parse_work(room_json);
    if (!w) return NULL;
    json_t *r = hrd_reconstruct_task_plan(w);
    json_free(w);
    if (!r) return NULL;
    return hrd_ret(r);
}

/* PoP: plan_publication @ gateway/hosted_room_discussion.py:plan_publication */
char *plan_publication(const char *room_json) {
    json_free(g_hrd_pending_raise); g_hrd_pending_raise = NULL;
    json_t *w = hrd_parse_work(room_json);
    if (!w) return NULL;
    json_t *r = hrd_plan_publication(w);
    json_free(w);
    if (!r) return NULL;
    return hrd_ret(r);
}
