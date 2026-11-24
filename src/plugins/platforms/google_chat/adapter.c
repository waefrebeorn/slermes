/*
 * adapter.c - C11 port of upstream plugins/platforms/google_chat/adapter.py.
 *
 * 101 KB, 101 defs.  What is ported here is exactly the part of the Google
 * Chat adapter that is a pure function of values: inbound envelope extraction
 * (Workspace Add-ons / native Chat API / flat relay), the self-filter and
 * dedup gate, message-body and create-kwarg shaping, the SSRF host allow-list,
 * error-text redaction, retry classification, config/resource validation, the
 * HTTP-event bearer identity gate, setting coercion, text chunking, thread-key
 * resolution, patch/card/attachment bodies, typing-card state transitions,
 * Markdown -> Chat dialect conversion, backoff arithmetic and the small
 * routing policies.
 *
 * Seamed (never started here): every Chat REST call (spaces.messages.create/
 * patch/delete, spaces.get, spaces.members.list, media.download/upload), the
 * Pub/Sub subscriber and its supervisor loop, google-auth / service-account
 * credential loading and id-token verification, aiohttp/httplib2 transports,
 * the on-disk thread-count and bot-id caches, the i18n catalog, and the
 * plugin register() wiring.  Those are reached through the values above, not
 * re-implemented.
 *
 * Gates (4 differential harnesses, one file):
 *   w27_adapter_97bbcb_p1  inbound shaping
 *   w27_adapter_97bbcb_p2  predicates / validation
 *   w27_adapter_97bbcb_p3  outbound shaping
 *   w27_adapter_97bbcb_p4  markdown / retry math / routing
 *
 * PoP: w27_gchat_p1_extract_message_payload @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._extract_message_payload
 * PoP: w27_gchat_p1_prepare_inbound         @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._prepare_inbound
 * PoP: w27_gchat_p1_thread_body             @ plugins/platforms/google_chat/adapter.py:_thread_body
 * PoP: w27_gchat_p1_create_kwargs           @ plugins/platforms/google_chat/adapter.py:_create_kwargs
 * PoP: w27_gchat_p1_mime_for_message_type   @ plugins/platforms/google_chat/adapter.py:_mime_for_message_type
 * PoP: w27_gchat_p1_route_pubsub_event      @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._handle_pubsub_message
 * PoP: w27_gchat_p1_build_message_event     @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._build_message_event
 * PoP: w27_gchat_p2_is_google_owned_host    @ plugins/platforms/google_chat/adapter.py:_is_google_owned_host
 * PoP: w27_gchat_p2_redact_sensitive        @ plugins/platforms/google_chat/adapter.py:_redact_sensitive
 * PoP: w27_gchat_p2_retry_class             @ plugins/platforms/google_chat/adapter.py:_is_retryable_error
 * PoP: w27_gchat_p2_validate_config         @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._validate_config
 * PoP: w27_gchat_p2_plugin_validate_config  @ plugins/platforms/google_chat/adapter.py:_validate_config
 * PoP: w27_gchat_p2_is_connected            @ plugins/platforms/google_chat/adapter.py:_is_connected
 * PoP: w27_gchat_p2_space_id_ok             @ plugins/platforms/google_chat/adapter.py:_GCHAT_CHAT_ID_RE
 * PoP: w27_gchat_p2_thread_id_ok            @ plugins/platforms/google_chat/adapter.py:_GCHAT_THREAD_ID_RE
 * PoP: w27_gchat_p2_verify_http_event       @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter.verify_http_event_request
 * PoP: w27_gchat_p2_int_setting             @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._int_setting
 * PoP: w27_gchat_p2_str_setting             @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._str_setting
 * PoP: w27_gchat_p3_chunk_text              @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._chunk_text
 * PoP: w27_gchat_p3_resolve_thread_id       @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._resolve_thread_id
 * PoP: w27_gchat_p3_patch_shape             @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._patch_message
 * PoP: w27_gchat_p3_send_card_shape         @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter.send_card
 * PoP: w27_gchat_p3_attachment_body         @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._send_file
 * PoP: w27_gchat_p3_media_filename          @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._download_attachment
 * PoP: w27_gchat_p3_upload_basename         @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._send_file
 * PoP: w27_gchat_p3_note_rate_limit         @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._note_rate_limit
 * PoP: w27_gchat_p3_typing_send_action      @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter.send_typing
 * PoP: w27_gchat_p3_typing_stop_action      @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter.stop_typing
 * PoP: w27_gchat_p3_typing_consume_action   @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._consume_typing_card_with_text
 * PoP: w27_gchat_p3_chat_info_shape         @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter.get_chat_info
 * PoP: w27_gchat_p4_format_message          @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter.format_message
 * PoP: w27_gchat_p4_retry_plan              @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._call_with_retry
 * PoP: w27_gchat_p4_reconnect_delay         @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._run_supervisor
 * PoP: w27_gchat_p4_reconnect_jitter        @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._run_supervisor
 * PoP: w27_gchat_p4_bot_spaces              @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._resolve_bot_user_id
 * PoP: w27_gchat_p4_sa_source_kind          @ plugins/platforms/google_chat/adapter.py:_load_sa_credentials_from
 * PoP: w27_gchat_p4_release_unadmitted      @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._release_unadmitted
 * PoP: w27_gchat_p4_outbound_thread_incr    @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._track_outbound_thread
 * PoP: w27_gchat_p4_invalidate_identity     @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._invalidate_user_creds
 * PoP: w27_gchat_p4_loop_accepts            @ plugins/platforms/google_chat/adapter.py:GoogleChatAdapter._loop_accepts_callbacks
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "hermes_json.h"
#include "noc_w27_adapter_97bbcb_p1.h"
#include "noc_w27_adapter_97bbcb_p2.h"
#include "noc_w27_adapter_97bbcb_p3.h"
#include "noc_w27_adapter_97bbcb_p4.h"

/* Mirror of the module constants the ported paths consult. */
#define GC_MAX_TEXT_LENGTH 4000
#define GC_RATE_LIMIT_WARN_THRESHOLD 5
#define GC_RETRY_MAX_ATTEMPTS 3
#define GC_RETRY_MAX_DELAY 8.0
#define GC_RETRY_JITTER 0.3
#define GC_RECONNECT_MAX_DELAY 120.0
#define GC_RECONNECT_BASE_DELAY 2.0
#define GC_LEGACY_USER_IDENTITY "__legacy__"

/* ===================== shared value helpers ===================== */

typedef struct {
    char *p;
    size_t n, cap;
} sb_t;

static char *xstrdup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static void sb_init(sb_t *b) {
    b->cap = 128;
    b->n = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}

static void sb_reserve(sb_t *b, size_t extra) {
    size_t c;
    if (!b->p) return;
    if (b->n + extra + 1 <= b->cap) return;
    c = b->cap ? b->cap : 128;
    while (c < b->n + extra + 1) c *= 2;
    b->p = (char *)realloc(b->p, c);
    b->cap = c;
}

static void sb_addn(sb_t *b, const char *s, size_t n) {
    if (!b->p || !s || !n) return;
    sb_reserve(b, n);
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void sb_adds(sb_t *b, const char *s) {
    if (!s) return;
    sb_addn(b, s, strlen(s));
}

static void sb_addc(sb_t *b, char c) { sb_addn(b, &c, 1); }

static char *sb_take(sb_t *b) {
    char *p = b->p;
    if (!p) return xstrdup("");
    b->p = NULL;
    b->n = b->cap = 0;
    return p;
}

static void sb_free(sb_t *b) {
    free(b->p);
    b->p = NULL;
    b->n = b->cap = 0;
}

static json_t *rqo(const json_t *o, const char *k) {
    if (!o || o->type != JSON_OBJECT) return NULL;
    return json_obj_get(o, k);
}

static int is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

/* String field of an object; "" when absent or not a string. */
static const char *ostr(const json_t *o, const char *k) {
    const json_t *v = rqo(o, k);
    return is_str(v) ? v->str_val : "";
}

/* Python truthiness of a JSON value (NULL == None). */
static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

/* Decode one UTF-8 code point at s[i]; *adv receives the byte width. */
static unsigned cp_at(const char *s, size_t i, size_t *adv) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) {
        *adv = 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0 && s[i + 1]) {
        *adv = 2;
        return ((unsigned)(c & 0x1F) << 6) | ((unsigned char)s[i + 1] & 0x3F);
    }
    if ((c & 0xF0) == 0xE0 && s[i + 1] && s[i + 2]) {
        *adv = 3;
        return ((unsigned)(c & 0x0F) << 12) | (((unsigned char)s[i + 1] & 0x3F) << 6) |
               ((unsigned char)s[i + 2] & 0x3F);
    }
    if ((c & 0xF8) == 0xF0 && s[i + 1] && s[i + 2] && s[i + 3]) {
        *adv = 4;
        return ((unsigned)(c & 0x07) << 18) | (((unsigned char)s[i + 1] & 0x3F) << 12) |
               (((unsigned char)s[i + 2] & 0x3F) << 6) | ((unsigned char)s[i + 3] & 0x3F);
    }
    *adv = 1;
    return c;
}

static size_t cp_count(const char *s) {
    size_t n = 0, i = 0, a;
    while (s[i]) {
        cp_at(s, i, &a);
        i += a;
        n++;
    }
    return n;
}

/* Byte offset of code-point index `idx` (clamped to the terminator). */
static size_t cp_off(const char *s, size_t idx) {
    size_t i = 0, a;
    while (s[i] && idx) {
        cp_at(s, i, &a);
        i += a;
        idx--;
    }
    return i;
}

/* Python str.isspace() for the code points str.strip()/lstrip() remove. */
static int cp_is_space(unsigned c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v' ||
           c == 0x85 || c == 0xA0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 ||
           c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000;
}

/* str.strip() on UTF-8; caller frees. */
static char *py_strip(const char *s) {
    size_t i = 0, a, end;
    if (!s) return xstrdup("");
    end = strlen(s);
    while (s[i]) {
        unsigned c = cp_at(s, i, &a);
        if (!cp_is_space(c)) break;
        i += a;
    }
    while (end > i) {
        size_t j = end - 1;
        while (j > i && ((unsigned char)s[j] & 0xC0) == 0x80) j--;
        {
            unsigned c = cp_at(s, j, &a);
            if (!cp_is_space(c)) break;
            end = j;
        }
    }
    {
        char *out = (char *)malloc(end - i + 1);
        memcpy(out, s + i, end - i);
        out[end - i] = '\0';
        return out;
    }
}

/* str.lstrip() on UTF-8; caller frees. */
static char *py_lstrip(const char *s) {
    size_t i = 0, a;
    if (!s) return xstrdup("");
    while (s[i]) {
        unsigned c = cp_at(s, i, &a);
        if (!cp_is_space(c)) break;
        i += a;
    }
    return xstrdup(s + i);
}

/* str.strip().lower() (ASCII lower); caller frees. */
static char *py_strip_lower(const char *s) {
    char *t = py_strip(s), *p;
    for (p = t; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    }
    return t;
}

/* ASCII str.upper(); caller frees. */
static char *py_upper(const char *s) {
    char *t = xstrdup(s ? s : ""), *p;
    for (p = t; *p; p++) {
        if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
    }
    return t;
}

/* Python str() of a JSON scalar; caller frees. */
static char *py_str(const json_t *v) {
    char tmp[64];
    if (!v || v->type == JSON_NULL) return xstrdup("None");
    switch (v->type) {
        case JSON_STRING: return xstrdup(v->str_val ? v->str_val : "");
        case JSON_BOOL: return xstrdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val) {
                snprintf(tmp, sizeof(tmp), "%lld", (long long)v->num_val);
            } else {
                snprintf(tmp, sizeof(tmp), "%.17g", v->num_val);
            }
            return xstrdup(tmp);
        default: return xstrdup("");
    }
}

/* True when obj has the key at all (Python `"k" in d`). */
static int has_key(const json_t *o, const char *k) {
    return is_obj(o) && json_obj_get(o, k) != NULL;
}

/* Copy `v` when truthy, else a fresh empty string. */
static json_t *or_empty_str(const json_t *v) { return py_truthy(v) ? json_copy(v) : json_string(""); }

/* space = envelope["space"] or msg["space"] or {} */
static json_t *pick_space(const json_t *env, const json_t *msg) {
    const json_t *a = is_obj(env) ? rqo(env, "space") : NULL;
    if (py_truthy(a)) return json_copy(a);
    a = is_obj(msg) ? rqo(msg, "space") : NULL;
    if (py_truthy(a)) return json_copy(a);
    return json_object();
}

/* Membership of a JSON array of strings. */
static int arr_has_str(const json_t *arr, const char *s) {
    size_t i;
    if (!is_arr(arr)) return 0;
    for (i = 0; i < arr->c.count; i++) {
        const json_t *v = json_get(arr, i);
        if (is_str(v) && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}

/* str.split (on a single char), stripped parts, empties dropped. */
static void append_split_parts(json_t *arr, const char *s, char sep) {
    const char *p = s, *q;
    sb_t b;
    sb_init(&b);
    for (;;) {
        q = strchr(p, sep);
        sb_free(&b);
        sb_init(&b);
        if (q) sb_addn(&b, p, (size_t)(q - p));
        else sb_adds(&b, p);
        {
            char *part = py_strip(b.p ? b.p : "");
            if (part[0]) json_append(arr, json_string(part));
            free(part);
        }
        if (!q) break;
        p = q + 1;
    }
    sb_free(&b);
}

/* ===================== gate p1: inbound shaping ===================== */

/* _extract_message_payload -> {msg, space, format} or NULL. */
static json_t *extract_payload(const json_t *env) {
    json_t *out, *msg, *space;
    if (!is_obj(env)) return NULL;

    {
        const json_t *chat = rqo(env, "chat");
        const json_t *mpl = is_obj(chat) ? rqo(chat, "messagePayload") : NULL;
        if (py_truthy(mpl)) {
            const json_t *m = rqo(mpl, "message");
            const json_t *sp = rqo(mpl, "space");
            msg = (is_obj(m) && m->c.count) ? json_copy(m) : json_object();
            if (!py_truthy(sp) && is_obj(m)) sp = rqo(m, "space");
            space = py_truthy(sp) ? json_copy(sp) : json_object();
            out = json_object();
            json_set(out, "msg", msg);
            json_set(out, "space", space);
            json_set(out, "format", json_string("workspace_addons"));
            return out;
        }
    }
    {
        const json_t *m = rqo(env, "message");
        if (is_obj(m)) {
            const json_t *ty = rqo(env, "type");
            if (!(is_str(ty) && !strcmp(ty->str_val, "MESSAGE"))) return NULL;
            msg = json_copy(m);
            space = pick_space(env, m);
            out = json_object();
            json_set(out, "msg", msg);
            json_set(out, "space", space);
            json_set(out, "format", json_string("native_chat_api"));
            return out;
        }
    }
    if (has_key(env, "event_type") || has_key(env, "sender_email")) {
        const json_t *et = rqo(env, "event_type");
        char *sender_email, *sender_type, *lname, *p;
        const json_t *text_v, *dn;
        if (has_key(env, "event_type") && !(is_str(et) && !strcmp(et->str_val, "MESSAGE"))) return NULL;
        sender_email = py_strip(ostr(env, "sender_email"));
        text_v = rqo(env, "text");
        if (!py_truthy(text_v)) text_v = NULL;
        {
            json_t *st = rqo(env, "sender_type");
            char *raw = py_truthy(st) ? py_str(st) : xstrdup("HUMAN");
            char *trimmed = py_strip(raw);
            free(raw);
            sender_type = py_upper(trimmed);
            free(trimmed);
            if (strcmp(sender_type, "HUMAN") && strcmp(sender_type, "BOT")) {
                free(sender_type);
                sender_type = xstrdup("HUMAN");
            }
        }
        {
            sb_t b;
            const char *src = sender_email[0] ? sender_email : "unknown";
            sb_init(&b);
            sb_adds(&b, "users/relay-");
            for (p = (char *)src; *p; p++) {
                if (*p == '@') sb_adds(&b, "_at_");
                else if (*p == '.') sb_addc(&b, '_');
                else sb_addc(&b, *p);
            }
            lname = sb_take(&b);
        }
        dn = rqo(env, "sender_display_name");
        msg = json_object();
        json_set(msg, "name", or_empty_str(rqo(env, "message_name")));
        {
            json_t *sender = json_object();
            json_set(sender, "name", json_string(lname));
            json_set(sender, "email", json_string(sender_email));
            if (py_truthy(dn)) json_set(sender, "displayName", json_copy(dn));
            else if (sender_email[0]) json_set(sender, "displayName", json_string(sender_email));
            else json_set(sender, "displayName", json_string("Unknown"));
            json_set(sender, "type", json_string(sender_type));
            json_set(msg, "sender", sender);
        }
        if (text_v) {
            json_set(msg, "text", json_copy(text_v));
            json_set(msg, "argumentText", json_copy(text_v));
        } else {
            json_set(msg, "text", json_string(""));
            json_set(msg, "argumentText", json_string(""));
        }
        if (py_truthy(rqo(env, "thread_name"))) {
            json_t *th = json_object();
            json_set(th, "name", json_copy(rqo(env, "thread_name")));
            json_set(msg, "thread", th);
        }
        space = json_object();
        json_set(space, "name", or_empty_str(rqo(env, "space_name")));
        if (has_key(env, "space_type")) json_set(space, "spaceType", json_copy(rqo(env, "space_type")));
        else json_set(space, "spaceType", json_string("SPACE"));
        free(sender_email);
        free(sender_type);
        free(lname);
        out = json_object();
        json_set(out, "msg", msg);
        json_set(out, "space", space);
        json_set(out, "format", json_string("relay_flat"));
        return out;
    }
    return NULL;
}

json_t *w27_gchat_p1_extract_message_payload(const json_t *req) {
    json_t *r = extract_payload(rqo(req, "envelope"));
    return r ? r : json_null();
}

json_t *w27_gchat_p1_prepare_inbound(const json_t *req) {
    const json_t *env = rqo(req, "envelope");
    json_t *ex = extract_payload(env), *out;
    if (!ex) {
        out = json_object();
        json_set(out, "outcome", json_string("drop_unknown"));
        return out;
    }
    {
        json_t *msg = rqo(ex, "msg");
        json_t *space = rqo(ex, "space");
        const json_t *sender = rqo(msg, "sender");
        const char *msg_name = ostr(msg, "name");
        out = json_object();
        if (is_str(rqo(sender, "type")) && !strcmp(ostr(sender, "type"), "BOT")) {
            json_set(out, "outcome", json_string("drop_bot"));
            json_free(ex);
            return out;
        }
        if (msg_name[0] && arr_has_str(rqo(req, "dedup_seen"), msg_name)) {
            json_set(out, "outcome", json_string("drop_dup"));
            json_free(ex);
            return out;
        }
        {
            json_t *m2 = json_copy(msg), *e2 = json_copy(env);
            if (!has_key(m2, "space") && py_truthy(space)) json_set(m2, "space", json_copy(space));
            if (!has_key(e2, "space") && py_truthy(space)) json_set(e2, "space", json_copy(space));
            json_set(out, "outcome", json_string("ok"));
            json_set(out, "msg", m2);
            json_set(out, "envelope", e2);
        }
    }
    json_free(ex);
    return out;
}

json_t *w27_gchat_p1_thread_body(const json_t *req) {
    json_t *body = json_object();
    const json_t *text = rqo(req, "text");
    const json_t *tid = rqo(req, "thread_id");
    json_set(body, "text", text ? json_copy(text) : json_string(""));
    if (py_truthy(tid)) {
        json_t *th = json_object();
        json_set(th, "name", json_copy(tid));
        json_set(body, "thread", th);
    }
    return body;
}

json_t *w27_gchat_p1_create_kwargs(const json_t *req) {
    json_t *kwargs = json_object();
    const json_t *body = rqo(req, "body");
    const json_t *thread, *tname;
    json_set(kwargs, "parent", json_copy(rqo(req, "chat_id")));
    json_set(kwargs, "body", is_obj(body) ? json_copy(body) : json_object());
    thread = rqo(body, "thread");
    tname = is_obj(thread) ? rqo(thread, "name") : NULL;
    if (py_truthy(tname)) json_set(kwargs, "messageReplyOption", json_string("REPLY_MESSAGE_FALLBACK_TO_NEW_THREAD"));
    return kwargs;
}

json_t *w27_gchat_p1_mime_for_message_type(const json_t *req) {
    const char *mime = ostr(req, "mime");
    const char *out = "document";
    if (mime[0]) {
        if (!strncmp(mime, "image/", 6)) out = "photo";
        else if (!strncmp(mime, "audio/", 6)) out = "audio";
        else if (!strncmp(mime, "video/", 6)) out = "video";
    }
    return json_string(out);
}

json_t *w27_gchat_p1_route_pubsub_event(const json_t *req) {
    const char *ce = ostr(req, "ce_type");
    const json_t *env = rqo(req, "envelope");
    const json_t *bot_in = rqo(req, "bot_user_id");
    json_t *out = json_object();
    if (strstr(ce, "membership") || strstr(ce, "MEMBERSHIP")) {
        const json_t *chat = rqo(env, "chat");
        const json_t *mpl = is_obj(chat) ? rqo(chat, "membershipPayload") : NULL;
        const json_t *space, *ms, *member;
        const char *sname;
        if (!is_obj(mpl)) mpl = NULL;
        space = mpl ? rqo(mpl, "space") : NULL;
        sname = ostr(space, "name");
        json_set(out, "space_name", sname[0] ? json_string(sname) : json_null());
        json_set(out, "bot_user_id", py_truthy(bot_in) ? json_copy(bot_in) : json_null());
        if (strstr(ce, "created")) {
            ms = mpl ? rqo(mpl, "membership") : NULL;
            member = is_obj(ms) ? rqo(ms, "member") : NULL;
            if (!strcmp(ostr(member, "type"), "BOT") && !py_truthy(bot_in) && ostr(member, "name")[0]) {
                json_set(out, "bot_user_id", json_string(ostr(member, "name")));
            }
            json_set(out, "action", json_string("membership_added"));
        } else {
            json_set(out, "action", json_string("membership_removed"));
        }
        return out;
    }
    {
        char *low = py_upper(ce);
        int has_card;
        size_t i;
        for (i = 0; low[i]; i++) {
            if (low[i] >= 'A' && low[i] <= 'Z') low[i] = (char)(low[i] - 'A' + 'a');
        }
        has_card = strstr(low, "card") != NULL;
        free(low);
        json_set(out, "space_name", json_null());
        json_set(out, "bot_user_id", py_truthy(bot_in) ? json_copy(bot_in) : json_null());
        if (strstr(ce, "widget") || has_card) {
            json_set(out, "action", json_string("card_ack"));
            return out;
        }
    }
    json_set(out, "action", json_string("message"));
    return out;
}

json_t *w27_gchat_p1_build_message_event(const json_t *req) {
    const json_t *msg = rqo(req, "msg");
    const json_t *env = rqo(req, "envelope");
    json_t *out, *space, *source, *urls, *types, *cache = NULL;
    const json_t *sender, *slash, *atts, *authd;
    const char *space_name, *thread_name, *sender_name, *sender_email;
    char *space_type, *text;
    int is_dm, is_slash = 0, prev = 0, side = 0;
    const char *mtype = "text";

    if (!is_obj(msg)) return json_null();

    space = pick_space(env, msg);
    space_name = ostr(space, "name");
    {
        const json_t *tv = py_truthy(rqo(space, "type")) ? rqo(space, "type") : rqo(space, "spaceType");
        char *raw = py_truthy(tv) ? py_str(tv) : xstrdup("");
        space_type = py_upper(raw);
        free(raw);
    }
    is_dm = !strcmp(space_type, "DIRECT_MESSAGE") || !strcmp(space_type, "DM");
    free(space_type);

    {
        const json_t *th = rqo(msg, "thread");
        const json_t *tn = is_obj(th) ? rqo(th, "name") : NULL;
        thread_name = (py_truthy(tn) && is_str(tn)) ? tn->str_val : NULL;
    }
    sender = rqo(msg, "sender");
    if (!is_obj(sender)) sender = NULL;
    sender_name = ostr(sender, "name");
    sender_email = ostr(sender, "email");

    {
        const json_t *at = rqo(msg, "argumentText"), *tx = rqo(msg, "text");
        const json_t *tv = py_truthy(at) ? at : (py_truthy(tx) ? tx : NULL);
        char *raw = tv ? py_str(tv) : xstrdup("");
        text = py_strip(raw);
        free(raw);
    }

    slash = rqo(msg, "slashCommand");
    if (is_obj(slash) && slash->c.count) {
        is_slash = 1;
        {
            const json_t *cid = rqo(slash, "commandId");
            char *cid_s = py_truthy(cid) ? py_str(cid) : xstrdup("");
            if (cid_s[0] && text[0] != '/') {
                sb_t b;
                char *joined;
                sb_init(&b);
                sb_adds(&b, "/cmd_");
                sb_adds(&b, cid_s);
                sb_addc(&b, ' ');
                sb_adds(&b, text);
                joined = py_strip(b.p ? b.p : "");
                sb_free(&b);
                free(text);
                text = joined;
            }
            free(cid_s);
        }
    }

    if (thread_name && thread_name[0] && space_name[0]) {
        const json_t *counts = rqo(req, "counts");
        const json_t *cm = is_obj(counts) ? rqo(counts, space_name) : NULL;
        const json_t *cnt = is_obj(cm) ? rqo(cm, thread_name) : NULL;
        if (is_num(cnt)) prev = (int)cnt->num_val;
        else if (is_bool(cnt)) prev = cnt->bool_val ? 1 : 0;
    }

    if (is_dm) {
        side = prev > 0;
        if (thread_name && thread_name[0] && space_name[0] && side) {
            cache = json_object();
            json_set(cache, "op", json_string("set"));
            json_set(cache, "thread", json_string(thread_name));
        } else if (space_name[0]) {
            cache = json_object();
            json_set(cache, "op", json_string("del"));
        }
    } else if (thread_name && thread_name[0] && space_name[0]) {
        cache = json_object();
        json_set(cache, "op", json_string("set"));
        json_set(cache, "thread", json_string(thread_name));
    }

    urls = json_array();
    types = json_array();
    atts = rqo(msg, "attachment");
    authd = rqo(req, "authorized");
    if (is_arr(atts) && atts->c.count > 0 && is_bool(authd) && authd->bool_val) {
        const json_t *list = rqo(req, "attachments");
        size_t i;
        for (i = 0; is_arr(list) && i < list->c.count; i++) {
            const json_t *a = json_get(list, i);
            const char *path = ostr(a, "path"), *mime = ostr(a, "mime");
            if (!path[0]) continue;
            json_append(urls, json_string(path));
            json_append(types, json_string(mime[0] ? mime : "application/octet-stream"));
            if (!strcmp(mtype, "text") && !text[0]) {
                if (mime[0] && !strncmp(mime, "image/", 6)) mtype = "photo";
                else if (mime[0] && !strncmp(mime, "audio/", 6)) mtype = "audio";
                else if (mime[0] && !strncmp(mime, "video/", 6)) mtype = "video";
                else mtype = "document";
            }
        }
    }
    if (is_slash) mtype = "command";

    source = json_object();
    json_set(source, "chat_id", json_string(space_name));
    {
        const json_t *dn = rqo(space, "displayName");
        if (py_truthy(dn)) json_set(source, "chat_name", json_copy(dn));
        else if (py_truthy(rqo(space, "name"))) json_set(source, "chat_name", json_copy(rqo(space, "name")));
        else json_set(source, "chat_name", json_string(""));
    }
    json_set(source, "chat_type", json_string(is_dm ? "dm" : "group"));
    if (sender_email[0]) json_set(source, "user_id", json_string(sender_email));
    else json_set(source, "user_id", json_string(sender_name));
    {
        const json_t *dn = rqo(sender, "displayName");
        if (py_truthy(dn)) json_set(source, "user_name", json_copy(dn));
        else if (sender_email[0]) json_set(source, "user_name", json_string(sender_email));
        else json_set(source, "user_name", json_string(sender_name));
    }
    if (is_dm && !side) json_set(source, "thread_id", json_null());
    else if (thread_name && thread_name[0]) json_set(source, "thread_id", json_string(thread_name));
    else json_set(source, "thread_id", json_null());
    if (sender_name[0]) json_set(source, "user_id_alt", json_string(sender_name));
    else json_set(source, "user_id_alt", json_null());
    if (py_truthy(rqo(msg, "name"))) json_set(source, "message_id", json_copy(rqo(msg, "name")));
    else json_set(source, "message_id", json_null());

    out = json_object();
    json_set(out, "chat_type", json_string(is_dm ? "dm" : "group"));
    json_set(out, "text", json_string(text));
    json_set(out, "message_type", json_string(mtype));
    json_set(out, "session_thread_id",
             (is_dm && !side) ? json_null()
                              : ((thread_name && thread_name[0]) ? json_string(thread_name) : json_null()));
    json_set(out, "prev_thread_count", json_number((double)prev));
    json_set(out, "thread_cache", cache ? cache : json_null());
    if (sender_email[0] && space_name[0]) {
        char *e = py_strip_lower(sender_email);
        json_t *ls = json_object();
        json_set(ls, "space", json_string(space_name));
        json_set(ls, "email", json_string(e));
        json_set(out, "last_sender", ls);
        free(e);
    } else {
        json_set(out, "last_sender", json_null());
    }
    json_set(out, "media_urls", urls);
    json_set(out, "media_types", types);
    json_set(out, "source", source);
    free(text);
    json_free(space);
    return out;
}

/* ===================== gate p2: predicates / validation ===================== */

/* str.strip().lower() over a byte span; caller frees. */
static char *py_strip_lower_span(const char *s, size_t len) {
    char *tmp = (char *)malloc(len + 1), *t, *p;
    memcpy(tmp, s, len);
    tmp[len] = '\0';
    t = py_strip_lower(tmp);
    for (p = t; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    }
    free(tmp);
    return t;
}

/* _is_google_owned_host: https + a trusted Google-owned attachment host. */
static int google_owned_host(const char *url) {
    static const char *const hosts[] = {
        "googleapis.com", "chat.google.com", "drive.google.com", "docs.google.com",
        "lh3.googleusercontent.com", "lh4.googleusercontent.com", "lh5.googleusercontent.com",
        "lh6.googleusercontent.com"};
    char *host, *scheme_end, *p;
    size_t hlen, i;
    int ok = 0;

    if (!url || !url[0]) return 0;
    /* urlparse: scheme must be https, hostname is what follows "//" up to /?#:  */
    scheme_end = strstr(url, "://");
    if (!scheme_end) return 0;
    if ((size_t)(scheme_end - url) != 5) return 0;
    {
        const char *want = "https";
        size_t k;
        for (k = 0; k < 5; k++) {
            char c = url[k];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            if (c != want[k]) return 0;
        }
    }
    host = scheme_end + 3;
    /* netloc runs to the first / ? or # (urlparse url_parse semantics) */
    hlen = 0;
    while (host[hlen] && host[hlen] != '/' && host[hlen] != '?' && host[hlen] != '#') hlen++;
    if (!hlen) return 0;
    /* hostname is the netloc after the last '@' (userinfo), without the port */
    {
        size_t off = 0, k;
        for (k = 0; k < hlen; k++) {
            if (host[k] == '@') off = k + 1;
        }
        hlen -= off;
        host += off;
    }
    for (i = 0; i < hlen; i++) {
        if (host[i] == ':') {
            hlen = i;
            break;
        }
    }
    if (!hlen) return 0;
    {
        char *h = (char *)malloc(hlen + 1);
        memcpy(h, host, hlen);
        h[hlen] = '\0';
        for (p = h; *p; p++) {
            if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
        }
        for (i = 0; i < sizeof(hosts) / sizeof(hosts[0]); i++) {
            size_t bl = strlen(hosts[i]);
            if (!strcmp(h, hosts[i])) { ok = 1; break; }
            if (strlen(h) > bl && !strcmp(h + strlen(h) - bl, hosts[i]) && h[strlen(h) - bl - 1] == '.') {
                ok = 1;
                break;
            }
        }
        free(h);
    }
    return ok;
}

json_t *w27_gchat_p2_is_google_owned_host(const json_t *req) {
    return json_bool(google_owned_host(ostr(req, "url")) != 0);
}

/* --- _redact_sensitive --- */

static int is_seg_char(unsigned char c) { return c != '/' && !(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'); }

static int is_local_char(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
           c == '.' || c == '+' || c == '-';
}

static int is_label_char(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
}

/* projects/<seg>/subscriptions/<seg> and projects/<seg>/topics/<seg>. */
static int match_resource(const char *s, size_t i, const char *middle, size_t *end) {
    size_t p = i;
    if (strncmp(s + p, "projects/", 9)) return 0;
    p += 9;
    if (!is_seg_char((unsigned char)s[p])) return 0;
    while (is_seg_char((unsigned char)s[p])) p++;
    if (strncmp(s + p, middle, strlen(middle))) return 0;
    p += strlen(middle);
    if (!is_seg_char((unsigned char)s[p])) return 0;
    while (is_seg_char((unsigned char)s[p])) p++;
    *end = p;
    return 1;
}

/* <local>@<label>.iam.gserviceaccount.com */
static int match_sa_email(const char *s, size_t i, size_t *end) {
    static const char suffix[] = ".iam.gserviceaccount.com";
    size_t p = i;
    if (!is_local_char((unsigned char)s[p])) return 0;
    while (is_local_char((unsigned char)s[p])) p++;
    if (s[p] != '@') return 0;
    p++;
    if (!is_label_char((unsigned char)s[p])) return 0;
    while (is_label_char((unsigned char)s[p])) p++;
    if (strncmp(s + p, suffix, sizeof(suffix) - 1)) return 0;
    *end = p + sizeof(suffix) - 1;
    return 1;
}

json_t *w27_gchat_p2_redact_sensitive(const json_t *req) {
    const char *text = ostr(req, "text");
    char *cur = xstrdup(text);
    int pass;
    for (pass = 0; pass < 3; pass++) {
        const char *middle = pass == 0 ? "/subscriptions/" : (pass == 1 ? "/topics/" : NULL);
        const char *repl = pass == 0 ? "projects/<redacted>/subscriptions/<redacted>"
                                     : (pass == 1 ? "projects/<redacted>/topics/<redacted>"
                                                  : "<sa>@<project>.iam.gserviceaccount.com");
        sb_t b;
        size_t i = 0, n = strlen(cur);
        int changed = 0;
        sb_init(&b);
        while (i < n) {
            size_t end = 0;
            int hit = middle ? match_resource(cur, i, middle, &end) : match_sa_email(cur, i, &end);
            if (hit) {
                sb_adds(&b, repl);
                i = end;
                changed = 1;
            } else {
                sb_addc(&b, cur[i]);
                i++;
            }
        }
        free(cur);
        cur = sb_take(&b);
        if (!changed) {
            /* keep scanning remaining passes: earlier passes may still matter */
        }
    }
    {
        json_t *out = json_string(cur);
        free(cur);
        return out;
    }
}

/* _http_status + _is_retryable_error. */
json_t *w27_gchat_p2_retry_class(const json_t *req) {
    const json_t *st = rqo(req, "status");
    json_t *out = json_object();
    int retryable = 0;
    if (is_num(st) && st->num_val == (double)(long long)st->num_val) {
        long long code = (long long)st->num_val;
        retryable = code == 429 || code == 500 || code == 502 || code == 503 || code == 504;
        json_set(out, "status", json_copy(st));
    } else {
        char *low = py_strip_lower(ostr(req, "message"));
        const char *phrases[] = {"timeout", "timed out", "broken pipe", "remote disconnected"};
        size_t i;
        for (i = 0; i < 4; i++) {
            if (strstr(low, phrases[i])) {
                retryable = 1;
                break;
            }
        }
        if (!retryable && strstr(low, "connection") &&
            (strstr(low, "reset") || strstr(low, "refused") || strstr(low, "aborted"))) {
            retryable = 1;
        }
        free(low);
        json_set(out, "status", json_null());
    }
    json_set(out, "retryable", json_bool(retryable != 0));
    return out;
}

/* ^projects/([^/]+)/subscriptions/([^/]+)$ -> project + subscription. */
static int subscription_path(const char *s, char **project, char **sub) {
    const char *p = s, *seg1, *seg2;
    size_t l1, l2;
    if (strncmp(p, "projects/", 9)) return 0;
    p += 9;
    seg1 = p;
    while (*p && *p != '/') p++;
    l1 = (size_t)(p - seg1);
    if (!l1) return 0;
    if (strncmp(p, "/subscriptions/", 15)) return 0;
    p += 15;
    seg2 = p;
    while (*p && *p != '/') p++;
    l2 = (size_t)(p - seg2);
    if (!l2) return 0;
    if (*p) return 0;
    *project = (char *)malloc(l1 + 1);
    memcpy(*project, seg1, l1);
    (*project)[l1] = '\0';
    *sub = (char *)malloc(l2 + 1);
    memcpy(*sub, seg2, l2);
    (*sub)[l2] = '\0';
    return 1;
}

json_t *w27_gchat_p2_validate_config(const json_t *req) {
    const json_t *extra = rqo(req, "extra");
    char *project_id = py_strip(ostr(extra, "project_id"));
    char *subscription = py_strip(ostr(extra, "subscription_name"));
    char *http_url = py_strip(ostr(extra, "http_events_url"));
    json_t *out = json_object();

    if (subscription[0]) {
        char *sp = NULL, *ss = NULL;
        if (!subscription_path(subscription, &sp, &ss)) {
            json_set(out, "ok", json_bool(0));
            json_set(out, "error", json_string(
                "GOOGLE_CHAT_SUBSCRIPTION_NAME must match 'projects/<project>/subscriptions/<sub>'."));
        } else if (project_id[0] && strcmp(sp, project_id)) {
            json_set(out, "ok", json_bool(0));
            json_set(out, "error", json_string(
                "project_id in GOOGLE_CHAT_PROJECT_ID does not match the project embedded in GOOGLE_CHAT_SUBSCRIPTION_NAME."));
        } else {
            json_set(out, "ok", json_bool(1));
            json_set(out, "project_id", json_string(project_id[0] ? project_id : sp));
            json_set(out, "subscription", json_string(subscription));
        }
        free(sp);
        free(ss);
    } else if (http_url[0]) {
        json_set(out, "ok", json_bool(1));
        json_set(out, "project_id", json_string(project_id));
        json_set(out, "subscription", json_null());
    } else if (!project_id[0]) {
        json_set(out, "ok", json_bool(0));
        json_set(out, "error", json_string("GOOGLE_CHAT_PROJECT_ID (or GOOGLE_CLOUD_PROJECT) is not set."));
    } else {
        json_set(out, "ok", json_bool(0));
        json_set(out, "error", json_string(
            "GOOGLE_CHAT_SUBSCRIPTION_NAME (or GOOGLE_CHAT_SUBSCRIPTION) is not set. "
            "Set GOOGLE_CHAT_HTTP_EVENTS_URL for HTTP callback mode."));
    }
    free(project_id);
    free(subscription);
    free(http_url);
    return out;
}

json_t *w27_gchat_p2_plugin_validate_config(const json_t *req) {
    const json_t *extra = rqo(req, "extra");
    int ok = py_truthy(rqo(extra, "http_events_url")) ||
             (py_truthy(rqo(extra, "project_id")) && py_truthy(rqo(extra, "subscription_name")));
    return json_bool(ok != 0);
}

json_t *w27_gchat_p2_is_connected(const json_t *req) {
    json_t *cfg = json_object();
    json_t *res;
    const json_t *extra = rqo(req, "extra");
    int enabled = py_truthy(rqo(req, "enabled"));
    if (!enabled) return json_bool(0);
    json_set(cfg, "extra", is_obj(extra) ? json_copy(extra) : json_object());
    res = w27_gchat_p2_plugin_validate_config(cfg);
    json_free(cfg);
    return res;
}

/* ^(?:spaces|users)/[A-Za-z0-9_-]+$ */
json_t *w27_gchat_p2_space_id_ok(const json_t *req) {
    const char *s = ostr(req, "chat_id");
    const char *p;
    if (!strncmp(s, "spaces/", 7)) p = s + 7;
    else if (!strncmp(s, "users/", 6)) p = s + 6;
    else return json_bool(0);
    if (!*p) return json_bool(0);
    while (*p) {
        unsigned char c = (unsigned char)*p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
              c == '-'))
            return json_bool(0);
        p++;
    }
    return json_bool(1);
}

/* ^spaces/[A-Za-z0-9_-]+/threads/[A-Za-z0-9_-]+$ */
json_t *w27_gchat_p2_thread_id_ok(const json_t *req) {
    const char *s = ostr(req, "thread_id");
    const char *p = s;
    int part;
    if (strncmp(p, "spaces/", 7)) return json_bool(0);
    p += 7;
    for (part = 0; part < 2; part++) {
        const char *start = p;
        while (*p) {
            unsigned char c = (unsigned char)*p;
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                  c == '-'))
                break;
            p++;
        }
        if (p == start) return json_bool(0);
        if (part == 0) {
            if (strncmp(p, "/threads/", 9)) return json_bool(0);
            p += 9;
        }
    }
    return json_bool(*p == '\0');
}

json_t *w27_gchat_p2_verify_http_event(const json_t *req) {
    char *audience = py_strip(ostr(req, "audience"));
    char *sa = py_strip(ostr(req, "sa_emails"));
    const char *header = ostr(req, "auth_header");
    json_t *out = json_object();
    const char *reason = NULL;

    if (!audience[0] || !sa[0]) {
        reason = "google_chat_http_events_not_configured";
    } else if (!(strncmp(header, "Bearer ", 7) == 0 && header[7])) {
        reason = "missing_google_bearer";
    } else if (py_truthy(rqo(req, "claims_error"))) {
        reason = "invalid_google_bearer";
    } else if (!is_obj(rqo(req, "claims"))) {
        reason = "invalid_google_bearer";
    } else {
        const json_t *claims = rqo(req, "claims");
        const json_t *em = rqo(claims, "email");
        char *email = py_strip_lower(py_truthy(em) ? py_str(em) : "");
        int found = 0;
        const char *p = sa;
        while (*p) {
            const char *comma = strchr(p, ',');
            size_t len = comma ? (size_t)(comma - p) : strlen(p);
            char *part = py_strip_lower_span(p, len);
            if (part[0] && !strcmp(part, email)) found = 1;
            free(part);
            if (!comma) break;
            p = comma + 1;
        }
        free(email);
        if (!found) reason = "unexpected_google_bearer_identity";
    }
    json_set(out, "ok", json_bool(reason == NULL));
    json_set(out, "reason", json_string(reason ? reason : ""));
    free(audience);
    free(sa);
    return out;
}

/* Python int(): optional whitespace, sign, digits with single '_' separators. */
static long long py_int_parse(const char *s, int *ok) {
    const char *p = s;
    int neg = 0, digits = 0;
    long long v = 0;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
    if (*p == '+' || *p == '-') {
        neg = (*p == '-');
        p++;
    }
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            v = v * 10 + (*p - '0');
            digits++;
            p++;
        } else if (*p == '_' && digits && p[1] >= '0' && p[1] <= '9') {
            p++;
        } else {
            break;
        }
    }
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
    if (!digits || *p) {
        *ok = 0;
        return 0;
    }
    *ok = 1;
    return neg ? -v : v;
}

json_t *w27_gchat_p2_int_setting(const json_t *req) {
    const json_t *extra = rqo(req, "extra");
    const char *key = ostr(req, "key");
    const json_t *v = rqo(extra, key);
    const json_t *dflt = rqo(req, "default");
    long long fallback = is_num(dflt) ? (long long)dflt->num_val : 0;

    if (!py_truthy(v)) {
        v = rqo(req, "env_value");
        if (!py_truthy(v)) v = NULL;
    }
    if (!v || is_obj(v) || is_arr(v)) return json_number((double)fallback);
    if (is_num(v)) return json_number((double)(long long)v->num_val);
    if (is_bool(v)) return json_number(v->bool_val ? 1.0 : 0.0);
    if (is_str(v)) {
        int ok = 0;
        long long parsed = py_int_parse(v->str_val, &ok);
        return json_number(ok ? (double)parsed : (double)fallback);
    }
    return json_number((double)fallback);
}

json_t *w27_gchat_p2_str_setting(const json_t *req) {
    const json_t *extra = rqo(req, "extra");
    const char *key = ostr(req, "key");
    const json_t *v = rqo(extra, key);
    const json_t *fb = rqo(req, "fallback");
    char *picked = NULL, *out;

    if (py_truthy(v)) picked = py_str(v);
    else if (py_truthy(rqo(req, "env_value"))) picked = py_str(rqo(req, "env_value"));
    else if (fb) picked = py_str(fb);
    else picked = xstrdup("");
    out = py_strip(picked);
    free(picked);
    {
        json_t *res = json_string(out);
        free(out);
        return res;
    }
}

/* ===================== gate p3: outbound shaping ===================== */

static char *substr(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

json_t *w27_gchat_p3_chunk_text(const json_t *req) {
    char *remaining = xstrdup(ostr(req, "text"));
    json_t *arr = json_array();
    while (remaining[0]) {
        size_t n = cp_count(remaining);
        size_t limit, cut;
        long cut_cp = -1;
        size_t i, idx;
        if (n <= GC_MAX_TEXT_LENGTH) {
            json_append(arr, json_string(remaining));
            break;
        }
        limit = cp_off(remaining, GC_MAX_TEXT_LENGTH);
        i = 0;
        idx = 0;
        while (i < limit) {
            size_t a;
            unsigned c = cp_at(remaining, i, &a);
            if (c == '\n') cut_cp = (long)idx;
            i += a;
            idx++;
        }
        cut = (cut_cp < GC_MAX_TEXT_LENGTH / 2) ? limit : cp_off(remaining, (size_t)cut_cp);
        if (cut == 0) break; /* defensive: never spin */
        {
            char *chunk = substr(remaining, cut);
            json_append(arr, json_string(chunk));
            free(chunk);
        }
        {
            char *rest = py_lstrip(remaining + cut);
            free(remaining);
            remaining = rest;
        }
    }
    free(remaining);
    return arr;
}

json_t *w27_gchat_p3_resolve_thread_id(const json_t *req) {
    const json_t *md = rqo(req, "metadata");
    const json_t *reply_to = rqo(req, "reply_to");
    const json_t *chat_id = rqo(req, "chat_id");
    static const char *const keys[] = {"thread_id", "thread_name", "thread_ts"};
    size_t i;

    if (py_truthy(md)) {
        for (i = 0; i < 3; i++) {
            const json_t *v = rqo(md, keys[i]);
            if (py_truthy(v)) {
                char *s = py_str(v);
                json_t *out = json_string(s);
                free(s);
                return out;
            }
        }
    }
    if (py_truthy(reply_to) && is_str(reply_to) && strstr(reply_to->str_val, "/threads/") &&
        !strstr(reply_to->str_val, "/messages/")) {
        return json_copy(reply_to);
    }
    if (py_truthy(md) && py_truthy(rqo(md, "job_id"))) return json_null();
    if (py_truthy(chat_id) && is_str(chat_id)) {
        const json_t *cache = rqo(req, "last_inbound_thread");
        const json_t *v = is_obj(cache) ? rqo(cache, chat_id->str_val) : NULL;
        if (py_truthy(v)) return json_copy(v);
    }
    return json_null();
}

json_t *w27_gchat_p3_patch_shape(const json_t *req) {
    const json_t *body = rqo(req, "body");
    json_t *out = json_object(), *patch_body = json_object();
    sb_t mask;
    size_t i;
    int first = 1;

    sb_init(&mask);
    if (has_key(body, "text")) {
        sb_adds(&mask, "text");
        first = 0;
    }
    if (has_key(body, "cardsV2")) {
        if (!first) sb_addc(&mask, ',');
        sb_adds(&mask, "cardsV2");
        first = 0;
    }
    if (first) sb_adds(&mask, "text");
    for (i = 0; is_obj(body) && i < body->c.count; i++) {
        const char *k = json_object_get_key_at(body, i);
        if (!k || !strcmp(k, "thread")) continue;
        json_set(patch_body, k, json_copy(json_object_get_at(body, i)));
    }
    json_set(out, "update_mask", json_string(mask.p ? mask.p : "text"));
    json_set(out, "patch_body", patch_body);
    sb_free(&mask);
    return out;
}

json_t *w27_gchat_p3_send_card_shape(const json_t *req) {
    const json_t *card = rqo(req, "card");
    json_t *body = json_object(), *cards = json_array(), *out = json_object();
    json_t *tid;
    json_append(cards, card ? json_copy(card) : json_null());
    json_set(body, "cardsV2", cards);
    tid = w27_gchat_p3_resolve_thread_id(req);
    if (py_truthy(tid)) {
        json_t *th = json_object();
        json_set(th, "name", json_copy(tid));
        json_set(body, "thread", th);
    }
    json_set(out, "body", body);
    json_set(out, "thread_id", tid);
    return out;
}

json_t *w27_gchat_p3_attachment_body(const json_t *req) {
    const json_t *ref = rqo(req, "attachment_ref");
    const json_t *caption = rqo(req, "caption");
    const json_t *tid = rqo(req, "thread_id");
    json_t *body = json_object(), *atts = json_array(), *att = json_object(), *out = json_object();
    json_t *kwargs;

    json_set(att, "attachmentDataRef", ref ? json_copy(ref) : json_null());
    json_append(atts, att);
    json_set(body, "attachment", atts);
    if (py_truthy(caption)) json_set(body, "text", json_copy(caption));
    if (py_truthy(tid)) {
        json_t *th = json_object();
        json_set(th, "name", json_copy(tid));
        json_set(body, "thread", th);
    }
    {
        json_t *kreq = json_object();
        json_set(kreq, "chat_id", json_copy(rqo(req, "chat_id")));
        json_set(kreq, "body", json_copy(body));
        kwargs = w27_gchat_p1_create_kwargs(kreq);
        json_free(kreq);
    }
    json_set(out, "body", body);
    json_set(out, "create_kwargs", kwargs);
    return out;
}

json_t *w27_gchat_p3_media_filename(const json_t *req) {
    const char *name = ostr(req, "name");
    char *filename, *ext, *dot;
    json_t *out = json_object();

    if (name[0]) {
        const char *slash = strrchr(name, '/');
        filename = xstrdup(slash ? slash + 1 : name);
    } else {
        filename = xstrdup("attachment");
    }
    dot = strrchr(filename, '.');
    if (dot) {
        char *p;
        ext = substr(dot, strlen(dot));
        for (p = ext; *p; p++) {
            if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
        }
    } else {
        ext = xstrdup("");
    }
    json_set(out, "filename", json_string(filename));
    json_set(out, "ext", json_string(ext));
    free(filename);
    free(ext);
    return out;
}

/* ntpath.basename: last component after / or \, drive-independent. */
static char *py_basename(const char *path) {
    const char *p = path ? path : "";
    const char *last = p;
    const char *q;
    for (q = p; *q; q++) {
        if (*q == '/' || *q == '\\') last = q + 1;
    }
    return xstrdup(last);
}

json_t *w27_gchat_p3_upload_basename(const json_t *req) {
    const json_t *ov = rqo(req, "override");
    char *fn;
    if (py_truthy(ov)) {
        fn = py_str(ov);
    } else {
        char *base = py_basename(ostr(req, "path"));
        if (base[0]) fn = base;
        else {
            free(base);
            fn = xstrdup("upload.bin");
        }
    }
    {
        json_t *out = json_string(fn);
        free(fn);
        return out;
    }
}

json_t *w27_gchat_p3_note_rate_limit(const json_t *req) {
    const json_t *hits = rqo(req, "hits");
    long long count = (is_num(hits) ? (long long)hits->num_val : 0) + 1;
    json_t *out = json_object();
    json_set(out, "count", json_number((double)count));
    json_set(out, "warn", json_bool(count >= GC_RATE_LIMIT_WARN_THRESHOLD));
    return out;
}

static const char *state_of(const json_t *req) { return ostr(req, "state"); }

json_t *w27_gchat_p3_typing_send_action(const json_t *req) {
    const char *s = state_of(req);
    if (!strcmp(s, "card") || !strcmp(s, "sentinel")) return json_string("already");
    if (!strcmp(s, "inflight")) return json_string("wait");
    return json_string("create");
}

json_t *w27_gchat_p3_typing_stop_action(const json_t *req) {
    return json_string(!strcmp(state_of(req), "sentinel") ? "pop" : "noop");
}

json_t *w27_gchat_p3_typing_consume_action(const json_t *req) {
    const char *s = state_of(req);
    const json_t *err = rqo(req, "error_status");
    if (!strcmp(s, "absent") || !strcmp(s, "sentinel")) return json_string("none");
    if (is_num(err)) {
        if ((long long)err->num_val == 404) return json_string("none");
        return json_string("raise");
    }
    return json_string("patched");
}

json_t *w27_gchat_p3_chat_info_shape(const json_t *req) {
    const char *chat_id = ostr(req, "chat_id");
    const json_t *info = rqo(req, "info");
    json_t *out = json_object();
    if (!is_obj(info)) {
        json_set(out, "name", json_string(chat_id));
        json_set(out, "type", json_string("group"));
        json_set(out, "chat_id", json_string(chat_id));
        return out;
    }
    {
        const json_t *tv = py_truthy(rqo(info, "spaceType")) ? rqo(info, "spaceType") : rqo(info, "type");
        char *raw = py_truthy(tv) ? py_str(tv) : xstrdup("");
        char *up = py_upper(raw);
        int is_dm = !strcmp(up, "DIRECT_MESSAGE") || !strcmp(up, "DM");
        free(up);
        free(raw);
        {
            const json_t *dn = rqo(info, "displayName");
            if (py_truthy(dn)) json_set(out, "name", json_copy(dn));
            else json_set(out, "name", json_string(chat_id));
        }
        json_set(out, "type", json_string(is_dm ? "dm" : "group"));
    }
    json_set(out, "chat_id", json_string(chat_id));
    return out;
}

/* ===================== gate p4: markdown / retry math / routing ===================== */

/* ---- format_message: Markdown -> Google Chat dialect ---- */

/*
 * Upstream shields rewritten spans behind placeholder keys "\x00GC<n>\x00".
 * A C string cannot carry a NUL, so the sentinel here is "\x01GC<n>\x01":
 * byte-for-byte the same algorithm, different marker.  The marker bytes are
 * only observable if a placeholder's *value* embeds another placeholder's key
 * (nested emphasis inside a link label, e.g. "[**b**](u)"), and then both
 * implementations leak their own marker; no fixture case relies on it.
 */

typedef struct {
    char **v;
    size_t n, cap;
} phl_t;

static void ph_add(phl_t *l, const char *val, size_t vlen, sb_t *out) {
    char key[40];
    if (l->n >= l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->v = (char **)realloc(l->v, l->cap * sizeof(char *));
    }
    l->v[l->n] = substr(val, vlen);
    snprintf(key, sizeof(key), "\x01GC%u\x01", (unsigned)l->n);
    l->n++;
    sb_adds(out, key);
}

static void ph_free(phl_t *l) {
    size_t i;
    for (i = 0; i < l->n; i++) free(l->v[i]);
    free(l->v);
    l->v = NULL;
    l->n = l->cap = 0;
}

/* re.sub(r"(```(?:[^\n]*\n)?[\s\S]*?```)") */
static void md_fences(const char *s, sb_t *out, phl_t *ph) {
    size_t i = 0, len = strlen(s);
    while (i < len) {
        if (i + 3 <= len && s[i] == '`' && s[i + 1] == '`' && s[i + 2] == '`') {
            const char *nl = strchr(s + i + 3, '\n');
            size_t end = (size_t)-1;
            if (nl) {
                const char *close = strstr(nl + 1, "```");
                if (close) end = (size_t)(close - s) + 3;
            }
            if (end == (size_t)-1) {
                const char *close = strstr(s + i + 3, "```");
                if (close) end = (size_t)(close - s) + 3;
            }
            if (end != (size_t)-1) {
                ph_add(ph, s + i, end - i, out);
                i = end;
                continue;
            }
            ph_add(ph, s + i, 3, out); /* literal ``` with no close */
            i += 3;
            continue;
        }
        sb_addc(out, s[i]);
        i++;
    }
}

/* re.sub(r"(`[^`]+`)") */
static void md_inline_code(const char *s, sb_t *out, phl_t *ph) {
    size_t i = 0, len = strlen(s);
    while (i < len) {
        if (s[i] == '`') {
            size_t j = i + 1;
            while (s[j] && s[j] != '`') j++;
            if (s[j] == '`' && j > i + 1) {
                ph_add(ph, s + i, j + 1 - i, out);
                i = j + 1;
                continue;
            }
        }
        sb_addc(out, s[i]);
        i++;
    }
}

/* re.sub(r"^#{1,6}\s+(.+)$", ... , flags=MULTILINE) */
static void md_headers(const char *s, sb_t *out, phl_t *ph) {
    size_t i = 0, len = strlen(s);
    for (;;) {
        int at_start = (i == 0) || (i > 0 && s[i - 1] == '\n');
        if (at_start) {
            size_t h = i;
            int nh = 0;
            while (s[h] == '#' && nh < 6) {
                h++;
                nh++;
            }
            if (nh >= 1) {
                size_t w = h;
                int nws = 0;
                while (s[w]) {
                    size_t a;
                    unsigned c = cp_at(s, w, &a);
                    if (!cp_is_space(c)) break;
                    w += a;
                    nws++;
                }
                if (nws > 0) {
                    size_t e = w;
                    while (s[e] && s[e] != '\n') e++;
                    if (e > w) {
                        char *title = py_strip(substr(s + w, e - w));
                        sb_t v;
                        sb_init(&v);
                        sb_addc(&v, '*');
                        sb_adds(&v, title);
                        sb_addc(&v, '*');
                        ph_add(ph, v.p ? v.p : "", v.n, out);
                        sb_free(&v);
                        free(title);
                        i = e;
                        continue;
                    }
                }
            }
        }
        if (i >= len) break;
        sb_addc(out, s[i]);
        i++;
    }
}

/* re.sub(r"<open>(.+?)<close>") with '.' never crossing a newline. */
static void md_emphasis(const char *s, sb_t *out, phl_t *ph, const char *open, const char *pre,
                        const char *post) {
    size_t i = 0, len = strlen(s), ol = strlen(open);
    while (i < len) {
        if (i + ol <= len && !strncmp(s + i, open, ol)) {
            const char *p = strstr(s + i + ol, open);
            if (p && p > s + i + ol) {
                const char *q;
                int has_nl = 0;
                for (q = s + i + ol; q < p; q++) {
                    if (*q == '\n') {
                        has_nl = 1;
                        break;
                    }
                }
                if (!has_nl) {
                    sb_t v;
                    sb_init(&v);
                    sb_adds(&v, pre);
                    sb_addn(&v, s + i + ol, (size_t)(p - (s + i + ol)));
                    sb_adds(&v, post);
                    ph_add(ph, v.p ? v.p : "", v.n, out);
                    sb_free(&v);
                    i = (size_t)(p - s) + ol;
                    continue;
                }
            }
        }
        sb_addc(out, s[i]);
        i++;
    }
}

/* re.sub(r"\[([^\]]+)\]\(([^)]+)\)", "<url|text>") */
static void md_links(const char *s, sb_t *out, phl_t *ph) {
    size_t i = 0, len = strlen(s);
    while (i < len) {
        if (s[i] == '[') {
            const char *rb = strchr(s + i + 1, ']');
            if (rb && rb > s + i + 1 && rb[1] == '(') {
                const char *rp = strchr(rb + 2, ')');
                if (rp && rp > rb + 2) {
                    sb_t v;
                    sb_init(&v);
                    sb_addc(&v, '<');
                    sb_addn(&v, rb + 2, (size_t)(rp - (rb + 2)));
                    sb_addc(&v, '|');
                    sb_addn(&v, s + i + 1, (size_t)(rb - (s + i + 1)));
                    sb_addc(&v, '>');
                    ph_add(ph, v.p ? v.p : "", v.n, out);
                    sb_free(&v);
                    i = (size_t)(rp - s) + 1;
                    continue;
                }
            }
        }
        sb_addc(out, s[i]);
        i++;
    }
}

/* _INVISIBLE_RE: ZWSP/ZWNJ/ZWJ, LTR/RTL marks, word joiner, BOM, VS1-256. */
static int cp_invisible(unsigned c) {
    return c == 0x200B || c == 0x200C || c == 0x200D || c == 0x200E || c == 0x200F || c == 0x2060 ||
           c == 0xFEFF || (c >= 0xFE00 && c <= 0xFE0F) || (c >= 0xE0100 && c <= 0xE01EF);
}

static char *md_strip_invisible(const char *s) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        size_t a;
        unsigned c = cp_at(s, i, &a);
        if (!cp_invisible(c)) sb_addn(&b, s + i, a);
        i += a;
    }
    return sb_take(&b);
}

/* re.sub(r"  +", " ") */
static char *md_collapse_spaces(const char *s) {
    sb_t b;
    size_t i = 0;
    sb_init(&b);
    while (s[i]) {
        size_t j = i;
        if (s[i] == ' ') {
            while (s[j] == ' ') j++;
            if (j - i >= 2) {
                sb_addc(&b, ' ');
                i = j;
                continue;
            }
        }
        sb_addc(&b, s[i]);
        i++;
    }
    return sb_take(&b);
}

static char *replace_all(const char *hay, const char *key, const char *val) {
    sb_t b;
    size_t i = 0, klen = strlen(key);
    sb_init(&b);
    while (hay[i]) {
        if (klen && !strncmp(hay + i, key, klen)) {
            sb_adds(&b, val);
            i += klen;
        } else {
            sb_addc(&b, hay[i]);
            i++;
        }
    }
    return sb_take(&b);
}

json_t *w27_gchat_p4_format_message(const json_t *req) {
    const char *content = ostr(req, "content");
    phl_t ph;
    char *text, *tmp;
    sb_t b;
    size_t k;

    if (!content[0]) return json_string("");
    ph.v = NULL;
    ph.n = ph.cap = 0;

    sb_init(&b);
    md_fences(content, &b, &ph);
    text = sb_take(&b);
    sb_init(&b);
    md_inline_code(text, &b, &ph);
    free(text);
    text = sb_take(&b);
    sb_init(&b);
    md_headers(text, &b, &ph);
    free(text);
    text = sb_take(&b);
    sb_init(&b);
    md_emphasis(text, &b, &ph, "***", "*_", "_*");
    free(text);
    text = sb_take(&b);
    sb_init(&b);
    md_emphasis(text, &b, &ph, "**", "*", "*");
    free(text);
    text = sb_take(&b);
    sb_init(&b);
    md_links(text, &b, &ph);
    free(text);

    tmp = md_strip_invisible(b.p ? b.p : "");
    sb_free(&b);
    text = md_collapse_spaces(tmp);
    free(tmp);

    for (k = 0; k < ph.n; k++) {
        char key[40];
        snprintf(key, sizeof(key), "\x01GC%u\x01", (unsigned)k);
        tmp = replace_all(text, key, ph.v[k]);
        free(text);
        text = tmp;
    }
    ph_free(&ph);
    {
        json_t *out = json_string(text);
        free(text);
        return out;
    }
}

json_t *w27_gchat_p4_retry_plan(const json_t *req) {
    const json_t *randoms = rqo(req, "randoms");
    const json_t *fa = rqo(req, "failures");
    int failures = is_num(fa) ? (int)fa->num_val : 0;
    double delay = 1.0;
    json_t *waits = json_array(), *delays = json_array(), *out = json_object();
    int attempt, exhausted = 0;

    for (attempt = 1; attempt <= GC_RETRY_MAX_ATTEMPTS; attempt++) {
        double r, wait, cap;
        if (attempt > failures) break;
        if (attempt >= GC_RETRY_MAX_ATTEMPTS) {
            exhausted = 1;
            break;
        }
        r = (is_arr(randoms) && (size_t)(attempt - 1) < randoms->c.count &&
             is_num(json_get(randoms, (size_t)(attempt - 1))))
                ? json_get(randoms, (size_t)(attempt - 1))->num_val
                : 0.0;
        wait = delay + delay * GC_RETRY_JITTER * r;
        cap = GC_RETRY_MAX_DELAY + GC_RETRY_JITTER;
        if (wait > cap) wait = cap;
        json_append(waits, json_number(wait));
        delay = delay * 2.0;
        if (delay > GC_RETRY_MAX_DELAY) delay = GC_RETRY_MAX_DELAY;
        json_append(delays, json_number(delay));
    }
    json_set(out, "waits", waits);
    json_set(out, "delays", delays);
    json_set(out, "exhausted", json_bool(exhausted != 0));
    return out;
}

json_t *w27_gchat_p4_reconnect_delay(const json_t *req) {
    const json_t *a = rqo(req, "attempt");
    int attempt = is_num(a) ? (int)a->num_val : 0;
    double delay = GC_RECONNECT_MAX_DELAY;
    if (attempt >= 1 && attempt <= 20) {
        double exp = 1.0;
        int k;
        for (k = 1; k < attempt; k++) exp *= 2.0;
        delay = GC_RECONNECT_BASE_DELAY * exp;
        if (delay > GC_RECONNECT_MAX_DELAY) delay = GC_RECONNECT_MAX_DELAY;
    }
    return json_number(delay);
}

json_t *w27_gchat_p4_reconnect_jitter(const json_t *req) {
    const json_t *a = rqo(req, "attempt");
    const json_t *r = rqo(req, "rand");
    int attempt = is_num(a) ? (int)a->num_val : 0;
    double rand = is_num(r) ? r->num_val : 0.0;
    double delay = GC_RECONNECT_MAX_DELAY;
    if (attempt >= 1 && attempt <= 20) {
        double exp = 1.0;
        int k;
        for (k = 1; k < attempt; k++) exp *= 2.0;
        delay = GC_RECONNECT_BASE_DELAY * exp;
        if (delay > GC_RECONNECT_MAX_DELAY) delay = GC_RECONNECT_MAX_DELAY;
    }
    return json_number(delay * rand);
}

json_t *w27_gchat_p4_bot_spaces(const json_t *req) {
    json_t *arr = json_array();
    const json_t *home = rqo(req, "home");
    const char *boot = ostr(req, "bootstrap");
    if (py_truthy(home)) json_append(arr, json_copy(home));
    if (boot[0]) append_split_parts(arr, boot, ',');
    return arr;
}

json_t *w27_gchat_p4_sa_source_kind(const json_t *req) {
    const json_t *sv = rqo(req, "sa_value");
    json_t *out = json_object();
    const char *source = NULL;
    const char *kind = "creds";

    if (py_truthy(sv)) {
        char *ls = py_lstrip(py_str(sv));
        int is_json = ls[0] == '{';
        free(ls);
        if (is_json) {
            source = "inline";
            kind = py_truthy(rqo(req, "inline_valid")) ? "creds" : "inline_invalid";
        } else if (!py_truthy(rqo(req, "exists"))) {
            kind = "not_found";
        } else {
            source = "file";
            kind = py_truthy(rqo(req, "file_valid")) ? "creds" : "file_invalid";
        }
    } else if (py_truthy(rqo(req, "multiplex")) && py_truthy(rqo(req, "env_has_sa"))) {
        kind = "adc_foreign";
    } else if (!py_truthy(rqo(req, "has_google_auth"))) {
        kind = "adc_no_auth";
    } else if (py_truthy(rqo(req, "adc_raises"))) {
        source = "adc";
        kind = "adc_failed";
    } else {
        source = "adc";
    }
    json_set(out, "source", (source && !strcmp(kind, "creds")) ? json_string(source) : json_null());
    json_set(out, "kind", json_string(kind));
    return out;
}

json_t *w27_gchat_p4_release_unadmitted(const json_t *req) {
    const json_t *msg = rqo(req, "msg");
    const json_t *env = rqo(req, "envelope");
    const char *name = ostr(msg, "name");
    json_t *out = json_object();
    const json_t *ev_space = rqo(env, "space");
    const json_t *mv_space;
    const char *space_name, *thread_name;

    json_set(out, "dedup", json_string(name));
    mv_space = rqo(msg, "space");
    space_name = ostr(py_truthy(ev_space) ? ev_space : mv_space, "name");
    thread_name = ostr(rqo(msg, "thread"), "name");
    if (space_name[0] && thread_name[0]) {
        json_t *d = json_object();
        json_set(d, "chat", json_string(space_name));
        json_set(d, "thread", json_string(thread_name));
        json_set(out, "decr", d);
    } else {
        json_set(out, "decr", json_null());
    }
    return out;
}

json_t *w27_gchat_p4_outbound_thread_incr(const json_t *req) {
    const char *chat_id = ostr(req, "chat_id");
    const json_t *resp = rqo(req, "resp");
    const char *thread = ostr(rqo(resp, "thread"), "name");
    if (chat_id[0] && thread[0]) {
        json_t *out = json_object();
        json_set(out, "chat", json_string(chat_id));
        json_set(out, "thread", json_string(thread));
        return out;
    }
    return json_null();
}

json_t *w27_gchat_p4_invalidate_identity(const json_t *req) {
    const json_t *id = rqo(req, "identity");
    if (!py_truthy(id)) return json_string("none");
    if (is_str(id) && !strcmp(id->str_val, GC_LEGACY_USER_IDENTITY)) return json_string("legacy");
    return json_string("email");
}

json_t *w27_gchat_p4_loop_accepts(const json_t *req) {
    int present = py_truthy(rqo(req, "present"));
    int closed = py_truthy(rqo(req, "closed"));
    return json_bool(present && !closed);
}

