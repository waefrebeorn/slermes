/*
 * whatsapp_adapter_flow.c -- WhatsApp adapter bridge-lifecycle FLOW cores and
 * leaves of plugins/platforms/whatsapp/adapter.py (hermes-agent).
 *
 * Ports the DECISION SEQUENCES of the bridge process-management tree:
 * port-listener discovery (netstat / lsof / ss / psutil), the fail-closed
 * node-bridge identity check, the SIGTERM->grace->SIGKILL kill flows
 * (_kill_port_process, _kill_stale_bridge_by_pidfile), the pidfile /
 * fingerprint / token file readers+writers, the spawn wrapper, the
 * requirement check, and the pure leaves (int/float coercion, uptime
 * formatting, sha256/hmac digests, media-type classification, mention
 * dedup). Call order, short-circuit branches, suppress() regions,
 * early returns and exception propagation mirror the live Python.
 *
 * Universal seam: every SEAMED sibling call enters through ONE callback
 * wa_call(ctx, name, args). The harness wires ctx->io to a recording stub
 * fed the case's scripted results; the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts. args is ADOPTED into the trace event; the return is a NEW owned
 * json_t (NULL == Python None; {"__raise__":cls,"message":str(exc)} == a
 * raised exception). A missing/exhausted script is a fail-closed
 * RuntimeError on both sides. BaseException kinds (SystemExit /
 * KeyboardInterrupt / CancelledError / GeneratorExit) pass through EVERY
 * suppress()/except site.
 *
 * Mode table (how a function is invoked BY ITS CALLERS):
 *  REAL-ALWAYS (run inline, direct C call, never seamed): _safe_ints,
 *    _fmt_uptime, _bridge_auth_headers, _bridge_media_type,
 *    _bridge_auth_proof, _bridge_config_fingerprint,
 *    _classify_bridge_message.
 *  REAL-TARGET / SEAMED-CALLER (wa_call when called by another ported
 *    function; real body when the case target): _wenv, _is_connected,
 *    _cache_dirs, _unlink_quietly, _tighten_bridge_log, _read_bridge_token,
 *    _write_bridge_fingerprint, _write_bridge_pidfile, _read_bridge_pidfile,
 *    _bridge_pid_is_ours, _pids_alive, _wait_pids_exit,
 *    _pid_looks_like_node_bridge, _windows_listener_pids,
 *    _listener_pids_on_port, _port_listener_pids, _kill_port_process,
 *    _kill_stale_bridge_by_pidfile, check_whatsapp_requirements,
 *    _file_content_hash, _bridge_source_hash, _spawn_bridge_process,
 *    _normalize_outbound_mentions.
 *  SEAMED foreign calls: subprocess.run, windows_hide_flags,
 *    windows_detach_flags, psutil.Process, psutil.net_connections,
 *    _pid_exists, get_process_start_time (gateway.status), time.monotonic,
 *    time.sleep, os.kill, chmod (os.chmod), logger.warning, logger.info,
 *    find_node_executable, with_hermes_node_path, lazy_installs_allowed,
 *    get_scoped_secret, get_env_value, spawn_owned,
 *    normalize_whatsapp_mention_jid, the four cache-dir getters, and the
 *    PathDouble file ops exists / read_text / write_text / unlink /
 *    read_bytes.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and wa_-prefixed.
 *
 * Representation contract (shared with tests/oracle_whatsapp_adapter_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__": cls, "message": str(exc)}.
 * - MessageType members project to their .value strings ("text", "photo",
 *   "video", "voice", "audio", "document", "location", "sticker").
 * - Paths are NORMALIZED POSIX strings (no trailing slash except root, no
 *   "//" or "." components); the oracle drives pathlib.PurePosixPath
 *   doubles (fixture domain excludes drive-letter case folding and
 *   resolve(); joins are lexical).
 *   read_text seams all use encoding="utf-8-sig": one leading BOM
 *   (EF BB BF) is stripped from the scripted string on BOTH sides.
 *   read_bytes seam results are UTF-8 strings (hashed as their bytes).
 * - subprocess.run / psutil.Process / net_connections results are
 *   attribute doubles: JSON objects whose keys are the live attributes
 *   (stdout / returncode / name / cmdline / status / laddr / pid). A
 *   missing key models a missing attribute (AttributeError with the
 *   py3.14 'types.SimpleNamespace' prefix); null models None ('NoneType').
 * - _IS_WINDOWS is fixture-driven (args.is_windows) for _port_listener_pids,
 *   _kill_port_process and _kill_stale_bridge_by_pidfile; signal constants
 *   are modeled: SIGTERM=15, _SIGKILL = 9 (POSIX) / 15 (Windows, no
 *   SIGKILL). The oracle overrides ns["_IS_WINDOWS"] / ns["_SIGKILL"]
 *   per case to match.
 * - Integral floats == ints (libjson %.0f; oracle canon folds integral
 *   floats to int). Tuples project to lists.
 * - Every ported function emits state {} (all observable effects ride the
 *   seam trace; no by-reference accumulators in this cluster).
 *
 * Fixture corpus restrictions (documented; generator honours them):
 * - str()-computed positions (_bridge_config_fingerprint env values,
 *   _bridge_media_type / _classify_bridge_message mediaType, pidfile
 *   start/port) carry str / int / bool / None only -- no containers, no
 *   non-integral floats (Python str(5.0)=="5.0" vs C %.0f=="5").
 * - int()/float() string tokens: ASCII digits / sign / underscore / dot /
 *   exponent, plus Unicode White_Space padding (\xa0, \x85, U+2000);
 *   INVALID tokens are printable ASCII without quotes/backslashes so the
 *   ValueError repr is the plain single-quoted token. \x1c-\x1f are NOT
 *   int/float whitespace (they ARE str.strip/split whitespace).
 * - int values fit |v| < 2^53; _fmt_uptime seconds fit the same.
 * - logger format args carry the raw values (no %s/%d transformation).
 * - psutil import always succeeds (ImportError branch out of domain);
 *   net_connections entries are objects or null; cmdline elements are str.
 * - token/challenge for _bridge_auth_proof are UTF-8 strings (hashed as
 *   bytes); mention seam results are str / null / "".
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- universal seam context -- */
typedef struct wa_ctx {
    json_t *state;      /* owned; emitted as out.state (always {} here) */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} wa_ctx_t;

/* -- basic helpers -- */
static bool wa_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *wa_kind(const json_t *v) {
    const json_t *k = v ? json_obj_get(v, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static bool wa_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
/* Exception-kind hierarchy for suppress()/except matching. */
static bool wa_kind_is(const char *kind, const char *base) {
    if (strcmp(kind, base) == 0) return true;
    if (strcmp(base, "OSError") == 0) {
        return strcmp(kind, "FileNotFoundError") == 0 || strcmp(kind, "PermissionError") == 0
            || strcmp(kind, "ProcessLookupError") == 0 || strcmp(kind, "TimeoutError") == 0
            || strcmp(kind, "ConnectionError") == 0 || strcmp(kind, "ConnectionRefusedError") == 0
            || strcmp(kind, "InterruptedError") == 0 || strcmp(kind, "BlockingIOError") == 0;
    }
    if (strcmp(base, "SubprocessError") == 0) {
        return strcmp(kind, "TimeoutExpired") == 0 || strcmp(kind, "CalledProcessError") == 0;
    }
    if (strcmp(base, "Exception") == 0) return !wa_is_base_exc(kind);
    return false;
}
/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool wa_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *wa_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *wa_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *wa_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return wa_err(type, msg);
}
/* The one universal seam. args is ADOPTED into the trace event; the return
 * is a fresh owned node (scripted value copy or raise descriptor). */
static json_t *wa_call(wa_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = wa_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = wa_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = wa_make_exc(raise);
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
static json_t *wa_args(void) { return json_object(); }
static void wa_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void wa_set_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void wa_set_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void wa_set_null(json_t *a, const char *k) { json_set(a, k, json_null()); }
static void wa_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* -- string / utf-8 helpers -- */
static char *wa_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static uint32_t *wa_utf8_decode(const char *s, size_t *n) {
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
static void wa_utf8_putcp(char **q, uint32_t cp) {
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
static char *wa_utf8_encode(const uint32_t *cps, size_t n) {
    char *o = malloc(n * 4 + 1);
    if (!o) return NULL;
    char *q = o;
    for (size_t i = 0; i < n; i++) wa_utf8_putcp(&q, cps[i]);
    *q = '\0';
    return o;
}
/* Python str.strip() White_Space set. */
static bool wa_is_ws_cp(uint32_t c) {
    if (c == 0x20 || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F)) return true;
    if (c == 0x85 || c == 0xA0 || c == 0x1680) return true;
    if (c >= 0x2000 && c <= 0x200A) return true;
    if (c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000) return true;
    return false;
}
/* int()/float() strip set: White_Space MINUS \x1c-\x1f (probed on 3.14.9). */
static bool wa_is_num_ws_cp(uint32_t c) {
    if (c >= 0x1C && c <= 0x1F) return false;
    return wa_is_ws_cp(c);
}
static char *wa_strip(const char *s) {
    size_t n = 0;
    uint32_t *cps = wa_utf8_decode(s, &n);
    size_t a = 0, b = n;
    while (a < b && wa_is_ws_cp(cps[a])) a++;
    while (b > a && wa_is_ws_cp(cps[b-1])) b--;
    char *o = wa_utf8_encode(cps + a, b - a);
    free(cps);
    return o;
}
static char *wa_strip_num(const char *s) {
    size_t n = 0;
    uint32_t *cps = wa_utf8_decode(s, &n);
    size_t a = 0, b = n;
    while (a < b && wa_is_num_ws_cp(cps[a])) a++;
    while (b > a && wa_is_num_ws_cp(cps[b-1])) b--;
    char *o = wa_utf8_encode(cps + a, b - a);
    free(cps);
    return o;
}
/* ASCII lower (fixture corpus for .lower() positions is ASCII). */
static char *wa_lower_ascii(const char *s) {
    char *o = wa_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
/* str.splitlines(): ASCII boundaries \n \r \r\n \v \f \x1c \x1d \x1e
 * (NOT \x1f); corpus is ASCII + those controls. Returns malloc'd parts. */
static char **wa_splitlines(const char *s, size_t *n) {
    if (!s) s = "";
    size_t len = strlen(s), cap = 8, k = 0;
    char **parts = malloc(cap * sizeof(char *));
    size_t start = 0;
    for (size_t i = 0; i < len;) {
        unsigned char c = (unsigned char)s[i];
        bool brk = (c == '\n' || c == '\r' || c == '\v' || c == '\f'
                    || c == 0x1C || c == 0x1D || c == 0x1E);
        if (brk) {
            if (k == cap) { cap *= 2; parts = realloc(parts, cap * sizeof(char *)); }
            parts[k] = malloc(i - start + 1);
            memcpy(parts[k], s + start, i - start);
            parts[k][i - start] = '\0';
            k++;
            if (c == '\r' && i + 1 < len && s[i+1] == '\n') i++;
            i++;
            start = i;
        } else {
            i++;
        }
    }
    if (start < len) {
        if (k == cap) { cap *= 2; parts = realloc(parts, cap * sizeof(char *)); }
        parts[k] = malloc(len - start + 1);
        memcpy(parts[k], s + start, len - start);
        parts[k][len - start] = '\0';
        k++;
    }
    *n = k;
    return parts;
}
/* str.split() with no arg: runs of White_Space codepoints (incl \x1c-\x1f),
 * no empties. Codepoint-level (a byte scan would cut multi-byte chars). */
static char **wa_split_ws(const char *s, size_t *n) {
    size_t nc = 0;
    uint32_t *cps = wa_utf8_decode(s, &nc);
    size_t cap = 8, k = 0;
    char **parts = malloc(cap * sizeof(char *));
    size_t i = 0;
    while (i < nc) {
        while (i < nc && wa_is_ws_cp(cps[i])) i++;
        if (i >= nc) break;
        size_t start = i;
        while (i < nc && !wa_is_ws_cp(cps[i])) i++;
        if (k == cap) { cap *= 2; parts = realloc(parts, cap * sizeof(char *)); }
        parts[k] = wa_utf8_encode(cps + start, i - start);
        k++;
    }
    free(cps);
    *n = k;
    return parts;
}
static void wa_free_parts(char **parts, size_t n) {
    for (size_t i = 0; i < n; i++) free(parts[i]);
    free(parts);
}
/* Lexical POSIX path join (PurePosixPath model; fixture domain: no "." /
 * ".." / double-slash components). Trailing slashes on base collapse. */
static char *wa_path_join(const char *base, const char *name) {
    if (!base) base = "";
    size_t bl = strlen(base);
    while (bl > 1 && base[bl-1] == '/') bl--;
    if (bl == 1 && base[0] == '/') {
        char *o = malloc(strlen(name) + 2);
        o[0] = '/';
        strcpy(o + 1, name);
        return o;
    }
    if (bl == 0) return wa_strdup(name);
    char *o = malloc(bl + strlen(name) + 2);
    memcpy(o, base, bl);
    o[bl] = '/';
    strcpy(o + bl + 1, name);
    return o;
}
/* PurePosixPath.name: last component ("" for "/"). */
static char *wa_path_name(const char *p) {
    if (!p) p = "";
    size_t len = strlen(p);
    while (len > 1 && p[len-1] == '/') len--;
    if (len == 1 && p[0] == '/') return wa_strdup("");
    size_t start = len;
    while (start > 0 && p[start-1] != '/') start--;
    char *o = malloc(len - start + 1);
    memcpy(o, p + start, len - start);
    o[len - start] = '\0';
    return o;
}
/* PurePosixPath.with_name: replace the last component. */
static char *wa_with_name(const char *p, const char *name) {
    if (!p) p = "";
    size_t len = strlen(p);
    while (len > 1 && p[len-1] == '/') len--;
    size_t start = len;
    while (start > 0 && p[start-1] != '/') start--;
    char *o = malloc(start + strlen(name) + 1);
    memcpy(o, p, start);
    strcpy(o + start, name);
    return o;
}
/* genericpath._splitext(p, '\\', '/', '.') -- ntpath on the Windows gate
 * host. Returns the extension (incl dot) or ""; transcribed verbatim. */
static char *wa_splitext(const char *p) {
    if (!p) p = "";
    size_t len = strlen(p);
    long sepIndex = -1, altsepIndex = -1, dotIndex = -1;
    for (long i = (long)len - 1; i >= 0; i--) {
        if (p[i] == '\\' && sepIndex < 0) sepIndex = i;
        if (p[i] == '/' && altsepIndex < 0) altsepIndex = i;
        if (p[i] == '.' && dotIndex < 0) dotIndex = i;
    }
    if (altsepIndex > sepIndex) sepIndex = altsepIndex;
    if (dotIndex > sepIndex) {
        long filenameIndex = sepIndex + 1;
        while (filenameIndex < dotIndex) {
            if (p[filenameIndex] != '.') {
                return wa_strdup(p + dotIndex);
            }
            filenameIndex++;
        }
    }
    return wa_strdup("");
}
/* Python str() projection for the fixture domain (str/int/bool/None). */
static char *wa_str_proj(const json_t *v) {
    if (!v || v->type == JSON_NULL) return wa_strdup("None");
    if (v->type == JSON_BOOL) return wa_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof buf, "%.0f", v->num_val);
        return wa_strdup(buf);
    }
    if (v->type == JSON_STRING) return wa_strdup(v->str_val ? v->str_val : "");
    return wa_strdup("");  /* containers out of domain */
}
/* Python type name for error messages (integral double -> int). */
static const char *wa_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val) && fabs(v->num_val) < 9.0e15) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
/* Attribute read on a scripted double: borrowed node, or *err set to the
 * exact py3.14 AttributeError (null -> NoneType, object missing key ->
 * types.SimpleNamespace, other scalar -> its type name). */
static const json_t *wa_attr(const json_t *obj, const char *field, json_t **err) {
    *err = NULL;
    if (!obj || obj->type == JSON_NULL) {
        char msg[256];
        snprintf(msg, sizeof msg, "'NoneType' object has no attribute '%s'", field);
        *err = wa_err("AttributeError", msg);
        return NULL;
    }
    if (obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, field);
        if (!v) {
            char msg[256];
            snprintf(msg, sizeof msg, "'types.SimpleNamespace' object has no attribute '%s'", field);
            *err = wa_err("AttributeError", msg);
            return NULL;
        }
        return v;
    }
    char msg[256];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", wa_type_name(obj), field);
    *err = wa_err("AttributeError", msg);
    return NULL;
}
/* Strict Python == for the fixture domain (int/float/str/bool/null).
 * bool==int admitted (True==1); str vs number never equal. */
static bool wa_py_eq(const json_t *a, const json_t *b) {
    bool an = (!a || a->type == JSON_NULL), bn = (!b || b->type == JSON_NULL);
    if (an || bn) return an && bn;
    bool anum = (a->type == JSON_NUMBER || a->type == JSON_BOOL);
    bool bnum = (b->type == JSON_NUMBER || b->type == JSON_BOOL);
    if (anum && bnum) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return false;
}

/* -- numeric coercion (Python int() / float() semantics, ASCII corpus) -- */
/* int(v): number -> trunc toward zero; bool -> 1/0; str -> strip (num WS
 * set), optional sign, digits with '_' between digits only; null/container
 * -> TypeError; bad str -> ValueError quoting the ORIGINAL token. */
static json_t *wa_int_parse(const json_t *v) {
    if (v && v->type == JSON_NUMBER) return json_number(trunc(v->num_val));
    if (v && v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    const char *raw = wa_str(v);
    if (!raw) {
        char msg[256];
        snprintf(msg, sizeof msg,
                 "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                 wa_type_name(v));
        return wa_err("TypeError", msg);
    }
    char *s = wa_strip_num(raw);
    const char *p = s;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    const char *q = p;
    size_t ndigits = 0;
    bool ok = true;
    while (*q) {
        if (*q >= '0' && *q <= '9') { ndigits++; q++; }
        else if (*q == '_') {
            if (q == p || !(q[1] >= '0' && q[1] <= '9')) { ok = false; break; }
            q++;
        } else { ok = false; break; }
    }
    if (!ok || ndigits == 0) {
        free(s);
        char msg[512];
        snprintf(msg, sizeof msg, "invalid literal for int() with base 10: '%s'", raw);
        return wa_err("ValueError", msg);
    }
    char clean[128];
    size_t ck = 0;
    for (const char *t = p; *t; t++)
        if (*t != '_' && ck < sizeof clean - 1) clean[ck++] = *t;
    clean[ck] = '\0';
    long long val = strtoll(clean, NULL, 10);
    free(s);
    return json_number(neg ? -(double)val : (double)val);
}
/* Validate one digits/underscore run starting at *pp; advances *pp.
 * Every '_' needs a digit on both sides (run-start '_' rejected). */
static bool wa_num_run(const char **pp, size_t *ndigits) {
    const char *rs = *pp, *q = rs;
    bool ok = true;
    while (*q) {
        if (*q >= '0' && *q <= '9') { (*ndigits)++; q++; }
        else if (*q == '_') {
            if (q == rs || !(q[1] >= '0' && q[1] <= '9')) { ok = false; break; }
            q++;
        } else break;
    }
    *pp = q;
    return ok;
}
/* float(v): *special gets 0 normal / +1 +inf / -1 -inf / 2 nan.
 * Returns a number node (placeholder 0 for specials) or a raise spec. */
static json_t *wa_float_parse(const json_t *v, int *special) {
    *special = 0;
    if (v && v->type == JSON_NUMBER) return json_number(v->num_val);
    if (v && v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    const char *raw = wa_str(v);
    if (!raw) {
        char msg[256];
        snprintf(msg, sizeof msg, "float() argument must be a string or a real number, not '%s'",
                 wa_type_name(v));
        return wa_err("TypeError", msg);
    }
    char *s = wa_strip_num(raw);
    const char *p = s;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    /* inf / infinity / nan (case-insensitive, full remainder) */
    char low[32];
    size_t ll = 0;
    for (const char *t = p; *t && ll < sizeof low - 1; t++) low[ll++] = (char)tolower((unsigned char)*t);
    low[ll] = '\0';
    if (strcmp(low, "inf") == 0 || strcmp(low, "infinity") == 0) {
        free(s);
        *special = neg ? -1 : 1;
        return json_number(0.0);
    }
    if (strcmp(low, "nan") == 0) {
        free(s);
        *special = 2;
        return json_number(0.0);
    }
    /* grammar: int-run [. frac-run] [eE [sign] exp-run], >=1 mantissa digit */
    const char *q = p;
    size_t nd = 0;
    bool ok = wa_num_run(&q, &nd);
    if (ok && *q == '.') {
        q++;
        ok = wa_num_run(&q, &nd);
    }
    if (ok && nd > 0 && (*q == 'e' || *q == 'E')) {
        q++;
        if (*q == '+' || *q == '-') q++;
        size_t ne = 0;
        ok = wa_num_run(&q, &ne);
        if (ne == 0) ok = false;
    }
    if (!ok || nd == 0 || *q != '\0') {
        free(s);
        char msg[512];
        snprintf(msg, sizeof msg, "could not convert string to float: '%s'", raw);
        return wa_err("ValueError", msg);
    }
    /* clean buffer KEEPS the sign (strtod would drop a consumed sign) */
    char clean[256];
    size_t ck = 0;
    if (neg && ck < sizeof clean - 1) clean[ck++] = '-';
    for (const char *t = p; *t; t++)
        if (*t != '_' && ck < sizeof clean - 1) clean[ck++] = *t;
    clean[ck] = '\0';
    double d = strtod(clean, NULL);
    free(s);
    return json_number(d);
}
/* Python numeric + with operand-type TypeError. isfloat tags carry the
 * int/float identity JSON numbers lose (default timeout 5.0 is a float). */
static bool wa_num_tag(const json_t *v, bool *isfloat) {
    if (v && v->type == JSON_NUMBER) {
        *isfloat = !(v->num_val == floor(v->num_val) && fabs(v->num_val) < 9.0e15);
        return true;
    }
    if (v && v->type == JSON_BOOL) { *isfloat = false; return true; }
    return false;
}
static const char *wa_operand_name(const json_t *v, bool isfloat) {
    if (v && v->type == JSON_BOOL) return "bool";
    if (v && v->type == JSON_NUMBER) return isfloat ? "float" : "int";
    return wa_type_name(v);
}
static json_t *wa_num_add(const json_t *a, bool af, const json_t *b, bool bf, json_t **err) {
    *err = NULL;
    /* tags are caller-derived (JSON numbers lose int/float identity; the
     * default timeout 5.0 must stay a float for the operand-type name) */
    bool an = (a && (a->type == JSON_NUMBER || a->type == JSON_BOOL));
    bool bn = (b && (b->type == JSON_NUMBER || b->type == JSON_BOOL));
    if (!an || !bn) {
        char msg[256];
        snprintf(msg, sizeof msg, "unsupported operand type(s) for +: '%s' and '%s'",
                 wa_operand_name(a, af), wa_operand_name(b, bf));
        *err = wa_err("TypeError", msg);
        return NULL;
    }
    double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
    double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
    return json_number(av + bv);
}
/* numeric < deadline (deadline tag: int only when both operands int). */
static bool wa_lt_deadline(const json_t *a, double deadline, bool df, json_t **err) {
    *err = NULL;
    bool af = false;
    if (!wa_num_tag(a, &af)) {
        char msg[256];
        snprintf(msg, sizeof msg, "'<' not supported between instances of '%s' and '%s'",
                 wa_type_name(a), df ? "float" : "int");
        *err = wa_err("TypeError", msg);
        return false;
    }
    double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
    return av < deadline;
}

/* -- SHA-256 (FIPS 180-4) + HMAC-SHA256, self-contained -- */
typedef struct {
    uint32_t h[8];
    uint64_t total;
    unsigned char buf[64];
    size_t buflen;
} wa_sha256_t;
static const uint32_t WA_K256[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
static uint32_t wa_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static void wa_sha256_block(wa_sha256_t *c, const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16)
             | ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = wa_rotr(w[i-15],7) ^ wa_rotr(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = wa_rotr(w[i-2],17) ^ wa_rotr(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],h=c->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = wa_rotr(e,6) ^ wa_rotr(e,11) ^ wa_rotr(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + WA_K256[i] + w[i];
        uint32_t S0 = wa_rotr(a,2) ^ wa_rotr(a,13) ^ wa_rotr(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d; c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
}
static void wa_sha256_init(wa_sha256_t *c) {
    c->h[0]=0x6a09e667u; c->h[1]=0xbb67ae85u; c->h[2]=0x3c6ef372u; c->h[3]=0xa54ff53au;
    c->h[4]=0x510e527fu; c->h[5]=0x9b05688cu; c->h[6]=0x1f83d9abu; c->h[7]=0x5be0cd19u;
    c->total = 0; c->buflen = 0;
}
static void wa_sha256_update(wa_sha256_t *c, const void *data, size_t len) {
    const unsigned char *p = (const unsigned char *)data;
    c->total += len;
    while (len > 0) {
        size_t take = 64 - c->buflen;
        if (take > len) take = len;
        memcpy(c->buf + c->buflen, p, take);
        c->buflen += take; p += take; len -= take;
        if (c->buflen == 64) { wa_sha256_block(c, c->buf); c->buflen = 0; }
    }
}
static void wa_sha256_final(wa_sha256_t *c, unsigned char out[32]) {
    uint64_t bits = c->total * 8;
    unsigned char pad = 0x80;
    wa_sha256_update(c, &pad, 1);
    unsigned char zero = 0;
    while (c->buflen != 56) wa_sha256_update(c, &zero, 1);
    unsigned char lenb[8];
    for (int i = 0; i < 8; i++) lenb[i] = (unsigned char)(bits >> (56 - 8*i));
    wa_sha256_update(c, lenb, 8);
    for (int i = 0; i < 8; i++) {
        out[i*4]   = (unsigned char)(c->h[i] >> 24);
        out[i*4+1] = (unsigned char)(c->h[i] >> 16);
        out[i*4+2] = (unsigned char)(c->h[i] >> 8);
        out[i*4+3] = (unsigned char)(c->h[i]);
    }
}
static void wa_hex(const unsigned char *b, size_t n, char *o) {
    static const char hx[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) { o[i*2] = hx[b[i] >> 4]; o[i*2+1] = hx[b[i] & 15]; }
    o[n*2] = '\0';
}
/* malloc'd 64-char lowercase hex sha256 of the bytes. */
static char *wa_sha256_hex(const void *data, size_t len) {
    wa_sha256_t c;
    unsigned char d[32];
    wa_sha256_init(&c);
    wa_sha256_update(&c, data, len);
    wa_sha256_final(&c, d);
    char *o = malloc(65);
    wa_hex(d, 32, o);
    return o;
}
/* malloc'd 64-char lowercase hex HMAC-SHA256(key, msg), UTF-8 byte inputs. */
static char *wa_hmac_sha256_hex(const char *key, const char *msg) {
    unsigned char k[64];
    size_t kl = strlen(key);
    if (kl > 64) {
        wa_sha256_t kc;
        unsigned char kd[32];
        wa_sha256_init(&kc);
        wa_sha256_update(&kc, key, kl);
        wa_sha256_final(&kc, kd);
        memcpy(k, kd, 32);
        memset(k + 32, 0, 32);
    } else {
        memcpy(k, key, kl);
        memset(k + kl, 0, 64 - kl);
    }
    unsigned char ipad[64], opad[64];
    for (int i = 0; i < 64; i++) { ipad[i] = k[i] ^ 0x36; opad[i] = k[i] ^ 0x5c; }
    wa_sha256_t inner;
    unsigned char id[32];
    wa_sha256_init(&inner);
    wa_sha256_update(&inner, ipad, 64);
    wa_sha256_update(&inner, msg, strlen(msg));
    wa_sha256_final(&inner, id);
    wa_sha256_t outer;
    unsigned char od[32];
    wa_sha256_init(&outer);
    wa_sha256_update(&outer, opad, 64);
    wa_sha256_update(&outer, id, 32);
    wa_sha256_final(&outer, od);
    char *o = malloc(65);
    wa_hex(od, 32, o);
    return o;
}

/* -- REAL-ALWAYS leaves -- */

/* PoP: wa__safe_ints @ plugins/platforms/whatsapp/adapter.py:_safe_ints */
static json_t *wa__safe_ints(wa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *tokens = json_obj_get(args, "tokens");
    json_t *out = json_array();
    size_t n = tokens ? json_len(tokens) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *r = wa_int_parse(json_get(tokens, i));
        if (wa_is_raise(r)) {
            /* except ValueError: pass -- every other kind propagates */
            if (strcmp(wa_kind(r), "ValueError") == 0) { json_free(r); continue; }
            json_free(out);
            return r;
        }
        json_append(out, r);
    }
    return out;
}

/* PoP: wa__fmt_uptime @ plugins/platforms/whatsapp/adapter.py:_fmt_uptime */
static json_t *wa__fmt_uptime(wa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *seconds = json_obj_get(args, "seconds");
    int special = 0;
    json_t *f = wa_float_parse(seconds, &special);
    if (wa_is_raise(f)) {
        const char *k = wa_kind(f);
        if (strcmp(k, "TypeError") == 0 || strcmp(k, "ValueError") == 0) {
            json_free(f);
            return json_string("");
        }
        return f;
    }
    if (special == 1 || special == -1) {
        json_free(f);
        return wa_err("OverflowError", "cannot convert float infinity to integer");
    }
    if (special == 2) {
        json_free(f);
        return json_string("");  /* int(nan) ValueError is caught */
    }
    /* float("1e999") overflows to inf with special==0 -- same int() rules */
    if (isinf(f->num_val)) {
        json_free(f);
        return wa_err("OverflowError", "cannot convert float infinity to integer");
    }
    if (isnan(f->num_val)) {
        json_free(f);
        return json_string("");
    }
    long long secs = (long long)trunc(f->num_val);
    json_free(f);
    if (secs < 0) secs = 0;
    long long days = secs / 86400, rem = secs % 86400;
    long long hours = rem / 3600;
    rem = rem % 3600;
    long long minutes = rem / 60;
    char buf[128];
    if (days) snprintf(buf, sizeof buf, "%lldd %lldh", days, hours);
    else if (hours) snprintf(buf, sizeof buf, "%lldh %lldm", hours, minutes);
    else if (minutes) snprintf(buf, sizeof buf, "%lldm", minutes);
    else snprintf(buf, sizeof buf, "%llds", rem);
    return json_string(buf);
}

/* PoP: wa__bridge_auth_headers @ plugins/platforms/whatsapp/adapter.py:_bridge_auth_headers */
static json_t *wa__bridge_auth_headers(wa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *tok = wa_str_proj(json_obj_get(args, "token"));
    size_t n = strlen(tok) + 8;
    char *v = malloc(n);
    snprintf(v, n, "Bearer %s", tok);
    free(tok);
    json_t *out = json_object();
    json_set_hdr(out, "Authorization", v);
    free(v);
    return out;
}

/* _WA_EXT_MEDIA_TYPE (module constant, insertion order irrelevant: lookup). */
static const char *WA_EXT_MEDIA[][2] = {
    {".jpg","image"},{".jpeg","image"},{".png","image"},{".webp","image"},{".gif","image"},
    {".mp4","video"},{".mov","video"},{".avi","video"},{".mkv","video"},{".webm","video"},{".3gp","video"},
    {".ogg","audio"},{".opus","audio"},{".mp3","audio"},{".wav","audio"},{".m4a","audio"},{".flac","audio"}
};

/* PoP: wa__bridge_media_type @ plugins/platforms/whatsapp/adapter.py:_bridge_media_type */
static json_t *wa__bridge_media_type(wa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    if (wa_truthy(json_obj_get(args, "force_document"))) return json_string("document");
    if (wa_truthy(json_obj_get(args, "is_voice"))) return json_string("audio");
    const json_t *fp = json_obj_get(args, "file_path");
    const char *path = wa_str(fp);
    if (!path) {
        char msg[256];
        snprintf(msg, sizeof msg, "expected str, bytes or os.PathLike object, not %s", wa_type_name(fp));
        return wa_err("TypeError", msg);
    }
    char *ext = wa_splitext(path);
    char *low = wa_lower_ascii(ext);
    free(ext);
    const char *kind = "document";
    for (size_t i = 0; i < sizeof WA_EXT_MEDIA / sizeof WA_EXT_MEDIA[0]; i++) {
        if (strcmp(low, WA_EXT_MEDIA[i][0]) == 0) { kind = WA_EXT_MEDIA[i][1]; break; }
    }
    free(low);
    return json_string(kind);
}

/* PoP: wa__bridge_auth_proof @ plugins/platforms/whatsapp/adapter.py:_bridge_auth_proof */
static json_t *wa__bridge_auth_proof(wa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *tv = json_obj_get(args, "token");
    const json_t *cv = json_obj_get(args, "challenge");
    const char *token = wa_str(tv);
    if (!token) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'encode'", wa_type_name(tv));
        return wa_err("AttributeError", msg);
    }
    const char *challenge = wa_str(cv);
    if (!challenge) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'encode'", wa_type_name(cv));
        return wa_err("AttributeError", msg);
    }
    char *hex = wa_hmac_sha256_hex(token, challenge);
    json_t *out = json_string(hex);
    free(hex);
    return out;
}

/* _BRIDGE_CONSUMED_ENV (module constant; passthrough 14 + resolved 6). */
static const char *WA_CONSUMED_ENV[] = {
    "WHATSAPP_ALLOWED_USERS", "WHATSAPP_ALLOW_FROM", "WHATSAPP_DM_POLICY", "WHATSAPP_GROUP_POLICY",
    "WHATSAPP_GROUP_ALLOWED_USERS", "WHATSAPP_GROUP_ALLOW_FROM", "WHATSAPP_REQUIRE_MENTION",
    "WHATSAPP_MENTION_PATTERNS", "WHATSAPP_FREE_RESPONSE_CHATS", "WHATSAPP_DEBUG",
    "WHATSAPP_FORWARD_OWNER_MESSAGES", "WHATSAPP_MAX_MESSAGE_LENGTH",
    "WHATSAPP_CHUNK_DELAY_MS", "WHATSAPP_SEND_TIMEOUT_MS",
    "WHATSAPP_MODE", "WHATSAPP_REPLY_PREFIX", "WHATSAPP_SEND_READ_RECEIPTS",
    "HERMES_IMAGE_CACHE_DIR", "HERMES_AUDIO_CACHE_DIR", "HERMES_DOCUMENT_CACHE_DIR"
};

/* PoP: wa__bridge_config_fingerprint @ plugins/platforms/whatsapp/adapter.py:_bridge_config_fingerprint */
static json_t *wa__bridge_config_fingerprint(wa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *env = json_obj_get(args, "bridge_env");
    const json_t *phv = json_obj_get(args, "package_hash");
    const char *ph = phv ? wa_str(phv) : "";
    if (!ph) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'encode'", wa_type_name(phv));
        return wa_err("AttributeError", msg);
    }
    if (!env || env->type != JSON_OBJECT) {
        char msg[256];
        snprintf(msg, sizeof msg, "argument of type '%s' is not a container or iterable", wa_type_name(env));
        return wa_err("TypeError", msg);
    }
    wa_sha256_t c;
    wa_sha256_init(&c);
    wa_sha256_update(&c, "package.json\0", 13);
    wa_sha256_update(&c, ph, strlen(ph));
    wa_sha256_update(&c, "\0", 1);
    for (size_t i = 0; i < sizeof WA_CONSUMED_ENV / sizeof WA_CONSUMED_ENV[0]; i++) {
        const char *key = WA_CONSUMED_ENV[i];
        wa_sha256_update(&c, key, strlen(key));
        if (json_has(env, key)) {
            wa_sha256_update(&c, "\x01", 1);
            char *sv = wa_str_proj(json_obj_get(env, key));
            wa_sha256_update(&c, sv, strlen(sv));
            free(sv);
        } else {
            wa_sha256_update(&c, "\0", 1);
        }
        wa_sha256_update(&c, "\0", 1);
    }
    unsigned char d[32];
    wa_sha256_final(&c, d);
    char hex[65];
    wa_hex(d, 32, hex);
    hex[16] = '\0';
    return json_string(hex);
}

/* _NATIVE_MEDIA_TYPES / _MEDIA_NEEDLES (MessageType projected to .value). */
static const char *WA_NATIVE[][2] = {
    {"location","location"}, {"live_location","location"}, {"sticker","sticker"}
};
static const char *WA_NEEDLES[][2] = {
    {"image","photo"}, {"video","video"}, {"ptt","voice"}, {"audio","audio"}
};

/* PoP: wa__classify_bridge_message @ plugins/platforms/whatsapp/adapter.py:_classify_bridge_message */
static json_t *wa__classify_bridge_message(wa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *data = json_obj_get(args, "data");
    const json_t *mt = data ? json_obj_get(data, "mediaType") : NULL;
    char *media_type;
    if (!mt || !wa_truthy(mt)) media_type = wa_strdup("");
    else media_type = wa_str_proj(mt);
    for (size_t i = 0; i < sizeof WA_NATIVE / sizeof WA_NATIVE[0]; i++) {
        if (strcmp(media_type, WA_NATIVE[i][0]) == 0) {
            json_t *out = json_string(WA_NATIVE[i][1]);
            free(media_type);
            return out;
        }
    }
    if (!wa_truthy(data ? json_obj_get(data, "hasMedia") : NULL)) {
        free(media_type);
        return json_string("text");
    }
    const char *kind = "document";
    for (size_t i = 0; i < sizeof WA_NEEDLES / sizeof WA_NEEDLES[0]; i++) {
        if (strstr(media_type, WA_NEEDLES[i][0]) != NULL) { kind = WA_NEEDLES[i][1]; break; }
    }
    free(media_type);
    return json_string(kind);
}

/* -- simple flows (REAL target / SEAMED caller) -- */

/* Literal single-char split keeping empty parts (str.split("\n")). */
static char **wa_split_char(const char *s, char delim, size_t *n) {
    if (!s) s = "";
    size_t len = strlen(s), cap = 8, k = 0, start = 0;
    char **parts = malloc(cap * sizeof(char *));
    for (size_t i = 0; i <= len; i++) {
        if (i == len || s[i] == delim) {
            if (k == cap) { cap *= 2; parts = realloc(parts, cap * sizeof(char *)); }
            parts[k] = malloc(i - start + 1);
            memcpy(parts[k], s + start, i - start);
            parts[k][i - start] = '\0';
            k++;
            start = i + 1;
        }
    }
    *n = k;
    return parts;
}
/* utf-8-sig decode model: strip ONE leading BOM (EF BB BF). */
static const char *wa_skip_bom(const char *s) {
    if (s && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
        return s + 3;
    return s;
}
static json_t *wa_triple_null(void) {
    json_t *a = json_array();
    json_append(a, json_null());
    json_append(a, json_null());
    json_append(a, json_null());
    return a;
}

/* PoP: wa__wenv @ plugins/platforms/whatsapp/adapter.py:_wenv */
static json_t *wa__wenv(wa_ctx_t *ctx, const json_t *args) {
    json_t *a = wa_args();
    wa_set_copy(a, "name", json_obj_get(args, "name"));
    const json_t *def = json_obj_get(args, "default");
    if (def) wa_set_copy(a, "default", def);
    else wa_set_s(a, "default", "");
    return wa_call(ctx, "get_scoped_secret", a);
}

/* PoP: wa__is_connected @ plugins/platforms/whatsapp/adapter.py:_is_connected */
static json_t *wa__is_connected(wa_ctx_t *ctx, const json_t *args) {
    const json_t *config = json_obj_get(args, "config");
    if (config && config->type != JSON_NULL && config->type == JSON_OBJECT) {
        const json_t *enabled = json_obj_get(config, "enabled");
        const json_t *extra = json_obj_get(config, "extra");
        if (wa_truthy(enabled) && wa_truthy(extra)) return json_bool(true);
    }
    json_t *a = wa_args();
    wa_set_s(a, "name", "WHATSAPP_ENABLED");
    json_t *r = wa_call(ctx, "get_env_value", a);
    if (wa_is_raise(r)) return r;
    const char *s = NULL;
    char *tmp = NULL;
    if (!wa_truthy(r)) {
        s = "";
    } else if (wa_str(r)) {
        s = wa_str(r);
    } else {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", wa_type_name(r));
        json_free(r);
        return wa_err("AttributeError", msg);
    }
    tmp = wa_strip(s);
    char *low = wa_lower_ascii(tmp);
    free(tmp);
    bool conn = strcmp(low, "true") == 0 || strcmp(low, "1") == 0 || strcmp(low, "yes") == 0;
    free(low);
    json_free(r);
    return json_bool(conn);
}

/* PoP: wa__cache_dirs @ plugins/platforms/whatsapp/adapter.py:_cache_dirs */
static json_t *wa__cache_dirs(wa_ctx_t *ctx, const json_t *args) {
    (void)args;
    static const char *getters[] = {
        "get_image_cache_dir", "get_audio_cache_dir", "get_video_cache_dir", "get_document_cache_dir"
    };
    json_t *out = json_array();
    for (size_t i = 0; i < 4; i++) {
        json_t *r = wa_call(ctx, getters[i], wa_args());
        if (wa_is_raise(r)) { json_free(out); return r; }
        json_append(out, r);
    }
    return out;
}

/* PoP: wa__unlink_quietly @ plugins/platforms/whatsapp/adapter.py:_unlink_quietly */
static json_t *wa__unlink_quietly(wa_ctx_t *ctx, const json_t *args) {
    json_t *a = wa_args();
    wa_set_copy(a, "path", json_obj_get(args, "path"));
    json_t *r = wa_call(ctx, "unlink", a);
    if (wa_is_raise(r)) {
        if (wa_kind_is(wa_kind(r), "OSError")) { json_free(r); return json_null(); }
        return r;  /* BaseException and every other kind propagate */
    }
    json_free(r);
    return json_null();
}

/* PoP: wa__tighten_bridge_log @ plugins/platforms/whatsapp/adapter.py:_tighten_bridge_log */
static json_t *wa__tighten_bridge_log(wa_ctx_t *ctx, const json_t *args) {
    if (!wa_truthy(json_obj_get(args, "private"))) return json_null();
    json_t *a = wa_args();
    wa_set_copy(a, "path", json_obj_get(args, "path"));
    wa_set_n(a, "mode", 384.0);  /* 0o600 */
    json_t *r = wa_call(ctx, "chmod", a);
    if (wa_is_raise(r)) {
        if (wa_kind_is(wa_kind(r), "OSError")) { json_free(r); return json_null(); }
        return r;
    }
    json_free(r);
    return json_null();
}

/* PoP: wa__read_bridge_token @ plugins/platforms/whatsapp/adapter.py:_read_bridge_token */
static json_t *wa__read_bridge_token(wa_ctx_t *ctx, const json_t *args) {
    const char *session = json_get_str(args, "session_path", "");
    char *path = wa_path_join(session, ".bridge-token");
    json_t *a = wa_args();
    wa_set_s(a, "path", path);
    wa_set_s(a, "encoding", "utf-8-sig");
    free(path);
    json_t *r = wa_call(ctx, "read_text", a);
    if (wa_is_raise(r)) {
        if (wa_kind_is(wa_kind(r), "OSError")) { json_free(r); return json_string(""); }
        return r;
    }
    const char *s = wa_str(r);
    if (!s) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", wa_type_name(r));
        json_free(r);
        return wa_err("AttributeError", msg);
    }
    char *stripped = wa_strip(wa_skip_bom(s));
    json_free(r);
    json_t *out = json_string(stripped);
    free(stripped);
    return out;
}

/* PoP: wa__write_bridge_fingerprint @ plugins/platforms/whatsapp/adapter.py:_write_bridge_fingerprint */
static json_t *wa__write_bridge_fingerprint(wa_ctx_t *ctx, const json_t *args) {
    const char *session = json_get_str(args, "session_path", "");
    char *path = wa_path_join(session, "bridge.env-fingerprint");
    json_t *a = wa_args();
    wa_set_s(a, "path", path);
    wa_set_copy(a, "text", json_obj_get(args, "fingerprint"));
    wa_set_s(a, "encoding", "utf-8");
    free(path);
    json_t *r = wa_call(ctx, "write_text", a);
    if (wa_is_raise(r)) {
        if (wa_kind_is(wa_kind(r), "OSError")) { json_free(r); return json_null(); }
        return r;
    }
    json_free(r);
    return json_null();
}

/* PoP: wa__write_bridge_pidfile @ plugins/platforms/whatsapp/adapter.py:_write_bridge_pidfile */
static json_t *wa__write_bridge_pidfile(wa_ctx_t *ctx, const json_t *args) {
    const json_t *pid = json_obj_get(args, "pid");
    const json_t *port = json_obj_get(args, "port");  /* absent == None default */
    json_t *a = wa_args();
    wa_set_copy(a, "pid", pid);
    json_t *start = wa_call(ctx, "get_process_start_time", a);
    if (wa_is_raise(start)) {
        if (wa_kind_is(wa_kind(start), "OSError")) { json_free(start); return json_null(); }
        return start;
    }
    char *ps = wa_str_proj(pid);
    char *text;
    if (!start || start->type == JSON_NULL) {
        text = wa_strdup(ps);
    } else {
        char *ss = wa_str_proj(start);
        char *tail = "";
        char *ports = NULL;
        if (port && port->type != JSON_NULL) {
            ports = wa_str_proj(port);
            size_t tn = strlen(ports) + 2;
            tail = malloc(tn);
            snprintf(tail, tn, "\n%s", ports);
            free(ports);
        }
        size_t n = strlen(ps) + strlen(ss) + strlen(tail) + 2;
        text = malloc(n);
        snprintf(text, n, "%s\n%s%s", ps, ss, tail);
        if (tail[0]) free(tail);
        free(ss);
    }
    free(ps);
    json_free(start);
    const char *session = json_get_str(args, "session_path", "");
    char *path = wa_path_join(session, "bridge.pid");
    json_t *wa = wa_args();
    wa_set_s(wa, "path", path);
    wa_set_s(wa, "text", text);
    wa_set_s(wa, "encoding", "utf-8");
    free(path);
    free(text);
    json_t *r = wa_call(ctx, "write_text", wa);
    if (wa_is_raise(r)) {
        if (wa_kind_is(wa_kind(r), "OSError")) { json_free(r); return json_null(); }
        return r;
    }
    json_free(r);
    return json_null();
}

/* PoP: wa__read_bridge_pidfile @ plugins/platforms/whatsapp/adapter.py:_read_bridge_pidfile */
static json_t *wa__read_bridge_pidfile(wa_ctx_t *ctx, const json_t *args) {
    const char *session = json_get_str(args, "session_path", "");
    char *path = wa_path_join(session, "bridge.pid");
    json_t *a = wa_args();
    wa_set_s(a, "path", path);
    json_t *ex = wa_call(ctx, "exists", a);
    if (wa_is_raise(ex)) { free(path); return ex; }
    bool exists = wa_truthy(ex);
    json_free(ex);
    if (!exists) { free(path); return wa_triple_null(); }
    json_t *ra = wa_args();
    wa_set_s(ra, "path", path);
    wa_set_s(ra, "encoding", "utf-8-sig");
    free(path);
    json_t *r = wa_call(ctx, "read_text", ra);
    if (wa_is_raise(r)) {
        const char *k = wa_kind(r);
        if (strcmp(k, "ValueError") == 0 || strcmp(k, "TypeError") == 0
            || strcmp(k, "IndexError") == 0 || wa_kind_is(k, "OSError")) {
            json_free(r);
            return wa_triple_null();
        }
        return r;
    }
    const char *s = wa_str(r);
    if (!s) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'split'", wa_type_name(r));
        json_free(r);
        return wa_err("AttributeError", msg);
    }
    size_t nl = 0;
    char **lines = wa_split_char(wa_skip_bom(s), '\n', &nl);
    json_free(r);
    /* strip each line, then int() the first three (truthy-gated) */
    json_t *vals[3] = { NULL, NULL, NULL };
    bool failed = false;
    for (size_t i = 0; i < 3 && i < nl; i++) {
        char *st = wa_strip(lines[i]);
        if (i == 0 || st[0] != '\0') {
            json_t *tok = json_string(st);
            json_t *v = wa_int_parse(tok);
            json_free(tok);
            if (wa_is_raise(v)) {
                /* only ValueError reachable from a str token; the live
                 * except tuple catches it -> all-None triple */
                json_free(v);
                failed = true;
                free(st);
                break;
            }
            vals[i] = v;
        }
        free(st);
    }
    wa_free_parts(lines, nl);
    if (failed) {
        for (size_t i = 0; i < 3; i++) if (vals[i]) json_free(vals[i]);
        return wa_triple_null();
    }
    json_t *out = json_array();
    for (size_t i = 0; i < 3; i++) json_append(out, vals[i] ? vals[i] : json_null());
    return out;
}

/* PoP: wa__bridge_pid_is_ours @ plugins/platforms/whatsapp/adapter.py:_bridge_pid_is_ours */
static json_t *wa__bridge_pid_is_ours(wa_ctx_t *ctx, const json_t *args) {
    const json_t *pid = json_obj_get(args, "pid");
    const json_t *expected = json_obj_get(args, "expected_start");
    json_t *a = wa_args();
    wa_set_copy(a, "pid", pid);
    json_t *r = wa_call(ctx, "_pid_exists", a);
    if (wa_is_raise(r)) return r;
    bool alive = wa_truthy(r);
    json_free(r);
    if (!alive) return json_bool(false);
    if (!expected || expected->type == JSON_NULL) return json_bool(false);
    json_t *sa = wa_args();
    wa_set_copy(sa, "pid", pid);
    json_t *st = wa_call(ctx, "get_process_start_time", sa);
    if (wa_is_raise(st)) return st;
    bool eq = wa_py_eq(st, expected);
    json_free(st);
    return json_bool(eq);
}

/* PoP: wa__pids_alive @ plugins/platforms/whatsapp/adapter.py:_pids_alive */
static json_t *wa__pids_alive(wa_ctx_t *ctx, const json_t *args) {
    const json_t *pids = json_obj_get(args, "pids");
    json_t *out = json_array();
    size_t n = pids ? json_len(pids) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *pid = json_get(pids, i);
        json_t *a = wa_args();
        wa_set_copy(a, "pid", pid);
        json_t *r = wa_call(ctx, "_pid_exists", a);
        if (wa_is_raise(r)) { json_free(out); return r; }
        bool alive = wa_truthy(r);
        json_free(r);
        if (alive) json_append(out, json_copy(pid));
    }
    return out;
}

/* -- process discovery / identity flows -- */

static json_t *wa_argv(const char *const *items, size_t n) {
    json_t *a = json_array();
    for (size_t i = 0; i < n; i++) json_append(a, json_string(items[i]));
    return a;
}
/* subprocess.run kwargs projection. run_text adds the _RUN_TEXT expansion
 * (capture_output/text/encoding/errors/stdin); the taskkill site carries
 * only capture_output/stdin. creationflags/env included when non-NULL. */
static json_t *wa_run_args(json_t *argv, double timeout, bool run_text,
                           const json_t *creationflags, const json_t *env) {
    json_t *a = wa_args();
    json_set(a, "argv", argv);
    wa_set_n(a, "timeout", timeout);
    if (creationflags) wa_set_copy(a, "creationflags", creationflags);
    if (env) wa_set_copy(a, "env", env);
    wa_set_b(a, "capture_output", true);
    if (run_text) {
        wa_set_b(a, "text", true);
        wa_set_s(a, "encoding", "utf-8");
        wa_set_s(a, "errors", "replace");
    }
    wa_set_n(a, "stdin", -3.0);  /* subprocess.DEVNULL */
    wa_set_b(a, "check", false);
    return a;
}
static bool wa_endswith(const char *s, const char *suf) {
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && strcmp(s + ls - lf, suf) == 0;
}

/* PoP: wa__wait_pids_exit @ plugins/platforms/whatsapp/adapter.py:_wait_pids_exit */
static json_t *wa__wait_pids_exit(wa_ctx_t *ctx, const json_t *args) {
    const json_t *pids = json_obj_get(args, "pids");
    const json_t *timeout = json_obj_get(args, "timeout");
    double tval = 5.0;          /* _SIGTERM_GRACE_S */
    bool tfloat = true;         /* the default is a Python float */
    json_t *top = NULL;         /* timeout operand (borrowed or temp default) */
    bool top_owned = false;
    if (timeout && timeout->type != JSON_NULL) {
        top = (json_t *)timeout;
        if (timeout->type == JSON_NUMBER || timeout->type == JSON_BOOL) {
            tval = timeout->type == JSON_BOOL ? (timeout->bool_val ? 1.0 : 0.0) : timeout->num_val;
            tfloat = !(tval == floor(tval) && fabs(tval) < 9.0e15);
        } else {
            tfloat = false;     /* non-numeric: the + below raises */
        }
    } else {
        top = json_number(tval);
        top_owned = true;
    }
    json_t *m0 = wa_call(ctx, "time.monotonic", wa_args());
    if (wa_is_raise(m0)) { if (top_owned) json_free(top); return m0; }
    bool mfloat = false;
    wa_num_tag(m0, &mfloat);
    json_t *err = NULL;
    json_t *dl = wa_num_add(m0, mfloat, top, tfloat, &err);
    if (top_owned) json_free(top);
    json_free(m0);
    if (err) return err;
    double deadline = dl->num_val;
    bool dfloat = tfloat || mfloat;
    json_free(dl);
    json_t *pa = wa_args();
    wa_set_copy(pa, "pids", pids);
    json_t *alive = wa_call(ctx, "_pids_alive", pa);
    if (wa_is_raise(alive)) return alive;
    while (wa_truthy(alive)) {
        json_t *m = wa_call(ctx, "time.monotonic", wa_args());
        if (wa_is_raise(m)) { json_free(alive); return m; }
        json_t *lerr = NULL;
        bool lt = wa_lt_deadline(m, deadline, dfloat, &lerr);
        json_free(m);
        if (lerr) { json_free(alive); return lerr; }
        if (!lt) break;
        json_t *sa = wa_args();
        wa_set_n(sa, "seconds", 0.1);
        json_t *sr = wa_call(ctx, "time.sleep", sa);
        if (wa_is_raise(sr)) { json_free(alive); return sr; }
        json_free(sr);
        json_t *pa2 = wa_args();
        wa_set_copy(pa2, "pids", pids);
        json_t *alive2 = wa_call(ctx, "_pids_alive", pa2);
        if (wa_is_raise(alive2)) { json_free(alive); return alive2; }
        json_free(alive);
        alive = alive2;
    }
    return alive;
}

/* PoP: wa__pid_looks_like_node_bridge @ plugins/platforms/whatsapp/adapter.py:_pid_looks_like_node_bridge */
static json_t *wa__pid_looks_like_node_bridge(wa_ctx_t *ctx, const json_t *args) {
    json_t *a = wa_args();
    wa_set_copy(a, "pid", json_obj_get(args, "pid"));
    json_t *proc = wa_call(ctx, "psutil.Process", a);
    if (wa_is_raise(proc)) {
        if (wa_is_base_exc(wa_kind(proc))) return proc;
        json_free(proc);
        return json_bool(false);  /* except Exception */
    }
    json_t *err = NULL;
    const json_t *name = wa_attr(proc, "name", &err);
    if (err) { json_free(proc); json_free(err); return json_bool(false); }
    bool match = false;
    if (wa_truthy(name)) {
        const char *ns = wa_str(name);
        if (!ns) {
            json_free(proc);
            return json_bool(false);  /* (5).lower() AttributeError -> caught */
        }
        char *low = wa_lower_ascii(ns);
        match = strstr(low, "node") != NULL;
        free(low);
        if (match) { json_free(proc); return json_bool(true); }
    }
    const json_t *cmd = wa_attr(proc, "cmdline", &err);
    if (err) { json_free(proc); json_free(err); return json_bool(false); }
    /* " ".join(proc.cmdline() or []) -- list of str (corpus); a str joins
     * its codepoints; other truthy non-iterables raise TypeError. */
    char *joined = NULL;
    if (!wa_truthy(cmd)) {
        joined = wa_strdup("");
    } else if (cmd->type == JSON_ARRAY) {
        size_t n = json_len(cmd), cap = 64, k = 0;
        joined = malloc(cap);
        joined[0] = '\0';
        for (size_t i = 0; i < n; i++) {
            const json_t *el = json_get(cmd, i);
            const char *es = wa_str(el);
            if (!es) {
                /* "sequence item N: expected str instance" TypeError -> caught */
                free(joined);
                json_free(proc);
                return json_bool(false);
            }
            size_t need = k + strlen(es) + 2;
            if (need > cap) { while (cap < need) cap *= 2; joined = realloc(joined, cap); }
            if (i > 0) joined[k++] = ' ';
            strcpy(joined + k, es);
            k += strlen(es);
        }
    } else if (cmd->type == JSON_STRING) {
        size_t n = 0;
        uint32_t *cps = wa_utf8_decode(cmd->str_val, &n);
        joined = malloc(n * 5 + 2);
        char *q = joined;
        for (size_t i = 0; i < n; i++) {
            if (i > 0) *q++ = ' ';
            wa_utf8_putcp(&q, cps[i]);
        }
        *q = '\0';
        free(cps);
    } else {
        free(joined);
        json_free(proc);
        return json_bool(false);  /* " ".join(5) TypeError -> caught */
    }
    char *low = wa_lower_ascii(joined);
    free(joined);
    /* .split(" ", 1)[0] -- the first space-delimited token */
    char *sp = strchr(low, ' ');
    if (sp) *sp = '\0';
    match = strstr(low, "node") != NULL;
    free(low);
    json_free(proc);
    return json_bool(match);
}

/* PoP: wa__windows_listener_pids @ plugins/platforms/whatsapp/adapter.py:_windows_listener_pids */
static json_t *wa__windows_listener_pids(wa_ctx_t *ctx, const json_t *args) {
    const json_t *port = json_obj_get(args, "port");
    json_t *hf = wa_call(ctx, "windows_hide_flags", wa_args());
    if (wa_is_raise(hf)) return hf;
    static const char *argv[] = {"netstat", "-ano", "-p", "TCP"};
    json_t *r = wa_call(ctx, "subprocess.run",
                        wa_run_args(wa_argv(argv, 4), 5.0, true, hf, NULL));
    json_free(hf);
    if (wa_is_raise(r)) return r;
    json_t *err = NULL;
    const json_t *so = wa_attr(r, "stdout", &err);
    if (err) { json_free(r); return err; }
    const char *s = wa_str(so);
    if (!s) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'splitlines'", wa_type_name(so));
        json_free(r);
        return wa_err("AttributeError", msg);
    }
    char *ps = wa_str_proj(port);
    size_t suf_n = strlen(ps) + 2;
    char *suffix = malloc(suf_n);
    snprintf(suffix, suf_n, ":%s", ps);
    free(ps);
    size_t nl = 0;
    char **lines = wa_splitlines(s, &nl);
    json_free(r);
    json_t *tokens = json_array();
    for (size_t i = 0; i < nl; i++) {
        size_t np = 0;
        char **parts = wa_split_ws(lines[i], &np);
        if (np >= 5 && strcmp(parts[3], "LISTENING") == 0 && wa_endswith(parts[1], suffix))
            json_append(tokens, json_string(parts[4]));
        wa_free_parts(parts, np);
    }
    wa_free_parts(lines, nl);
    free(suffix);
    json_t *ta = wa_args();
    json_set(ta, "tokens", tokens);
    json_t *ret = wa__safe_ints(ctx, ta);  /* REAL child */
    json_free(ta);
    return ret;
}

/* PoP: wa__listener_pids_on_port @ plugins/platforms/whatsapp/adapter.py:_listener_pids_on_port */
static json_t *wa__listener_pids_on_port(wa_ctx_t *ctx, const json_t *args) {
    const json_t *port = json_obj_get(args, "port");
    char *ps = wa_str_proj(port);
    json_t *pids = json_array();
    /* block 1: lsof, suppress(FileNotFoundError) */
    {
        char arg[64];
        snprintf(arg, sizeof arg, "tcp:%s", ps);
        const char *argv[] = {"lsof", "-ti", arg, "-sTCP:LISTEN"};
        json_t *r = wa_call(ctx, "subprocess.run", wa_run_args(wa_argv(argv, 4), 5.0, true, NULL, NULL));
        if (wa_is_raise(r)) {
            if (strcmp(wa_kind(r), "FileNotFoundError") != 0) { json_free(pids); free(ps); return r; }
            json_free(r);
        } else {
            json_t *err = NULL;
            const json_t *so = wa_attr(r, "stdout", &err);
            if (err) { json_free(r); json_free(pids); free(ps); return err; }
            const char *s = wa_str(so);
            if (!s) {
                char msg[256];
                snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", wa_type_name(so));
                json_free(r); json_free(pids); free(ps);
                return wa_err("AttributeError", msg);
            }
            char *st = wa_strip(s);
            size_t nl = 0;
            char **lines = wa_splitlines(st, &nl);
            free(st);
            json_free(r);
            json_t *tokens = json_array();
            for (size_t i = 0; i < nl; i++) json_append(tokens, json_string(lines[i]));
            wa_free_parts(lines, nl);
            json_t *ta = wa_args();
            json_set(ta, "tokens", tokens);
            json_t *got = wa__safe_ints(ctx, ta);  /* REAL child */
            json_free(ta);
            if (wa_is_raise(got)) { json_free(pids); free(ps); return got; }
            json_free(pids);
            pids = got;
            if (wa_truthy(pids)) { free(ps); return pids; }
        }
    }
    /* block 2: ss, suppress(FileNotFoundError); pids.extend(pid= matches) */
    {
        char arg[64];
        snprintf(arg, sizeof arg, "sport = :%s", ps);
        const char *argv[] = {"ss", "-ltnHp", arg};
        json_t *r = wa_call(ctx, "subprocess.run", wa_run_args(wa_argv(argv, 3), 5.0, true, NULL, NULL));
        if (wa_is_raise(r)) {
            if (strcmp(wa_kind(r), "FileNotFoundError") != 0) { json_free(pids); free(ps); return r; }
            json_free(r);
        } else {
            json_t *err = NULL;
            const json_t *so = wa_attr(r, "stdout", &err);
            if (err) { json_free(r); json_free(pids); free(ps); return err; }
            const char *s = wa_str(so);
            if (!s) {
                char msg[256];
                snprintf(msg, sizeof msg, "expected string or bytes-like object, got '%s'", wa_type_name(so));
                json_free(r); json_free(pids); free(ps);
                return wa_err("TypeError", msg);
            }
            /* re.finditer(r"pid=(\d+)") -- ASCII digits, non-overlapping */
            const char *search = s;
            while ((search = strstr(search, "pid=")) != NULL) {
                const char *d = search + 4;
                size_t nd = 0;
                while (d[nd] >= '0' && d[nd] <= '9') nd++;
                if (nd > 0) {
                    char buf[64];
                    size_t take = nd < sizeof buf - 1 ? nd : sizeof buf - 1;
                    memcpy(buf, d, take);
                    buf[take] = '\0';
                    json_append(pids, json_number((double)strtoll(buf, NULL, 10)));
                    search = d + nd;
                } else {
                    search = search + 1;
                }
            }
            json_free(r);
        }
    }
    free(ps);
    if (wa_truthy(pids)) return pids;
    /* block 3: psutil, suppress(Exception); import always succeeds (domain) */
    {
        json_t *a = wa_args();
        wa_set_s(a, "kind", "tcp");
        json_t *r = wa_call(ctx, "psutil.net_connections", a);
        if (wa_is_raise(r)) {
            if (wa_is_base_exc(wa_kind(r))) { json_free(pids); return r; }
            json_free(r);
            return pids;  /* swallowed */
        }
        if (!r || r->type != JSON_ARRAY) {
            /* iterating a non-list raises TypeError/AttributeError (Exception) */
            json_free(r);
            return pids;
        }
        size_t n = json_len(r);
        /* the live filter is a LIST COMPREHENSION: an AttributeError on any
         * entry discards the partial result (pids keeps its prior value) */
        json_t *tmp = json_array();
        for (size_t i = 0; i < n; i++) {
            const json_t *c = json_get(r, i);
            json_t *err = NULL;
            const json_t *status = wa_attr(c, "status", &err);
            if (err) { json_free(err); json_free(tmp); json_free(r); return pids; }
            const char *ss = wa_str(status);
            if (!ss || strcmp(ss, "LISTEN") != 0) continue;
            const json_t *laddr = wa_attr(c, "laddr", &err);
            if (err) { json_free(err); json_free(tmp); json_free(r); return pids; }
            /* c.laddr is an addr OBJECT double (always truthy, even empty)
             * or None -- not JSON container truthiness */
            bool laddr_truthy = (laddr && laddr->type == JSON_OBJECT) ? true : wa_truthy(laddr);
            if (!laddr_truthy) continue;
            const json_t *lp = wa_attr(laddr, "port", &err);
            if (err) { json_free(err); json_free(tmp); json_free(r); return pids; }
            if (!wa_py_eq(lp, port)) continue;
            const json_t *cpid = wa_attr(c, "pid", &err);
            if (err) { json_free(err); json_free(tmp); json_free(r); return pids; }
            if (wa_truthy(cpid)) json_append(tmp, json_copy(cpid));
        }
        json_free(r);
        json_free(pids);
        pids = tmp;
    }
    return pids;
}

/* PoP: wa__port_listener_pids @ plugins/platforms/whatsapp/adapter.py:_port_listener_pids */
static json_t *wa__port_listener_pids(wa_ctx_t *ctx, const json_t *args) {
    bool isw = json_get_bool(args, "is_windows", true);
    json_t *a = wa_args();
    wa_set_copy(a, "port", json_obj_get(args, "port"));
    return wa_call(ctx, isw ? "_windows_listener_pids" : "_listener_pids_on_port", a);
}

/* -- kill flows -- */

/* PoP: wa__kill_port_process @ plugins/platforms/whatsapp/adapter.py:_kill_port_process */
static json_t *wa__kill_port_process(wa_ctx_t *ctx, const json_t *args) {
    const json_t *port = json_obj_get(args, "port");
    bool isw = json_get_bool(args, "is_windows", true);
    double sigkill = isw ? 15.0 : 9.0;  /* _SIGKILL: no SIGKILL on Windows */
    json_t *signalled = json_array();
    json_t *pla = wa_args();
    wa_set_copy(pla, "port", port);
    json_t *r = wa_call(ctx, "_port_listener_pids", pla);
    /* outer suppress(Exception) wraps the whole scan loop */
    if (wa_is_raise(r)) {
        if (wa_is_base_exc(wa_kind(r))) { json_free(signalled); return r; }
        json_free(r);  /* swallowed: scan aborts, flow continues */
        r = NULL;
    }
    if (r && r->type != JSON_ARRAY) {
        /* for pid in <non-iterable> TypeError -> swallowed */
        json_free(r);
        r = NULL;
    }
    if (r) {
        size_t n = json_len(r);
        for (size_t i = 0; i < n; i++) {
            const json_t *pid = json_get(r, i);
            bool le0 = false;
            if (pid && (pid->type == JSON_NUMBER || pid->type == JSON_BOOL)) {
                double pv = pid->type == JSON_BOOL ? (pid->bool_val ? 1.0 : 0.0) : pid->num_val;
                le0 = pv <= 0.0;
            } else {
                /* pid <= 0 TypeError -> outer suppress swallows, scan aborts */
                break;
            }
            bool looks = false;
            if (!le0) {
                json_t *la = wa_args();
                wa_set_copy(la, "pid", pid);
                json_t *lr = wa_call(ctx, "_pid_looks_like_node_bridge", la);
                if (wa_is_raise(lr)) {
                    if (wa_is_base_exc(wa_kind(lr))) { json_free(r); json_free(signalled); return lr; }
                    json_free(lr);
                    break;  /* Exception swallowed by the outer suppress */
                }
                looks = wa_truthy(lr);
                json_free(lr);
            }
            if (le0 || !looks) {
                json_t *wa = wa_args();
                wa_set_s(wa, "msg", "[whatsapp] Not killing PID %s on port %d: process is not a node bridge (or identity unverifiable)");
                json_t *aa = json_array();
                json_append(aa, json_copy(pid));
                json_append(aa, json_copy(port));
                json_set(wa, "args", aa);
                json_t *wr = wa_call(ctx, "logger.warning", wa);
                if (wa_is_raise(wr)) {
                    if (wa_is_base_exc(wa_kind(wr))) { json_free(r); json_free(signalled); return wr; }
                    json_free(wr);
                    break;
                }
                json_free(wr);
                continue;
            }
            if (isw) {
                /* inner suppress(subprocess.SubprocessError) per PID */
                json_t *hf = wa_call(ctx, "windows_hide_flags", wa_args());
                bool skip = false;
                if (wa_is_raise(hf)) {
                    if (wa_is_base_exc(wa_kind(hf))) { json_free(r); json_free(signalled); return hf; }
                    if (wa_kind_is(wa_kind(hf), "SubprocessError")) { json_free(hf); skip = true; }
                    else { json_free(hf); break; }  /* outer suppress */
                }
                if (!skip) {
                    char *ps = wa_str_proj(pid);
                    const char *argv[] = {"taskkill", "/PID", ps, "/F"};
                    json_t *rr = wa_call(ctx, "subprocess.run",
                                         wa_run_args(wa_argv(argv, 4), 5.0, false, hf, NULL));
                    free(ps);
                    json_free(hf);
                    if (wa_is_raise(rr)) {
                        if (wa_is_base_exc(wa_kind(rr))) { json_free(r); json_free(signalled); return rr; }
                        if (wa_kind_is(wa_kind(rr), "SubprocessError")) { json_free(rr); }
                        else { json_free(rr); break; }
                    } else {
                        json_free(rr);
                    }
                }
            } else {
                /* inner suppress(OSError): append only after a clean kill */
                json_t *ka = wa_args();
                wa_set_copy(ka, "pid", pid);
                wa_set_n(ka, "sig", 15.0);  /* signal.SIGTERM */
                json_t *kr = wa_call(ctx, "os.kill", ka);
                if (wa_is_raise(kr)) {
                    if (wa_is_base_exc(wa_kind(kr))) { json_free(r); json_free(signalled); return kr; }
                    if (wa_kind_is(wa_kind(kr), "OSError")) { json_free(kr); }
                    else { json_free(kr); break; }
                } else {
                    json_free(kr);
                    json_append(signalled, json_copy(pid));
                }
            }
        }
        json_free(r);
    }
    if (!wa_truthy(signalled)) { json_free(signalled); return json_null(); }
    json_t *wa = wa_args();
    json_set(wa, "pids", json_copy(signalled));
    wa_set_null(wa, "timeout");
    json_t *survivors = wa_call(ctx, "_wait_pids_exit", wa);
    json_free(signalled);
    if (wa_is_raise(survivors)) return survivors;
    if (!survivors || survivors->type != JSON_ARRAY) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", wa_type_name(survivors));
        json_free(survivors);
        return wa_err("TypeError", msg);
    }
    size_t ns = json_len(survivors);
    for (size_t i = 0; i < ns; i++) {
        const json_t *pid = json_get(survivors, i);
        json_t *la = wa_args();
        wa_set_copy(la, "pid", pid);
        json_t *lr = wa_call(ctx, "_pid_looks_like_node_bridge", la);
        if (wa_is_raise(lr)) { json_free(survivors); return lr; }
        bool looks = wa_truthy(lr);
        json_free(lr);
        if (!looks) {
            json_t *w = wa_args();
            wa_set_s(w, "msg", "[whatsapp] Not SIGKILLing PID %s on port %d: identity no longer verifiable");
            json_t *aa = json_array();
            json_append(aa, json_copy(pid));
            json_append(aa, json_copy(port));
            json_set(w, "args", aa);
            json_t *wr = wa_call(ctx, "logger.warning", w);
            if (wa_is_raise(wr)) { json_free(survivors); return wr; }
            json_free(wr);
            continue;
        }
        json_t *ka = wa_args();
        wa_set_copy(ka, "pid", pid);
        wa_set_n(ka, "sig", sigkill);
        json_t *kr = wa_call(ctx, "os.kill", ka);
        if (wa_is_raise(kr)) {
            if (wa_kind_is(wa_kind(kr), "OSError")) { json_free(kr); continue; }
            json_free(survivors);
            return kr;  /* BaseException + every other kind propagate */
        }
        json_free(kr);
        json_t *w = wa_args();
        wa_set_s(w, "msg", "[whatsapp] SIGKILLed node bridge PID %d on port %d after it ignored SIGTERM");
        json_t *aa = json_array();
        json_append(aa, json_copy(pid));
        json_append(aa, json_copy(port));
        json_set(w, "args", aa);
        json_t *wr = wa_call(ctx, "logger.warning", w);
        if (wa_is_raise(wr)) { json_free(survivors); return wr; }
        json_free(wr);
    }
    json_t *fa = wa_args();
    json_set(fa, "pids", json_copy(survivors));
    wa_set_n(fa, "timeout", 1.0);
    json_free(survivors);
    json_t *fr = wa_call(ctx, "_wait_pids_exit", fa);
    if (wa_is_raise(fr)) return fr;
    json_free(fr);
    return json_null();
}

/* PoP: wa__kill_stale_bridge_by_pidfile @ plugins/platforms/whatsapp/adapter.py:_kill_stale_bridge_by_pidfile */
static json_t *wa__kill_stale_bridge_by_pidfile(wa_ctx_t *ctx, const json_t *args) {
    const char *session = json_get_str(args, "session_path", "");
    bool isw = json_get_bool(args, "is_windows", true);
    double sigkill = isw ? 15.0 : 9.0;
    char *path = wa_path_join(session, "bridge.pid");
    json_t *ea = wa_args();
    wa_set_s(ea, "path", path);
    json_t *ex = wa_call(ctx, "exists", ea);
    if (wa_is_raise(ex)) { free(path); return ex; }
    bool exists = wa_truthy(ex);
    json_free(ex);
    if (!exists) { free(path); return json_null(); }
    json_t *ra = wa_args();
    wa_set_s(ra, "session_path", session);
    json_t *rf = wa_call(ctx, "_read_bridge_pidfile", ra);
    if (wa_is_raise(rf)) { free(path); return rf; }
    /* pid, recorded_start, _ = <triple> */
    if (!rf || rf->type != JSON_ARRAY || json_len(rf) != 3) {
        char msg[256];
        bool was_arr = rf && rf->type == JSON_ARRAY;
        if (!rf || rf->type == JSON_NULL)
            snprintf(msg, sizeof msg, "cannot unpack non-iterable NoneType object");
        else if (rf->type != JSON_ARRAY)
            snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object", wa_type_name(rf));
        else if (json_len(rf) > 3)
            snprintf(msg, sizeof msg, "too many values to unpack (expected 3, got %zu)", json_len(rf));
        else
            snprintf(msg, sizeof msg, "not enough values to unpack (expected 3, got %zu)", json_len(rf));
        json_free(rf);
        free(path);
        return wa_err(was_arr ? "ValueError" : "TypeError", msg);
    }
    json_t *pid = json_copy(json_get(rf, 0));
    json_t *start = json_copy(json_get(rf, 1));
    json_free(rf);
    if (!pid || pid->type == JSON_NULL) {
        json_t *ua = wa_args();
        wa_set_s(ua, "path", path);
        json_t *ur = wa_call(ctx, "_unlink_quietly", ua);
        free(path);
        json_free(pid);
        json_free(start);
        if (wa_is_raise(ur)) return ur;
        json_free(ur);
        return json_null();
    }
    json_t *oa = wa_args();
    wa_set_copy(oa, "pid", pid);
    wa_set_copy(oa, "expected_start", start);
    json_t *ours = wa_call(ctx, "_bridge_pid_is_ours", oa);
    if (wa_is_raise(ours)) { free(path); json_free(pid); json_free(start); return ours; }
    bool is_ours = wa_truthy(ours);
    json_free(ours);
    if (is_ours) {
        /* suppress(OSError) around kill + info log */
        json_t *ka = wa_args();
        wa_set_copy(ka, "pid", pid);
        wa_set_n(ka, "sig", 15.0);
        json_t *kr = wa_call(ctx, "os.kill", ka);
        bool killed = false;
        if (wa_is_raise(kr)) {
            if (wa_kind_is(wa_kind(kr), "OSError")) { json_free(kr); }
            else { free(path); json_free(pid); json_free(start); return kr; }
        } else { json_free(kr); killed = true; }
        if (killed) {
            json_t *ia = wa_args();
            wa_set_s(ia, "msg", "[whatsapp] Killed stale bridge PID %d from pidfile");
            json_t *aa = json_array();
            json_append(aa, json_copy(pid));
            json_set(ia, "args", aa);
            json_t *ir = wa_call(ctx, "logger.info", ia);
            if (wa_is_raise(ir)) {
                if (wa_kind_is(wa_kind(ir), "OSError")) { json_free(ir); }
                else { free(path); json_free(pid); json_free(start); return ir; }
            } else json_free(ir);
        }
        if (!isw) {
            json_t *w1a = wa_args();
            json_t *pl = json_array();
            json_append(pl, json_copy(pid));
            json_set(w1a, "pids", pl);
            wa_set_null(w1a, "timeout");
            json_t *w1 = wa_call(ctx, "_wait_pids_exit", w1a);
            if (wa_is_raise(w1)) { free(path); json_free(pid); json_free(start); return w1; }
            bool survivors = wa_truthy(w1);
            json_free(w1);
            if (survivors) {
                json_t *o2a = wa_args();
                wa_set_copy(o2a, "pid", pid);
                wa_set_copy(o2a, "expected_start", start);
                json_t *ours2 = wa_call(ctx, "_bridge_pid_is_ours", o2a);
                if (wa_is_raise(ours2)) { free(path); json_free(pid); json_free(start); return ours2; }
                bool still = wa_truthy(ours2);
                json_free(ours2);
                if (still) {
                    json_t *k2a = wa_args();
                    wa_set_copy(k2a, "pid", pid);
                    wa_set_n(k2a, "sig", sigkill);
                    json_t *k2 = wa_call(ctx, "os.kill", k2a);
                    bool killed2 = false;
                    if (wa_is_raise(k2)) {
                        if (wa_kind_is(wa_kind(k2), "OSError")) { json_free(k2); }
                        else { free(path); json_free(pid); json_free(start); return k2; }
                    } else { json_free(k2); killed2 = true; }
                    if (killed2) {
                        json_t *w2a = wa_args();
                        wa_set_s(w2a, "msg", "[whatsapp] SIGKILLed stale bridge PID %d after it ignored SIGTERM");
                        json_t *aa = json_array();
                        json_append(aa, json_copy(pid));
                        json_set(w2a, "args", aa);
                        json_t *w2 = wa_call(ctx, "logger.warning", w2a);
                        if (wa_is_raise(w2)) {
                            if (wa_kind_is(wa_kind(w2), "OSError")) { json_free(w2); }
                            else { free(path); json_free(pid); json_free(start); return w2; }
                        } else json_free(w2);
                    }
                    /* outside the suppress: let SIGKILL land */
                    json_t *w3a = wa_args();
                    json_t *pl3 = json_array();
                    json_append(pl3, json_copy(pid));
                    json_set(w3a, "pids", pl3);
                    wa_set_n(w3a, "timeout", 1.0);
                    json_t *w3 = wa_call(ctx, "_wait_pids_exit", w3a);
                    if (wa_is_raise(w3)) { free(path); json_free(pid); json_free(start); return w3; }
                    json_free(w3);
                }
            }
        }
    } else {
        json_t *pa = wa_args();
        wa_set_copy(pa, "pid", pid);
        json_t *pe = wa_call(ctx, "_pid_exists", pa);
        if (wa_is_raise(pe)) { free(path); json_free(pid); json_free(start); return pe; }
        bool alive = wa_truthy(pe);
        json_free(pe);
        if (alive) {
            const char *reason = (!start || start->type == JSON_NULL)
                ? "legacy pidfile lacks a start-time fingerprint and cmdline substring evidence can name a stranger"
                : "it is no longer the bridge (recycled onto an unrelated process)";
            json_t *wa = wa_args();
            wa_set_s(wa, "msg", "[whatsapp] Not killing pidfile PID %d: %s; skipping to avoid killing a stranger.");
            json_t *aa = json_array();
            json_append(aa, json_copy(pid));
            json_append(aa, json_string(reason));
            json_set(wa, "args", aa);
            json_t *wr = wa_call(ctx, "logger.warning", wa);
            if (wa_is_raise(wr)) { free(path); json_free(pid); json_free(start); return wr; }
            json_free(wr);
        }
    }
    json_free(pid);
    json_free(start);
    json_t *ua = wa_args();
    wa_set_s(ua, "path", path);
    free(path);
    json_t *ur = wa_call(ctx, "_unlink_quietly", ua);
    if (wa_is_raise(ur)) return ur;
    json_free(ur);
    return json_null();
}

/* PoP: wa_check_whatsapp_requirements @ plugins/platforms/whatsapp/adapter.py:check_whatsapp_requirements */
static json_t *wa_check_whatsapp_requirements(wa_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *na = wa_args();
    wa_set_s(na, "name", "node");
    json_t *node = wa_call(ctx, "find_node_executable", na);
    if (wa_is_raise(node)) return node;
    if (!wa_truthy(node)) {
        json_free(node);
        return wa_call(ctx, "lazy_installs_allowed", wa_args());
    }
    /* try region: env seam + run seam + returncode read; except Exception */
    json_t *env = wa_call(ctx, "with_hermes_node_path", wa_args());
    if (wa_is_raise(env)) {
        json_free(node);
        if (wa_is_base_exc(wa_kind(env))) return env;
        json_free(env);
        return json_bool(false);
    }
    json_t *argv = json_array();
    json_append(argv, json_copy(node));
    json_append(argv, json_string("--version"));
    json_free(node);
    json_t *r = wa_call(ctx, "subprocess.run", wa_run_args(argv, 5.0, true, NULL, env));
    json_free(env);
    if (wa_is_raise(r)) {
        if (wa_is_base_exc(wa_kind(r))) return r;
        json_free(r);
        return json_bool(false);
    }
    json_t *err = NULL;
    const json_t *rc = wa_attr(r, "returncode", &err);
    if (err) { json_free(r); json_free(err); return json_bool(false); }
    json_t *zero = json_number(0.0);
    bool ok = wa_py_eq(rc, zero);
    json_free(zero);
    json_free(r);
    return json_bool(ok);
}

/* PoP: wa__file_content_hash @ plugins/platforms/whatsapp/adapter.py:_file_content_hash */
static json_t *wa__file_content_hash(wa_ctx_t *ctx, const json_t *args) {
    json_t *a = wa_args();
    wa_set_copy(a, "path", json_obj_get(args, "path"));
    json_t *r = wa_call(ctx, "read_bytes", a);
    if (wa_is_raise(r)) {
        if (wa_kind_is(wa_kind(r), "OSError")) { json_free(r); return json_string(""); }
        return r;
    }
    const char *s = wa_str(r);
    if (!s) {
        json_free(r);
        return wa_err("TypeError", "object supporting the buffer API required");
    }
    char *hex = wa_sha256_hex(s, strlen(s));
    json_free(r);
    hex[16] = '\0';
    json_t *out = json_string(hex);
    free(hex);
    return out;
}

/* PoP: wa__bridge_source_hash @ plugins/platforms/whatsapp/adapter.py:_bridge_source_hash */
static json_t *wa__bridge_source_hash(wa_ctx_t *ctx, const json_t *args) {
    const char *bp = json_get_str(args, "bridge_path", "");
    char *paths[2];
    paths[0] = wa_strdup(bp);
    paths[1] = wa_with_name(bp, "bridge_auth.js");
    wa_sha256_t c;
    wa_sha256_init(&c);
    for (size_t i = 0; i < 2; i++) {
        json_t *a = wa_args();
        wa_set_s(a, "path", paths[i]);
        json_t *r = wa_call(ctx, "read_bytes", a);
        if (wa_is_raise(r)) {
            free(paths[0]); free(paths[1]);
            if (wa_kind_is(wa_kind(r), "OSError")) { json_free(r); return json_string(""); }
            return r;
        }
        const char *s = wa_str(r);
        if (!s) {
            free(paths[0]); free(paths[1]);
            json_free(r);
            char msg[256];
            snprintf(msg, sizeof msg, "can't concat %s to bytes", wa_type_name(r));
            return wa_err("TypeError", msg);
        }
        char *name = wa_path_name(paths[i]);
        wa_sha256_update(&c, name, strlen(name));
        free(name);
        wa_sha256_update(&c, "\0", 1);
        wa_sha256_update(&c, s, strlen(s));
        wa_sha256_update(&c, "\0", 1);
        json_free(r);
    }
    free(paths[0]); free(paths[1]);
    unsigned char d[32];
    wa_sha256_final(&c, d);
    char hex[65];
    wa_hex(d, 32, hex);
    hex[16] = '\0';
    return json_string(hex);
}

/* PoP: wa__spawn_bridge_process @ plugins/platforms/whatsapp/adapter.py:_spawn_bridge_process */
static json_t *wa__spawn_bridge_process(wa_ctx_t *ctx, const json_t *args) {
    json_t *cf = wa_call(ctx, "windows_detach_flags", wa_args());
    if (wa_is_raise(cf)) return cf;
    json_t *a = wa_args();
    wa_set_copy(a, "argv", json_obj_get(args, "argv"));
    wa_set_s(a, "purpose", "whatsapp-bridge");
    wa_set_n(a, "stdin", -3.0);
    wa_set_copy(a, "stdout", json_obj_get(args, "log_fh"));
    wa_set_copy(a, "stderr", json_obj_get(args, "log_fh"));
    wa_set_copy(a, "env", json_obj_get(args, "env"));
    wa_set_copy(a, "creationflags", cf);
    json_free(cf);
    const json_t *umask = json_obj_get(args, "umask");
    if (umask && umask->type != JSON_NULL) wa_set_copy(a, "umask", umask);
    else wa_set_n(a, "umask", -1.0);
    return wa_call(ctx, "spawn_owned", a);
}

/* PoP: wa__normalize_outbound_mentions @ plugins/platforms/whatsapp/adapter.py:_normalize_outbound_mentions */
static json_t *wa__normalize_outbound_mentions(wa_ctx_t *ctx, const json_t *args) {
    const json_t *mentions = json_obj_get(args, "mentions");
    json_t *out = json_array();
    if (!wa_truthy(mentions)) return out;
    if (mentions->type != JSON_ARRAY) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", wa_type_name(mentions));
        json_free(out);
        return wa_err("TypeError", msg);
    }
    size_t n = json_len(mentions);
    for (size_t i = 0; i < n; i++) {
        json_t *a = wa_args();
        wa_set_copy(a, "mention", json_get(mentions, i));
        json_t *jid = wa_call(ctx, "normalize_whatsapp_mention_jid", a);
        if (wa_is_raise(jid)) { json_free(out); return jid; }
        if (wa_truthy(jid)) {
            const char *js = wa_str(jid);
            bool dup = false;
            if (js) {
                for (size_t k = 0; k < json_len(out); k++) {
                    const char *prev = wa_str(json_get(out, k));
                    if (prev && strcmp(prev, js) == 0) { dup = true; break; }
                }
            }
            if (!dup) json_append(out, jid);
            else json_free(jid);
        } else {
            json_free(jid);
        }
    }
    return out;
}
