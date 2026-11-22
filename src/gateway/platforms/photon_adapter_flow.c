/*
 * photon_adapter_flow.c — Photon Spectrum (iMessage) adapter FLOW cores and
 * leaves of plugins/platforms/photon/adapter.py (hermes-agent).
 *
 * Ports three coherent decision clusters:
 *  A. Inbound content-normalization tree: _normalize_content dispatch ladder,
 *     _normalize_group_content / _normalize_binary_payload accumulators, the
 *     richlink / voice / preview predicates, _url_only_candidate (regex +
 *     urlparse scheme/netloc gate) and the pure text leaves.
 *  B. Outbound send flow: _sidecar_send -> _post_send -> (seamed _sidecar_call)
 *     with the richlink-first / markdown / truncation ladder, _sidecar_send_poll,
 *     _sidecar_send_attachment, _sidecar_try, _sidecar_payload_text,
 *     _markdown_enabled / _richlink_candidate and the body/label builders.
 *  C. Reaction/retry state helpers: _normalize_chat_key, the bounded_put-backed
 *     recorders (_record_sent_message / _put_by_chat / _record_last_inbound /
 *     _record_recent_richlink), _is_recent_richlink_preview (window + pop),
 *     _reactions_enabled, _is_retryable_error / _is_permanent_sidecar_failure /
 *     _send_retry_is_final.
 *
 * Universal seam: every SEAMED sibling call enters ONE callback
 * ph_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting the
 * identical event shape {"name","args","ret"} from the SAME scripts. args is
 * ADOPTED into the trace event; the return is a NEW owned json_t (NULL == Python
 * None; {"__raise__":cls,"message":str(exc),...} == a raised exception). A
 * missing/exhausted script is a fail-closed RuntimeError on both sides.
 * BaseException kinds (SystemExit / KeyboardInterrupt / CancelledError /
 * GeneratorExit) pass through every catch site (_sidecar_try / _post_send catch
 * Exception only, narrower than BaseException).
 *
 * Mode table (how a function is invoked BY ITS CALLERS):
 *  REAL (run inline, direct C call): ALL 36 ported functions. The normalization
 *    tree, the outbound orchestrators (_sidecar_send / _post_send /
 *    _sidecar_send_poll / _sidecar_send_attachment / _sidecar_try) and the state
 *    helpers call their REAL children directly.
 *  SEAMED (ph_call, scripted): the unported siblings _cache_inbound_attachment,
 *    _guess_mime, _sidecar_call, _record_sent_text_async and the foreign imports
 *    strip_markdown, get_scoped_secret (_get_scoped_secret), bounded_put,
 *    time.time, base_is_retryable_error (BasePlatformAdapter._is_retryable_error),
 *    validate_media_delivery_path. A REAL function calls its SEAMED children via
 *    ph_call; each SEAMED name is scripted everywhere it is called.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and ph_-prefixed.
 *
 * Representation contract (shared with tests/oracle_photon_adapter_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__": cls, "message": str(exc)}; a PhotonSidecarError
 *   raise additionally carries "error_class" and "retryable" (read by _post_send's
 *   structured branch). fa_make_exc preserves every extra spec key.
 * - MessageType projects to its .value string ("text"/"photo"/"video"/"audio"/
 *   "voice"/"document"). The _Normalized tuple projects to a 4-element JSON array
 *   [text, mtype_value, media_urls, media_types].
 * - SendResult projects via dataclasses.asdict to ALL 8 fields:
 *   {success, message_id, error, raw_response, retryable, retry_after,
 *    continuation_message_ids ([] for the () default), error_kind}.
 * - bounded_put is SEAMED (claimed by platforms_helpers_flow) but its in-place
 *   store mutation is applied identically on both sides AFTER the seam event
 *   (args carry the PRE-mutation store snapshot): pop(key); set(key,value);
 *   evict oldest while len>cap. libjson object key order == dict insertion order.
 * - time.time is a seam; its result feeds bounded_put's value arg (arg evaluation
 *   order: time.time fires BEFORE bounded_put).
 * - Adapter state (in.state) holds the mutable stores (_sent_message_ids,
 *   _last_inbound_by_chat, _recent_richlinks_by_chat) and optional class-const
 *   overrides (MAX_MESSAGE_LENGTH default 8000, _SENT_IDS_MAX default 1000,
 *   _LAST_INBOUND_CHATS_MAX default 200). Method cases emit the mutated state;
 *   module-function cases pass state {} and emit {}.
 * - Integral floats == ints (libjson %.0f; oracle canon folds integral floats).
 *
 * Fixture corpus restrictions (documented; generator honours them):
 * - str()-computed positions (duration in _normalize_binary_payload, unhandled
 *   ctype / item_type markers, poll option elements, attachment path) carry
 *   str / int / bool / None only — no containers, no non-integral floats
 *   (Python str(5.0)=="5.0" vs C %.0f=="5"). duration is int|null|str (no bool).
 * - lower() positions (name/id/mime/error/secret) are ASCII.
 * - _url_only_candidate / _SIDECAR_URL_RE text is ASCII; bracketed IPv6/IPvFuture
 *   hosts are OUT of domain (the C fail-closes on a bracketed netloc with a
 *   ValueError rather than transcribing ipaddress.ip_address). urlparse otherwise
 *   reduces to a scheme/netloc split (netloc ends at the first '/', '?', '#').
 * - _cache_inbound_attachment / _guess_mime / _sidecar_call are scripted; the
 *   base64 + media-cache + httpx surfaces are never transcribed.
 * - _sidecar_call success scripts are dicts; PhotonSidecarError raise scripts
 *   carry error_class/retryable and a message equal to the intended str(exc).
 * - attachment paths use '/' or '\\' separators (ntpath.basename splits both).
 * - SendResult-double inputs (_is_permanent_sidecar_failure / _send_retry_is_final)
 *   always carry raw_response (present-null models None); an absent attribute
 *   would raise AttributeError live and is out of the fixture domain.
 * - Seam raise propagation: every helper that CONSUMES a seam result checks
 *   ph_is_raise and propagates the descriptor (get_scoped_secret / strip_markdown /
 *   base_is_retryable_error / _cache_inbound_attachment / time.time / bounded_put),
 *   matching live where the recording stub raises through the caller. _post_send /
 *   _sidecar_try catch Exception (PhotonSidecarError first in _post_send) and let
 *   BaseException pass. bounded_put mutates the store only on a non-raise result.
 *
 * PoP annotations credit plugins/platforms/photon/adapter.py per function.
 */
#include "hermes_json.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── universal seam context ── */
typedef struct ph_ctx {
    json_t *state;      /* owned; adapter attributes, mutated in place */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} ph_ctx_t;

/* ── basic helpers ── */
static bool ph_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ph_kind(const json_t *v) {
    const json_t *k = v ? json_obj_get(v, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static bool ph_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
static bool ph_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *ph_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool ph_str_eq(const json_t *v, const char *s) {
    const char *t = ph_str(v);
    return t && s && strcmp(t, s) == 0;
}
static json_t *ph_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
/* Materialize a scripted raise spec, preserving EVERY extra key (error_class /
 * retryable for PhotonSidecarError) so _post_send can read them and the trace
 * ret matches the oracle's spec projection. */
static json_t *ph_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *e = ph_err(type, msg);
    if (spec && spec->type == JSON_OBJECT) {
        for (size_t i = 0; i < spec->c.count; i++) {
            const char *k = spec->c.keys[i];
            if (strcmp(k, "type") == 0 || strcmp(k, "message") == 0) continue;
            json_set(e, k, json_copy(spec->c.items[i]));
        }
    }
    return e;
}
/* The one universal seam. args is ADOPTED into the trace event; the return is a
 * fresh owned node (scripted value copy or raise descriptor). */
static json_t *ph_call(ph_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ph_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ph_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = ph_make_exc(raise);
            } else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}
static json_t *ph_args(void) { return json_object(); }
static json_t *ph_str_ret(char *s); /* forward: wraps a malloc'd string (NULL -> null) */
static void ph_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void ph_set_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void ph_set_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void ph_set_null(json_t *a, const char *k) { json_set(a, k, json_null()); }
static void ph_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
/* state readers (class-const defaults) */
static double ph_state_num(ph_ctx_t *ctx, const char *k, double def) {
    return json_get_num(ctx->state, k, def);
}
static json_t *ph_state_obj(ph_ctx_t *ctx, const char *k) {
    json_t *v = json_obj_get(ctx->state, k);
    if (!v || v->type != JSON_OBJECT) {
        v = json_object();
        json_set(ctx->state, k, v);
    }
    return v;
}
/* ── string helpers ── */
static char *ph_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static uint32_t *ph_utf8_decode(const char *s, size_t *n) {
    if (!s) s = "";
    size_t len = strlen(s);
    uint32_t *cps = malloc((len + 1) * sizeof(uint32_t));
    size_t k = 0;
    for (size_t i = 0; i < len;) {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp; size_t adv;
        if (c < 0x80) { cp = c; adv = 1; }
        else if ((c & 0xE0) == 0xC0 && i + 1 < len) {
            cp = ((c & 0x1Fu) << 6) | ((unsigned char)s[i+1] & 0x3Fu); adv = 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < len) {
            cp = ((c & 0x0Fu) << 12) | (((unsigned char)s[i+1] & 0x3Fu) << 6)
               | ((unsigned char)s[i+2] & 0x3Fu); adv = 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < len) {
            cp = ((c & 0x07u) << 18) | (((unsigned char)s[i+1] & 0x3Fu) << 12)
               | (((unsigned char)s[i+2] & 0x3Fu) << 6) | ((unsigned char)s[i+3] & 0x3Fu); adv = 4;
        } else { cp = 0xFFFD; adv = 1; }
        cps[k++] = cp; i += adv;
    }
    *n = k;
    return cps;
}
static void ph_utf8_putcp(char **q, uint32_t cp) {
    char *o = *q;
    if (cp < 0x80) { *o++ = (char)cp; }
    else if (cp < 0x800) {
        *o++ = (char)(0xC0 | (cp >> 6)); *o++ = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        *o++ = (char)(0xE0 | (cp >> 12)); *o++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *o++ = (char)(0x80 | (cp & 0x3F));
    } else {
        *o++ = (char)(0xF0 | (cp >> 18)); *o++ = (char)(0x80 | ((cp >> 12) & 0x3F));
        *o++ = (char)(0x80 | ((cp >> 6) & 0x3F)); *o++ = (char)(0x80 | (cp & 0x3F));
    }
    *q = o;
}
static char *ph_utf8_encode(const uint32_t *cps, size_t n) {
    char *o = malloc(n * 4 + 1);
    if (!o) return NULL;
    char *q = o;
    for (size_t i = 0; i < n; i++) ph_utf8_putcp(&q, cps[i]);
    *q = '\0';
    return o;
}
static size_t ph_utf8_len(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}
/* char slice s[:limit] (Python len/slice are codepoint-based) */
static char *ph_utf8_slice(const char *s, size_t limit) {
    size_t n = 0;
    uint32_t *cps = ph_utf8_decode(s, &n);
    if (limit > n) limit = n;
    char *o = ph_utf8_encode(cps, limit);
    free(cps);
    return o;
}
/* Python str.strip() White_Space set (ASCII corpus in fixtures). */
static bool ph_is_ws_cp(uint32_t c) {
    if (c == 0x20 || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F)) return true;
    if (c == 0x85 || c == 0xA0 || c == 0x1680) return true;
    if (c >= 0x2000 && c <= 0x200A) return true;
    if (c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000) return true;
    return false;
}
static char *ph_strip(const char *s) {
    size_t n = 0;
    uint32_t *cps = ph_utf8_decode(s, &n);
    size_t a = 0, b = n;
    while (a < b && ph_is_ws_cp(cps[a])) a++;
    while (b > a && ph_is_ws_cp(cps[b-1])) b--;
    char *o = ph_utf8_encode(cps + a, b - a);
    free(cps);
    return o;
}
static char *ph_lstrip_chars(const char *s, const char *chars) {
    /* lstrip with an explicit char set (e.g. " ,:-"); NULL chars => whitespace */
    if (!s) s = "";
    size_t n = strlen(s), a = 0;
    while (a < n) {
        unsigned char c = (unsigned char)s[a];
        bool ws = chars ? (strchr(chars, c) != NULL)
                        : ph_is_ws_cp(c);
        if (!ws) break;
        a++;
    }
    return ph_strdup(s + a);
}
static char *ph_lower_ascii(const char *s) {
    char *o = ph_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static bool ph_startswith(const char *s, const char *pre) {
    return s && pre && strncmp(s, pre, strlen(pre)) == 0;
}
static bool ph_endswith(const char *s, const char *suf) {
    if (!s || !suf) return false;
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && strcmp(s + ls - lf, suf) == 0;
}
static bool ph_contains(const char *s, const char *sub) {
    return s && sub && strstr(s, sub) != NULL;
}
/* Python str() of a scalar (null/bool/int/str). Non-integral floats and
 * containers are OUT of the fixture domain. */
static char *ph_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ph_strdup("None");
    if (v->type == JSON_BOOL) return ph_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(buf, sizeof buf, "%lld", (long long)d);
        else snprintf(buf, sizeof buf, "%g", d);
        return ph_strdup(buf);
    }
    if (v->type == JSON_STRING) return ph_strdup(v->str_val ? v->str_val : "");
    return ph_strdup("");
}
/* "\n".join(list of malloc'd strings) — caller frees the parts array contents */
static char *ph_join_nl(char **parts, size_t n) {
    size_t total = 0;
    for (size_t i = 0; i < n; i++) total += strlen(parts[i]) + 1;
    char *o = malloc(total + 1);
    if (!o) return NULL;
    char *q = o;
    for (size_t i = 0; i < n; i++) {
        if (i) *q++ = '\n';
        size_t l = strlen(parts[i]);
        memcpy(q, parts[i], l); q += l;
    }
    *q = '\0';
    return o;
}
/* ntpath.basename: split on the last '/' or '\\' */
static char *ph_basename(const char *s) {
    if (!s) s = "";
    const char *last = s;
    for (const char *p = s; *p; p++)
        if (*p == '/' || *p == '\\') last = p + 1;
    return ph_strdup(last);
}
/* ── regex / urlparse leaves ── */
/* re.fullmatch(r"https?://\S+", candidate, IGNORECASE), ASCII corpus.
 * Returns true when the WHOLE string is http(s):// + >=1 non-whitespace. */
static bool ph_url_fullmatch(const char *s) {
    if (!s) return false;
    const char *p = s;
    /* scheme http or https, case-insensitive */
    if (strncasecmp(p, "http", 4) == 0) p += 4;
    else return false;
    if (*p == 's' || *p == 'S') p++;
    if (strncmp(p, "://", 3) != 0) return false;
    p += 3;
    if (!*p) return false;
    for (; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v')
            return false;
    }
    return true;
}
/* _SIDECAR_URL_RE.search: https?://[^\s)'"<>]+ (IGNORECASE), ASCII corpus. */
static bool ph_sidecar_url_search(const char *s) {
    if (!s) return false;
    for (const char *p = s; *p; p++) {
        const char *q = p;
        if (strncasecmp(q, "http", 4) != 0) continue;
        q += 4;
        if (*q == 's' || *q == 'S') q++;
        if (strncmp(q, "://", 3) != 0) continue;
        q += 3;
        const char *start = q;
        while (*q) {
            unsigned char c = (unsigned char)*q;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'
                || c == ')' || c == '\'' || c == '"' || c == '<' || c == '>') break;
            q++;
        }
        if (q > start) return true;
    }
    return false;
}
/* urlparse scheme/netloc for the bracket-free http(s) domain. Returns false and
 * sets *err on a bracketed netloc (fail-closed ValueError, out of domain).
 * The scheme / netloc outputs are malloc'd. netloc ends at the first '/', '?', '#'. */
static bool ph_urlparse(const char *url, char **scheme, char **netloc, bool *err) {
    *err = false;
    *scheme = NULL; *netloc = NULL;
    const char *colon = strchr(url, ':');
    if (!colon) { *scheme = ph_strdup(""); *netloc = ph_strdup(""); return true; }
    *scheme = ph_strdup("");
    {
        size_t sl = (size_t)(colon - url);
        char *sc = malloc(sl + 1);
        memcpy(sc, url, sl); sc[sl] = '\0';
        *scheme = sc;
    }
    const char *rest = colon + 1;
    if (strncmp(rest, "//", 2) == 0) {
        const char *nl = rest + 2;
        const char *end = nl;
        while (*end && *end != '/' && *end != '?' && *end != '#') end++;
        size_t nlen = (size_t)(end - nl);
        char *nloc = malloc(nlen + 1);
        memcpy(nloc, nl, nlen); nloc[nlen] = '\0';
        if (strchr(nloc, '[') || strchr(nloc, ']')) {
            free(nloc);
            *netloc = ph_strdup("");
            *err = true;
            return false;
        }
        *netloc = nloc;
    } else {
        *netloc = ph_strdup("");
    }
    return true;
}
/* _DM_CHAT_GUID_RE.match: ^any;-;(\+\d{6,})$ (ASCII digits). Returns malloc'd
 * group(1) ("+digits") or NULL. */
static char *ph_dm_guid_match(const char *s) {
    if (!s) return NULL;
    const char *pre = "any;-;+";
    size_t pl = strlen(pre);
    if (strncmp(s, pre, pl) != 0) return NULL;
    const char *p = s + pl;
    size_t nd = 0;
    while (*p >= '0' && *p <= '9') { p++; nd++; }
    if (nd < 6 || *p != '\0') return NULL;
    return ph_strdup(s + pl - 1); /* include the '+' */
}
/* ── projections ── */
static json_t *ph_send_result(bool success, const char *message_id, const char *error,
                              json_t *raw_response, bool retryable) {
    json_t *r = json_object();
    ph_set_b(r, "success", success);
    if (message_id) ph_set_s(r, "message_id", message_id); else ph_set_null(r, "message_id");
    if (error) ph_set_s(r, "error", error); else ph_set_null(r, "error");
    json_set(r, "raw_response", raw_response ? raw_response : json_null());
    ph_set_b(r, "retryable", retryable);
    ph_set_null(r, "retry_after");
    json_set(r, "continuation_message_ids", json_array());
    ph_set_null(r, "error_kind");
    return r;
}
/* bounded_put in-place mutation (applied after the seam event on both sides). */
static void ph_bounded_put(json_t *store, const char *key, json_t *value, double cap) {
    json_obj_del(store, key);
    json_set(store, key, value ? value : json_null());
    while ((double)store->c.count > cap && store->c.count > 0) {
        json_obj_del(store, store->c.keys[0]);
    }
}
/* ── normalization tree (REAL cores) ── */
#define PH_MT_TEXT "text"
#define PH_MT_PHOTO "photo"
#define PH_MT_VIDEO "video"
#define PH_MT_AUDIO "audio"
#define PH_MT_VOICE "voice"
#define PH_MT_DOCUMENT "document"

static json_t *ph_normalized(const char *text, const char *mtype,
                             json_t *urls, json_t *types) {
    json_t *a = json_array();
    json_append(a, json_string(text ? text : ""));
    json_append(a, json_string(mtype));
    json_append(a, urls ? urls : json_array());
    json_append(a, types ? types : json_array());
    return a;
}
static const char *ph_get_str_or(const json_t *obj, const char *k, const char *def) {
    /* content.get(k) or def — truthiness, not presence (null/"" -> def) */
    const json_t *v = json_obj_get(obj, k);
    const char *s = ph_str(v);
    if (s && s[0]) return s;
    return def;
}
static bool ph_is_voice_payload(const json_t *payload) {
    if (ph_str_eq(json_obj_get(payload, "type"), "voice")) return true;
    if (!ph_str_eq(json_obj_get(payload, "type"), "attachment")) return false;
    const char *name = ph_get_str_or(payload, "name", "");
    char *ln = ph_lower_ascii(name);
    bool caf = ph_endswith(ln, ".caf");
    free(ln);
    return caf || ph_str_eq(json_obj_get(payload, "mimeType"), "audio/x-caf");
}
static char *ph_cached_media_type(const json_t *payload, const char *mime, bool is_voice) {
    if (!mime) mime = "";
    if (!is_voice) return ph_strdup(mime[0] ? mime : "application/octet-stream");
    char *lm = ph_lower_ascii(mime);
    bool audio = ph_startswith(lm, "audio/");
    free(lm);
    if (audio) return ph_strdup(mime);
    const char *name = ph_get_str_or(payload, "name", "");
    char *ln = ph_lower_ascii(name);
    bool caf = ph_endswith(ln, ".caf");
    free(ln);
    return ph_strdup(caf ? "audio/x-caf" : "audio/mp4");
}
static const char *ph_attachment_message_type(const char *mime) {
    char *lm = ph_lower_ascii(mime ? mime : "");
    const char *mt = PH_MT_DOCUMENT;
    if (ph_startswith(lm, "image/")) mt = PH_MT_PHOTO;
    else if (ph_startswith(lm, "video/")) mt = PH_MT_VIDEO;
    else if (ph_startswith(lm, "audio/")) mt = PH_MT_AUDIO;
    free(lm);
    return mt;
}
static json_t *ph_group_item_contents(const json_t *content) {
    json_t *out = json_array();
    const json_t *items = json_obj_get(content, "items");
    if (!ph_truthy(items)) items = NULL;
    if (items && items->type == JSON_ARRAY) {
        for (size_t i = 0; i < items->c.count; i++) {
            const json_t *item = items->c.items[i];
            const json_t *c = (item && item->type == JSON_OBJECT) ? json_obj_get(item, "content") : NULL;
            if (c && c->type == JSON_OBJECT) json_append(out, json_copy(c));
        }
    }
    return out;
}
static char *ph_format_richlink_content(const json_t *content) {
    const char *keys[3] = {"url", "title", "summary"};
    char *vals[3];
    for (int i = 0; i < 3; i++) {
        const json_t *v = json_obj_get(content, keys[i]);
        /* str(content.get(k) or "") — falsy collapses to "" (NOT "None") */
        char *s = ph_truthy(v) ? ph_pystr(v) : ph_strdup("");
        vals[i] = ph_strip(s);
        free(s);
    }
    /* parts = [p for p in (title, summary if summary!=title else "", url) if p] */
    char *parts[3]; size_t np = 0;
    if (vals[1][0]) parts[np++] = vals[1];
    if (strcmp(vals[2], vals[1]) != 0 && vals[2][0]) parts[np++] = vals[2];
    if (vals[0][0]) parts[np++] = vals[0];
    char *o;
    if (np) o = ph_join_nl(parts, np);
    else o = ph_strdup("[Photon rich link received with no URL]");
    for (int i = 0; i < 3; i++) free(vals[i]);
    return o;
}
static char *ph_url_only_candidate(const char *text) {
    char *cand = ph_strip(text ? text : "");
    if (!ph_url_fullmatch(cand)) { free(cand); return NULL; }
    char *scheme = NULL, *netloc = NULL; bool err = false;
    ph_urlparse(cand, &scheme, &netloc, &err);
    char *ls = ph_lower_ascii(scheme);
    bool ok = !err && (strcmp(ls, "http") == 0 || strcmp(ls, "https") == 0) && netloc[0];
    free(ls); free(scheme); free(netloc);
    if (!ok) { free(cand); return NULL; }
    return cand;
}
static char *ph_richlink_url_from_content(const json_t *content) {
    const char *ctype = ph_str(json_obj_get(content, "type"));
    if (ctype && (strcmp(ctype, "text") == 0 || strcmp(ctype, "richlink") == 0)) {
        const char *key = strcmp(ctype, "text") == 0 ? "text" : "url";
        const char *raw = ph_get_str_or(content, key, "");
        return ph_url_only_candidate(raw);
    }
    if (ctype && strcmp(ctype, "group") == 0) {
        json_t *contents = ph_group_item_contents(content);
        char *found = NULL;
        for (size_t i = 0; i < contents->c.count; i++) {
            char *u = ph_richlink_url_from_content(contents->c.items[i]);
            if (u && u[0]) { found = u; break; }
            free(u);
        }
        json_free(contents);
        return found;
    }
    return NULL;
}
#define PH_PREVIEW_SUFFIX ".pluginpayloadattachment"
static bool ph_is_richlink_preview_attachment(const json_t *payload) {
    if (!ph_str_eq(json_obj_get(payload, "type"), "attachment")) return false;
    const char *keys[2] = {"name", "id"};
    for (int i = 0; i < 2; i++) {
        const json_t *v = json_obj_get(payload, keys[i]);
        char *s = ph_truthy(v) ? ph_pystr(v) : ph_strdup(""); /* str(... or "") */
        char *ls = ph_lower_ascii(s);
        bool hit = ph_contains(ls, PH_PREVIEW_SUFFIX);
        free(s); free(ls);
        if (hit) return true;
    }
    return false;
}
static char *ph_preview_label_one(const json_t *c) {
    const json_t *name = json_obj_get(c, "name");
    if (ph_truthy(name)) return ph_pystr(name);
    const json_t *id = json_obj_get(c, "id");
    if (ph_truthy(id)) return ph_pystr(id);
    return ph_strdup("(unnamed)");
}
static char *ph_richlink_preview_label(const json_t *content) {
    const char *ctype = ph_str(json_obj_get(content, "type"));
    if (ctype && strcmp(ctype, "attachment") == 0) return ph_preview_label_one(content);
    if (ctype && strcmp(ctype, "group") == 0) {
        json_t *contents = ph_group_item_contents(content);
        size_t n = contents->c.count;
        char **labels = malloc((n ? n : 1) * sizeof(char *));
        for (size_t i = 0; i < n; i++) labels[i] = ph_preview_label_one(contents->c.items[i]);
        /* ", ".join */
        size_t total = 0;
        for (size_t i = 0; i < n; i++) total += strlen(labels[i]) + 2;
        char *o = malloc(total + 1);
        char *q = o;
        for (size_t i = 0; i < n; i++) {
            if (i) { *q++ = ','; *q++ = ' '; }
            size_t l = strlen(labels[i]); memcpy(q, labels[i], l); q += l;
            free(labels[i]);
        }
        *q = '\0';
        free(labels);
        json_free(contents);
        if (!o[0]) { free(o); return ph_strdup("(group)"); }
        return o;
    }
    return ph_strdup("(unknown)");
}
static bool ph_is_richlink_preview_content(const json_t *content) {
    if (ph_is_richlink_preview_attachment(content)) return true;
    if (!ph_str_eq(json_obj_get(content, "type"), "group")) return false;
    const json_t *items = json_obj_get(content, "items");
    if (!ph_truthy(items)) items = NULL;
    size_t nitems = (items && items->type == JSON_ARRAY) ? items->c.count : 0;
    json_t *contents = ph_group_item_contents(content);
    bool ok = nitems > 0 && contents->c.count == nitems;
    if (ok) {
        for (size_t i = 0; i < contents->c.count; i++)
            if (!ph_is_richlink_preview_attachment(contents->c.items[i])) { ok = false; break; }
    }
    json_free(contents);
    return ok;
}
static const json_t *ph_reply_inner(const json_t *content) {
    if (ph_str_eq(json_obj_get(content, "type"), "reply")) {
        const json_t *inner = json_obj_get(content, "content");
        if (inner && inner->type == JSON_OBJECT) return inner;
    }
    return content;
}
static bool ph_carries_voice(const json_t *content) {
    if (ph_str_eq(json_obj_get(content, "type"), "group")) {
        json_t *contents = ph_group_item_contents(content);
        bool any = false;
        for (size_t i = 0; i < contents->c.count; i++)
            if (ph_is_voice_payload(contents->c.items[i])) { any = true; break; }
        json_free(contents);
        return any;
    }
    return ph_is_voice_payload(content);
}
static char *ph_mention_gate_text(const json_t *content) {
    const char *ctype = ph_str(json_obj_get(content, "type"));
    if (ctype && strcmp(ctype, "text") == 0) return ph_strdup(ph_get_str_or(content, "text", ""));
    if (ctype && strcmp(ctype, "richlink") == 0) return ph_format_richlink_content(content);
    if (ctype && strcmp(ctype, "group") == 0) {
        json_t *contents = ph_group_item_contents(content);
        size_t n = contents->c.count;
        char **parts = malloc((n ? n : 1) * sizeof(char *));
        size_t np = 0;
        for (size_t i = 0; i < n; i++) {
            char *t = ph_mention_gate_text(contents->c.items[i]);
            if (t[0]) parts[np++] = t; else free(t);
        }
        char *o = ph_join_nl(parts, np);
        for (size_t i = 0; i < np; i++) free(parts[i]);
        free(parts);
        json_free(contents);
        return o;
    }
    return ph_strdup("");
}
/* ── normalization orchestrators (REAL, seam _cache_inbound_attachment) ── */
static json_t *ph_normalize_binary_payload(ph_ctx_t *ctx, const json_t *payload) {
    bool is_voice = ph_is_voice_payload(payload);
    const char *name = ph_get_str_or(payload, "name", is_voice ? "voice" : "(unnamed)");
    const char *mime = ph_get_str_or(payload, "mimeType", "");
    const char *mtype = is_voice ? PH_MT_VOICE : ph_attachment_message_type(mime);
    const char *label = is_voice ? "voice" : "attachment";
    json_t *a = ph_args();
    ph_set_copy(a, "content", payload);
    ph_set_s(a, "name", name);
    ph_set_s(a, "mime", mime);
    ph_set_b(a, "force_audio", is_voice);
    json_t *cached = ph_call(ctx, "_cache_inbound_attachment", a);
    if (ph_is_raise(cached)) return cached;
    if (ph_truthy(cached) && cached->type == JSON_STRING) {
        char *marker = malloc(strlen(label) + 3);
        sprintf(marker, "(%s)", label);
        json_t *urls = json_array(); json_append(urls, json_copy(cached));
        char *cmt = ph_cached_media_type(payload, mime, is_voice);
        json_t *types = json_array(); json_append(types, json_string(cmt)); free(cmt);
        json_t *r = ph_normalized(marker, mtype, urls, types);
        free(marker); json_free(cached);
        return r;
    }
    json_free(cached);
    const json_t *dur = json_obj_get(payload, "duration");
    char *duration_text = ph_strdup("");
    if (dur && dur->type == JSON_NUMBER) {
        char *ds = ph_pystr(dur);
        duration_text = malloc(strlen(ds) + 16);
        sprintf(duration_text, ", duration: %ss", ds);
        free(ds);
    }
    const char *mime_disp = mime[0] ? mime : "unknown MIME";
    char *text = malloc(strlen(label) + strlen(name) + strlen(mime_disp) + strlen(duration_text) + 40);
    sprintf(text, "[Photon %s received: %s (%s%s)]", label, name, mime_disp, duration_text);
    free(duration_text);
    json_t *r = ph_normalized(text, mtype, json_array(), json_array());
    free(text);
    return r;
}
static json_t *ph_normalize_group_content(ph_ctx_t *ctx, const json_t *content) {
    json_t *contents = ph_group_item_contents(content);
    char **parts = malloc((contents->c.count + 1) * sizeof(char *));
    size_t np = 0;
    const char *mtype = PH_MT_TEXT;
    char *mtype_owned = NULL;  /* survives until ph_normalized consumes mtype */
    json_t *media_urls = json_array();
    json_t *media_types = json_array();
    for (size_t i = 0; i < contents->c.count; i++) {
        const json_t *ic = contents->c.items[i];
        const json_t *itn = json_obj_get(ic, "type");
        const char *it = ph_str(itn);
        if (it && (strcmp(it, "attachment") == 0 || strcmp(it, "voice") == 0)) {
            json_t *norm = ph_normalize_binary_payload(ctx, ic);
            const char *marker = ph_str(json_get(norm, 0));
            char *imt_copy = ph_strdup(ph_str(json_get(norm, 1)));
            const char *imt = imt_copy;
            json_t *iurls = json_get(norm, 2);
            json_t *itypes = json_get(norm, 3);
            if (strcmp(mtype, PH_MT_TEXT) == 0) {
                free(mtype_owned);
                mtype_owned = ph_strdup(imt);
                mtype = mtype_owned;
            }
            for (size_t j = 0; j < json_len(iurls); j++) json_append(media_urls, json_copy(json_get(iurls, j)));
            for (size_t j = 0; j < json_len(itypes); j++) json_append(media_types, json_copy(json_get(itypes, j)));
            if (json_len(iurls) == 0) parts[np++] = ph_strdup(marker ? marker : "");
            json_free(norm);
            free(imt_copy);
        } else if (it && strcmp(it, "text") == 0) {
            parts[np++] = ph_strdup(ph_get_str_or(ic, "text", ""));
        } else if (it && strcmp(it, "richlink") == 0) {
            parts[np++] = ph_format_richlink_content(ic);
        } else if (ph_truthy(itn)) {
            char *its = ph_pystr(itn);
            char *m = malloc(strlen(its) + 40);
            sprintf(m, "[Photon content type not handled: %s]", its);
            parts[np++] = m; free(its);
        }
    }
    if (media_urls->c.count > 0 && strcmp(mtype, PH_MT_TEXT) == 0) mtype = PH_MT_DOCUMENT;
    /* "\n".join(p for p in parts if p).strip() */
    char **nz = malloc((np + 1) * sizeof(char *));
    size_t nn = 0;
    for (size_t i = 0; i < np; i++) if (parts[i][0]) nz[nn++] = parts[i];
    char *joined = ph_join_nl(nz, nn);
    char *text = ph_strip(joined);
    free(joined); free(nz);
    for (size_t i = 0; i < np; i++) free(parts[i]);
    free(parts);
    json_free(contents);
    const char *final = text;
    char *fallback = NULL;
    if (!text[0]) {
        fallback = ph_strdup(media_urls->c.count > 0 ? "(attachment)" : "[Photon empty group received]");
        final = fallback;
    }
    json_t *r = ph_normalized(final, mtype, media_urls, media_types);
    free(text); free(fallback); free(mtype_owned);
    return r;
}
static json_t *ph_normalize_content(ph_ctx_t *ctx, const json_t *content) {
    const json_t *ctn = json_obj_get(content, "type");
    const char *ctype = ph_str(ctn);
    if (ctype) {
        if (strcmp(ctype, "text") == 0)
            return ph_normalized(ph_get_str_or(content, "text", ""), PH_MT_TEXT, json_array(), json_array());
        if (strcmp(ctype, "attachment") == 0 || strcmp(ctype, "voice") == 0)
            return ph_normalize_binary_payload(ctx, content);
        if (strcmp(ctype, "richlink") == 0) {
            char *t = ph_format_richlink_content(content);
            json_t *r = ph_normalized(t, PH_MT_TEXT, json_array(), json_array());
            free(t); return r;
        }
        if (strcmp(ctype, "group") == 0)
            return ph_normalize_group_content(ctx, content);
    }
    char *cs = ph_pystr(ctn);
    char *m = malloc(strlen(cs) + 40);
    sprintf(m, "[Photon content type not handled: %s]", cs);
    json_t *r = ph_normalized(m, PH_MT_TEXT, json_array(), json_array());
    free(m); free(cs);
    return r;
}
/* ── outbound module helpers (REAL) ── */
static json_t *ph_markdown_enabled(ph_ctx_t *ctx) {
    json_t *a = ph_args();
    ph_set_s(a, "name", "PHOTON_MARKDOWN");
    ph_set_s(a, "default", "true");
    json_t *r = ph_call(ctx, "get_scoped_secret", a);
    if (ph_is_raise(r)) return r;
    const char *s = ph_str(r);
    char *st = ph_strip(s ? s : "");
    char *lw = ph_lower_ascii(st);
    bool off = strcmp(lw, "false") == 0 || strcmp(lw, "0") == 0 || strcmp(lw, "no") == 0;
    free(st); free(lw); json_free(r);
    return json_bool(!off);
}
static json_t *ph_richlink_candidate(ph_ctx_t *ctx, const char *text) {
    /* _url_only_candidate(text) if _markdown_enabled() else None — COND first */
    json_t *me = ph_markdown_enabled(ctx);
    if (ph_is_raise(me)) return me;
    bool enabled = ph_truthy(me);
    json_free(me);
    if (!enabled) return json_null();
    return ph_str_ret(ph_url_only_candidate(text));
}
static json_t *ph_sidecar_payload_text(ph_ctx_t *ctx, const char *text, bool send_markdown) {
    if (send_markdown && !ph_sidecar_url_search(text ? text : ""))
        return json_string(text ? text : "");
    json_t *a = ph_args();
    ph_set_s(a, "text", text ? text : "");
    ph_set_b(a, "keep_link_targets", true);
    json_t *r = ph_call(ctx, "strip_markdown", a);
    if (ph_is_raise(r)) return r;
    json_t *o = json_string(ph_str(r) ? ph_str(r) : "");
    json_free(r);
    return o;
}
static char *ph_attachment_label(const char *kind, const char *name) {
    char *o = malloc(strlen(kind) + strlen(name) + 4);
    sprintf(o, "[%s: %s]", kind, name);
    return o;
}
static json_t *ph_attachment_body(const char *space_id, const char *safe_path,
                                  const char *kind, const char *name,
                                  const char *mime_type, const char *caption) {
    json_t *body = json_object();
    ph_set_s(body, "spaceId", space_id);
    ph_set_s(body, "path", safe_path);
    ph_set_s(body, "kind", (kind && strcmp(kind, "voice") == 0) ? "voice" : "attachment");
    if (name && name[0]) ph_set_s(body, "name", name);
    if (mime_type && mime_type[0]) ph_set_s(body, "mimeType", mime_type);
    if (caption && caption[0]) ph_set_s(body, "caption", caption);
    return body;
}
/* ── reaction / retry state helpers (REAL methods; state in ctx->state) ── */
static char *ph_normalize_chat_key(const char *chat_id) {
    char *g = ph_dm_guid_match(chat_id ? chat_id : "");
    if (g) return g;
    return ph_strdup(chat_id ? chat_id : "");
}
static json_t *ph_attr_absent(const char *k) {
    char msg[160];
    snprintf(msg, sizeof msg, "'PhotonAdapter' object has no attribute '%s'", k);
    return ph_err("AttributeError", msg);
}
/* bounded_put seam: record PRE-mutation store snapshot, then mutate in place.
 * Returns a raise descriptor to propagate (no mutation on raise), else NULL. */
static json_t *ph_seam_bounded_put(ph_ctx_t *ctx, json_t *store, const char *key,
                                   const json_t *value, double cap) {
    json_t *a = ph_args();
    ph_set_copy(a, "store", store);
    ph_set_s(a, "key", key);
    ph_set_copy(a, "value", value);
    ph_set_n(a, "cap", cap);
    json_t *r = ph_call(ctx, "bounded_put", a);
    if (ph_is_raise(r)) return r;
    json_free(r);
    ph_bounded_put(store, key, json_copy(value), cap);
    return NULL;
}
static json_t *ph_record_sent_message(ph_ctx_t *ctx, const json_t *message_id) {
    if (!ph_truthy(message_id)) return json_null();
    json_t *store = json_obj_get(ctx->state, "_sent_message_ids");
    if (!store) return ph_attr_absent("_sent_message_ids");
    json_t *t = ph_call(ctx, "time.time", ph_args());
    if (ph_is_raise(t)) return t;
    double cap = ph_state_num(ctx, "_SENT_IDS_MAX", 1000);
    const char *mid = ph_str(message_id);
    json_t *bp = ph_seam_bounded_put(ctx, store, mid ? mid : "", t, cap);
    json_free(t);
    return bp ? bp : json_null();
}
static json_t *ph_put_by_chat(ph_ctx_t *ctx, json_t *store, const char *chat_id,
                              const json_t *value) {
    char *key = ph_normalize_chat_key(chat_id);
    double cap = ph_state_num(ctx, "_LAST_INBOUND_CHATS_MAX", 200);
    json_t *bp = ph_seam_bounded_put(ctx, store, key, value, cap);
    free(key);
    return bp ? bp : json_null();
}
static json_t *ph_record_last_inbound(ph_ctx_t *ctx, const json_t *chat_id,
                                      const json_t *message_id) {
    if (ph_truthy(chat_id) && ph_truthy(message_id)) {
        json_t *store = json_obj_get(ctx->state, "_last_inbound_by_chat");
        if (!store) return ph_attr_absent("_last_inbound_by_chat");
        return ph_put_by_chat(ctx, store, ph_str(chat_id), message_id);
    }
    return json_null();
}
static json_t *ph_record_recent_richlink(ph_ctx_t *ctx, const json_t *chat_id,
                                         const char *text) {
    if (ph_truthy(chat_id)) {
        char *cand = ph_url_only_candidate(text);
        if (cand) {
            json_t *store = json_obj_get(ctx->state, "_recent_richlinks_by_chat");
            if (!store) { free(cand); return ph_attr_absent("_recent_richlinks_by_chat"); }
            json_t *t = ph_call(ctx, "time.time", ph_args());
            if (ph_is_raise(t)) { free(cand); return t; }
            json_t *r = ph_put_by_chat(ctx, store, ph_str(chat_id), t);
            json_free(t);
            free(cand);
            return r;
        }
        free(cand);
    }
    return json_null();
}
static json_t *ph_is_recent_richlink_preview(ph_ctx_t *ctx, const json_t *chat_id,
                                             const json_t *content) {
    if (!ph_truthy(chat_id) || !ph_is_richlink_preview_content(content))
        return json_bool(false);
    char *key = ph_normalize_chat_key(ph_str(chat_id));
    json_t *store = json_obj_get(ctx->state, "_recent_richlinks_by_chat");
    if (!store) { free(key); return ph_attr_absent("_recent_richlinks_by_chat"); }
    const json_t *last = json_obj_get(store, key);
    if (!last || last->type == JSON_NULL) { free(key); return json_bool(false); }
    json_t *t = ph_call(ctx, "time.time", ph_args());
    if (ph_is_raise(t)) { free(key); return t; }
    double now = (t && t->type == JSON_NUMBER) ? t->num_val : 0.0;
    json_free(t);
    double lastv = (last->type == JSON_NUMBER) ? last->num_val : 0.0;
    if (now - lastv > 30.0) {
        json_obj_del(store, key);
        free(key);
        return json_bool(false);
    }
    free(key);
    return json_bool(true);
}
static json_t *ph_reactions_enabled(ph_ctx_t *ctx) {
    json_t *a = ph_args();
    ph_set_s(a, "name", "PHOTON_REACTIONS");
    ph_set_s(a, "default", "false");
    json_t *r = ph_call(ctx, "get_scoped_secret", a);
    if (ph_is_raise(r)) return r;
    const char *s = ph_str(r);
    char *st = ph_strip(s ? s : "");
    char *lw = ph_lower_ascii(st);
    bool on = strcmp(lw, "true") == 0 || strcmp(lw, "1") == 0
           || strcmp(lw, "yes") == 0 || strcmp(lw, "on") == 0;
    free(st); free(lw); json_free(r);
    return json_bool(on);
}
static const char *PH_RETRYABLE_PATTERNS[] = {
    "internal sidecar error", "upstream connect error", "upstream unavailable",
    "connection dropped", "reset reason: overflow", "upstream_overflow",
    "upstream_unavailable"};
static json_t *ph_is_retryable_error(ph_ctx_t *ctx, const json_t *error) {
    json_t *a = ph_args();
    ph_set_copy(a, "error", error);
    json_t *base = ph_call(ctx, "base_is_retryable_error", a);
    if (ph_is_raise(base)) return base;
    bool bret = ph_truthy(base);
    json_free(base);
    if (bret) return json_bool(true);
    const char *e = ph_str(error);
    char *lw = ph_lower_ascii(e ? e : "");
    bool ret = false;
    if (lw[0] && !ph_contains(lw, "retryable=false") && !ph_contains(lw, "auth_or_config")) {
        for (size_t i = 0; i < sizeof(PH_RETRYABLE_PATTERNS) / sizeof(PH_RETRYABLE_PATTERNS[0]); i++)
            if (ph_contains(lw, PH_RETRYABLE_PATTERNS[i])) { ret = true; break; }
    }
    free(lw);
    return json_bool(ret);
}
static bool ph_is_permanent_sidecar_failure(const json_t *result) {
    const json_t *raw = json_obj_get(result, "raw_response");
    if (!raw || raw->type != JSON_OBJECT) return false;
    const json_t *retry = json_obj_get(raw, "retryable");
    if (!(retry && retry->type == JSON_BOOL && retry->bool_val == false)) return false;
    const char *ec = ph_str(json_obj_get(raw, "error_class"));
    return ec && (strcmp(ec, "auth_or_config") == 0 || strcmp(ec, "target_not_allowed") == 0);
}
static bool ph_send_retry_is_final(ph_ctx_t *ctx, const json_t *result) {
    (void)ctx;
    return ph_is_permanent_sidecar_failure(result);
}
/* ── outbound orchestrators (REAL methods) ── */
static json_t *ph_sidecar_try(ph_ctx_t *ctx, const char *path, const json_t *body,
                              const char *what) {
    (void)what;
    json_t *a = ph_args();
    ph_set_s(a, "path", path);
    ph_set_copy(a, "body", body);
    json_t *r = ph_call(ctx, "sidecar_call", a);
    if (ph_is_raise(r)) {
        if (ph_is_base_exc(ph_kind(r))) return r; /* BaseException passes through */
        json_free(r);
        return json_bool(false);
    }
    json_free(r);
    return json_bool(true);
}
static json_t *ph_post_send(ph_ctx_t *ctx, const char *path, const json_t *body,
                            bool structured, const char *sent_text) {
    json_t *a = ph_args();
    ph_set_s(a, "path", path);
    ph_set_copy(a, "body", body);
    json_t *data = ph_call(ctx, "sidecar_call", a);
    if (ph_is_raise(data)) {
        const char *kind = ph_kind(data);
        const char *msg = json_get_str(data, "message", "");
        if (strcmp(kind, "PhotonSidecarError") == 0) {
            json_t *r;
            if (structured) {
                const char *ec = json_get_str(data, "error_class", "sidecar_error");
                bool retry = json_get_bool(data, "retryable", false);
                json_t *raw = json_object();
                ph_set_s(raw, "error_class", ec);
                ph_set_b(raw, "retryable", retry);
                r = ph_send_result(false, NULL, msg, raw, retry);
            } else {
                r = ph_send_result(false, NULL, msg, NULL, false);
            }
            json_free(data);
            return r;
        }
        if (ph_is_base_exc(kind)) return data; /* propagate */
        json_t *r = ph_send_result(false, NULL, msg, NULL, false);
        json_free(data);
        return r;
    }
    const json_t *midn = json_obj_get(data, "messageId");
    json_t *rsm = ph_record_sent_message(ctx, midn);
    if (ph_is_raise(rsm)) { json_free(data); return rsm; }
    json_free(rsm);
    if (sent_text && sent_text[0]) {
        json_t *a2 = ph_args();
        ph_set_copy(a2, "chat_id", json_obj_get(body, "spaceId"));
        ph_set_copy(a2, "message_id", midn);
        ph_set_s(a2, "text", sent_text);
        json_t *r2 = ph_call(ctx, "record_sent_text_async", a2);
        if (ph_is_raise(r2)) { json_free(data); return r2; }
        json_free(r2);
    }
    json_t *r = ph_send_result(true, ph_str(midn), NULL, NULL, false);
    json_free(data);
    return r;
}
static json_t *ph_sidecar_send_poll(ph_ctx_t *ctx, const char *space_id,
                                    const json_t *title, const json_t *options) {
    /* opts = [str(o).strip() for o in (options or []) if str(o).strip()] */
    json_t *opts = json_array();
    if (options && options->type == JSON_ARRAY) {
        for (size_t i = 0; i < options->c.count; i++) {
            char *s = ph_pystr(options->c.items[i]);
            char *st = ph_strip(s);
            if (st[0]) json_append(opts, json_string(st));
            free(s); free(st);
        }
    }
    double maxl = ph_state_num(ctx, "MAX_MESSAGE_LENGTH", 8000);
    char *tstrip = ph_strip(ph_str(title) ? ph_str(title) : "");
    if (!ph_truthy(title) || !tstrip[0]) {
        free(tstrip); json_free(opts);
        return ph_send_result(false, NULL, "poll title is required", NULL, false);
    }
    if (opts->c.count < 2) {
        free(tstrip); json_free(opts);
        return ph_send_result(false, NULL, "poll needs at least two options", NULL, false);
    }
    char *ttrunc = ph_utf8_slice(tstrip, (size_t)maxl);
    free(tstrip);
    json_t *body = json_object();
    ph_set_s(body, "spaceId", space_id);
    ph_set_s(body, "title", ttrunc);
    json_set(body, "options", opts);
    json_t *r = ph_post_send(ctx, "/send-poll", body, false, ttrunc);
    free(ttrunc); json_free(body);
    return r;
}
static json_t *ph_sidecar_send_attachment(ph_ctx_t *ctx, const char *space_id,
                                          const json_t *path, const char *name,
                                          const char *mime_type, const char *caption,
                                          const char *kind) {
    char *path_str = ph_pystr(path);
    json_t *a = ph_args();
    ph_set_s(a, "path", path_str);
    ph_set_s(a, "session_key", "");
    json_t *safe = ph_call(ctx, "validate_media_delivery_path", a);
    if (ph_is_raise(safe)) { free(path_str); return safe; }
    const char *safe_str = ph_str(safe);
    if (!ph_truthy(safe)) {
        char *err = malloc(strlen(path_str) + 48);
        sprintf(err, "unsafe or missing attachment path: %s", path_str);
        json_t *r = ph_send_result(false, NULL, err, NULL, false);
        free(err); free(path_str); json_free(safe);
        return r;
    }
    /* mime_type or _guess_mime(safe_path) */
    const char *mime_final = mime_type;
    json_t *guess = NULL;
    if (!mime_final || !mime_final[0]) {
        json_t *ga = ph_args();
        ph_set_s(ga, "path", safe_str);
        guess = ph_call(ctx, "_guess_mime", ga);
        if (ph_is_raise(guess)) { free(path_str); json_free(safe); return guess; }
        mime_final = ph_str(guess);
    }
    json_t *body = ph_attachment_body(space_id, safe_str, kind, name, mime_final, caption);
    /* label = caption or _attachment_label(kind, name or basename(safe_path)) */
    char *label = NULL;
    if (caption && caption[0]) {
        label = ph_strdup(caption);
    } else {
        char *base = ph_basename(safe_str);
        const char *nm = (name && name[0]) ? name : base;
        label = ph_attachment_label(kind ? kind : "attachment", nm);
        free(base);
    }
    json_t *r = ph_post_send(ctx, "/send-attachment", body, true, label);
    free(label); free(path_str); json_free(body); json_free(safe);
    if (guess) json_free(guess);
    return r;
}
static json_t *ph_sidecar_send(ph_ctx_t *ctx, const char *space_id, const char *text,
                               bool richlink, bool markdown) {
    const char *sent_text = text ? text : "";
    json_t *ru = richlink ? ph_richlink_candidate(ctx, text) : json_null();
    if (ph_is_raise(ru)) return ru;
    const char *rich_url = ph_str(ru);
    if (rich_url && rich_url[0]) {
        json_t *body = json_object();
        ph_set_s(body, "spaceId", space_id);
        ph_set_s(body, "url", rich_url);
        json_t *rr = ph_post_send(ctx, "/send-richlink", body, false, sent_text);
        json_free(body);
        if (ph_is_raise(rr)) { json_free(ru); return rr; }
        if (ph_truthy(json_obj_get(rr, "success"))) { json_free(ru); return rr; }
        json_free(rr);
        markdown = false;
    }
    json_free(ru);
    json_t *me = markdown ? ph_markdown_enabled(ctx) : json_bool(false);
    if (ph_is_raise(me)) return me;
    bool send_markdown = ph_truthy(me);
    json_free(me);
    json_t *pt = ph_sidecar_payload_text(ctx, text, send_markdown);
    if (ph_is_raise(pt)) return pt;
    char *text2 = ph_strdup(ph_str(pt) ? ph_str(pt) : "");
    json_free(pt);
    double maxl = ph_state_num(ctx, "MAX_MESSAGE_LENGTH", 8000);
    if (ph_utf8_len(text2) > (size_t)maxl) {
        char *tr = ph_utf8_slice(text2, (size_t)maxl);
        free(text2); text2 = tr;
    }
    json_t *body = json_object();
    ph_set_s(body, "spaceId", space_id);
    ph_set_s(body, "text", text2);
    if (send_markdown) ph_set_s(body, "format", "markdown");
    json_t *r = ph_post_send(ctx, "/send", body, true, sent_text);
    free(text2); json_free(body);
    return r;
}
/* ── case-target wrappers (uniform ph__name(ctx, args) signature) ── */
static const char *ph_arg_str(const json_t *a, const char *k, const char *def) {
    return json_get_str(a, k, def);
}
static const json_t *ph_arg_node(const json_t *a, const char *k) {
    return json_obj_get(a, k);
}
static bool ph_arg_bool(const json_t *a, const char *k, bool def) {
    return json_get_bool(a, k, def);
}
static json_t *ph_str_ret(char *s) {
    if (!s) return json_null();
    json_t *r = json_string(s);
    free(s);
    return r;
}
/* PoP: ph__normalize_content @ plugins/platforms/photon/adapter.py:_normalize_content */
static json_t *ph__normalize_content(ph_ctx_t *ctx, const json_t *args) {
    return ph_normalize_content(ctx, ph_arg_node(args, "content"));
}
/* PoP: ph__normalize_group_content @ plugins/platforms/photon/adapter.py:_normalize_group_content */
static json_t *ph__normalize_group_content(ph_ctx_t *ctx, const json_t *args) {
    return ph_normalize_group_content(ctx, ph_arg_node(args, "content"));
}
/* PoP: ph__normalize_binary_payload @ plugins/platforms/photon/adapter.py:_normalize_binary_payload */
static json_t *ph__normalize_binary_payload(ph_ctx_t *ctx, const json_t *args) {
    return ph_normalize_binary_payload(ctx, ph_arg_node(args, "payload"));
}
/* PoP: ph__is_voice_payload @ plugins/platforms/photon/adapter.py:_is_voice_payload */
static json_t *ph__is_voice_payload(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx; return json_bool(ph_is_voice_payload(ph_arg_node(args, "payload")));
}
/* PoP: ph__cached_media_type @ plugins/platforms/photon/adapter.py:_cached_media_type */
static json_t *ph__cached_media_type(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return ph_str_ret(ph_cached_media_type(ph_arg_node(args, "payload"),
                                         ph_arg_str(args, "mime", ""),
                                         ph_arg_bool(args, "is_voice", false)));
}
/* PoP: ph__attachment_message_type @ plugins/platforms/photon/adapter.py:_attachment_message_type */
static json_t *ph__attachment_message_type(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx; return json_string(ph_attachment_message_type(ph_arg_str(args, "mime", "")));
}
/* PoP: ph__format_richlink_content @ plugins/platforms/photon/adapter.py:_format_richlink_content */
static json_t *ph__format_richlink_content(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx; return ph_str_ret(ph_format_richlink_content(ph_arg_node(args, "content")));
}
/* PoP: ph__group_item_contents @ plugins/platforms/photon/adapter.py:_group_item_contents */
static json_t *ph__group_item_contents(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx; return ph_group_item_contents(ph_arg_node(args, "content"));
}
/* PoP: ph__reply_inner @ plugins/platforms/photon/adapter.py:_reply_inner */
static json_t *ph__reply_inner(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx; return json_copy(ph_reply_inner(ph_arg_node(args, "content")));
}
/* PoP: ph__carries_voice @ plugins/platforms/photon/adapter.py:_carries_voice */
static json_t *ph__carries_voice(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx; return json_bool(ph_carries_voice(ph_arg_node(args, "content")));
}
/* PoP: ph__mention_gate_text @ plugins/platforms/photon/adapter.py:_mention_gate_text */
static json_t *ph__mention_gate_text(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx; return ph_str_ret(ph_mention_gate_text(ph_arg_node(args, "content")));
}
/* PoP: ph__richlink_url_from_content @ plugins/platforms/photon/adapter.py:_richlink_url_from_content */
static json_t *ph__richlink_url_from_content(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx; return ph_str_ret(ph_richlink_url_from_content(ph_arg_node(args, "content")));
}
/* PoP: ph__url_only_candidate @ plugins/platforms/photon/adapter.py:_url_only_candidate */
static json_t *ph__url_only_candidate(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx; return ph_str_ret(ph_url_only_candidate(ph_arg_str(args, "text", "")));
}
/* PoP: ph__is_richlink_preview_attachment @ plugins/platforms/photon/adapter.py:_is_richlink_preview_attachment */
static json_t *ph__is_richlink_preview_attachment(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx; return json_bool(ph_is_richlink_preview_attachment(ph_arg_node(args, "payload")));
}
/* PoP: ph__richlink_preview_label @ plugins/platforms/photon/adapter.py:_richlink_preview_label */
static json_t *ph__richlink_preview_label(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx; return ph_str_ret(ph_richlink_preview_label(ph_arg_node(args, "content")));
}
/* PoP: ph__is_richlink_preview_content @ plugins/platforms/photon/adapter.py:_is_richlink_preview_content */
static json_t *ph__is_richlink_preview_content(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx; return json_bool(ph_is_richlink_preview_content(ph_arg_node(args, "content")));
}
/* PoP: ph__markdown_enabled @ plugins/platforms/photon/adapter.py:_markdown_enabled */
static json_t *ph__markdown_enabled(ph_ctx_t *ctx, const json_t *args) {
    (void)args; return ph_markdown_enabled(ctx);
}
/* PoP: ph__richlink_candidate @ plugins/platforms/photon/adapter.py:_richlink_candidate */
static json_t *ph__richlink_candidate(ph_ctx_t *ctx, const json_t *args) {
    return ph_richlink_candidate(ctx, ph_arg_str(args, "text", ""));
}
/* PoP: ph__sidecar_payload_text @ plugins/platforms/photon/adapter.py:_sidecar_payload_text */
static json_t *ph__sidecar_payload_text(ph_ctx_t *ctx, const json_t *args) {
    return ph_sidecar_payload_text(ctx, ph_arg_str(args, "text", ""),
                                   ph_arg_bool(args, "send_markdown", false));
}
/* PoP: ph__attachment_label @ plugins/platforms/photon/adapter.py:_attachment_label */
static json_t *ph__attachment_label(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx; return ph_str_ret(ph_attachment_label(ph_arg_str(args, "kind", ""),
                                                     ph_arg_str(args, "name", "")));
}
/* PoP: ph__attachment_body @ plugins/platforms/photon/adapter.py:_attachment_body */
static json_t *ph__attachment_body(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return ph_attachment_body(ph_arg_str(args, "space_id", ""), ph_arg_str(args, "safe_path", ""),
                              ph_arg_str(args, "kind", "attachment"), ph_arg_str(args, "name", NULL),
                              ph_arg_str(args, "mime_type", NULL), ph_arg_str(args, "caption", NULL));
}
/* PoP: ph__normalize_chat_key @ plugins/platforms/photon/adapter.py:_normalize_chat_key */
static json_t *ph__normalize_chat_key(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx; return ph_str_ret(ph_normalize_chat_key(ph_arg_str(args, "chat_id", "")));
}
/* PoP: ph__record_sent_message @ plugins/platforms/photon/adapter.py:_record_sent_message */
static json_t *ph__record_sent_message(ph_ctx_t *ctx, const json_t *args) {
    return ph_record_sent_message(ctx, ph_arg_node(args, "message_id"));
}
/* PoP: ph__put_by_chat @ plugins/platforms/photon/adapter.py:_put_by_chat */
static json_t *ph__put_by_chat(ph_ctx_t *ctx, const json_t *args) {
    /* the generic store arg travels in args; echo its mutation as state.store */
    const json_t *src = ph_arg_node(args, "store");
    json_t *store = (src && src->type == JSON_OBJECT) ? json_copy(src) : json_object();
    json_set(ctx->state, "store", store);
    return ph_put_by_chat(ctx, store, ph_arg_str(args, "chat_id", ""), ph_arg_node(args, "value"));
}
/* PoP: ph__record_last_inbound @ plugins/platforms/photon/adapter.py:_record_last_inbound */
static json_t *ph__record_last_inbound(ph_ctx_t *ctx, const json_t *args) {
    return ph_record_last_inbound(ctx, ph_arg_node(args, "chat_id"), ph_arg_node(args, "message_id"));
}
/* PoP: ph__record_recent_richlink @ plugins/platforms/photon/adapter.py:_record_recent_richlink */
static json_t *ph__record_recent_richlink(ph_ctx_t *ctx, const json_t *args) {
    return ph_record_recent_richlink(ctx, ph_arg_node(args, "chat_id"), ph_arg_str(args, "text", ""));
}
/* PoP: ph__is_recent_richlink_preview @ plugins/platforms/photon/adapter.py:_is_recent_richlink_preview */
static json_t *ph__is_recent_richlink_preview(ph_ctx_t *ctx, const json_t *args) {
    return ph_is_recent_richlink_preview(ctx, ph_arg_node(args, "chat_id"), ph_arg_node(args, "content"));
}
/* PoP: ph__reactions_enabled @ plugins/platforms/photon/adapter.py:_reactions_enabled */
static json_t *ph__reactions_enabled(ph_ctx_t *ctx, const json_t *args) {
    (void)args; return ph_reactions_enabled(ctx);
}
/* PoP: ph__is_retryable_error @ plugins/platforms/photon/adapter.py:_is_retryable_error */
static json_t *ph__is_retryable_error(ph_ctx_t *ctx, const json_t *args) {
    return ph_is_retryable_error(ctx, ph_arg_node(args, "error"));
}
/* PoP: ph__is_permanent_sidecar_failure @ plugins/platforms/photon/adapter.py:_is_permanent_sidecar_failure */
static json_t *ph__is_permanent_sidecar_failure(ph_ctx_t *ctx, const json_t *args) {
    (void)ctx; return json_bool(ph_is_permanent_sidecar_failure(ph_arg_node(args, "result")));
}
/* PoP: ph__send_retry_is_final @ plugins/platforms/photon/adapter.py:_send_retry_is_final */
static json_t *ph__send_retry_is_final(ph_ctx_t *ctx, const json_t *args) {
    return json_bool(ph_send_retry_is_final(ctx, ph_arg_node(args, "result")));
}
/* PoP: ph__sidecar_try @ plugins/platforms/photon/adapter.py:_sidecar_try */
static json_t *ph__sidecar_try(ph_ctx_t *ctx, const json_t *args) {
    return ph_sidecar_try(ctx, ph_arg_str(args, "path", ""), ph_arg_node(args, "body"),
                          ph_arg_str(args, "what", ""));
}
/* PoP: ph__post_send @ plugins/platforms/photon/adapter.py:_post_send */
static json_t *ph__post_send(ph_ctx_t *ctx, const json_t *args) {
    return ph_post_send(ctx, ph_arg_str(args, "path", ""), ph_arg_node(args, "body"),
                        ph_arg_bool(args, "structured", false), ph_arg_str(args, "sent_text", NULL));
}
/* PoP: ph__sidecar_send_poll @ plugins/platforms/photon/adapter.py:_sidecar_send_poll */
static json_t *ph__sidecar_send_poll(ph_ctx_t *ctx, const json_t *args) {
    return ph_sidecar_send_poll(ctx, ph_arg_str(args, "space_id", ""),
                                ph_arg_node(args, "title"), ph_arg_node(args, "options"));
}
/* PoP: ph__sidecar_send_attachment @ plugins/platforms/photon/adapter.py:_sidecar_send_attachment */
static json_t *ph__sidecar_send_attachment(ph_ctx_t *ctx, const json_t *args) {
    return ph_sidecar_send_attachment(ctx, ph_arg_str(args, "space_id", ""),
                                      ph_arg_node(args, "path"), ph_arg_str(args, "name", NULL),
                                      ph_arg_str(args, "mime_type", NULL), ph_arg_str(args, "caption", NULL),
                                      ph_arg_str(args, "kind", "attachment"));
}
/* PoP: ph__sidecar_send @ plugins/platforms/photon/adapter.py:_sidecar_send */
static json_t *ph__sidecar_send(ph_ctx_t *ctx, const json_t *args) {
    return ph_sidecar_send(ctx, ph_arg_str(args, "space_id", ""), ph_arg_str(args, "text", ""),
                           ph_arg_bool(args, "richlink", true), ph_arg_bool(args, "markdown", true));
}