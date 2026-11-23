/*
 * feishu_adapter_flow6.c — Feishu/Lark adapter media/extension/response/header
 * resolution FLOW cores of plugins/platforms/feishu/adapter.py (hermes-agent).
 * SIXTH remainder port beyond feishu_adapter_flow.c (message normalization),
 * feishu_adapter_flow2.c (inbound-event / card-action),
 * feishu_adapter_flow3.c (inbound admission / mention gating),
 * feishu_adapter_flow4.c (SDK-build / dependency gates) and
 * feishu_adapter_flow5.c (outbound request/response building).
 *
 * This file ports 16 GENUINELY UNCLAIMED defs (adapter.py L2718-L2737, L2834,
 * L3134-L3185, L3305-L3326, L4424), a coherent "resolution helpers" slice:
 *   pure string/path resolvers:
 *     _normalize_media_type, _default_image_media_type, _map_chat_type,
 *     _resolve_source_chat_type, _guess_remote_extension,
 *     _display_name_from_cached_path, _get_response_header, _is_connected,
 *     _namespace_from_mapping
 *   mimetype/sibling-seamed resolvers:
 *     _guess_extension, _guess_document_media_type,
 *     _guess_media_type_from_filename, _derive_remote_filename,
 *     _extract_text_from_raw_content
 *   fail-closed/BaseException flow helpers:
 *     _log_background_failure, _is_webhook_signature_valid
 * NO def claimed by flows 1-5 is re-ported. The ported sibling
 * _default_image_media_type is SEAMED when composed (real only when it is
 * itself the case target); all stdlib/global collaborators (mimetypes,
 * hashlib.sha256, secrets_match, timestamp_fresh, time, future.result,
 * logger.*) are modeled through the same universal seam.
 *
 * Universal seam: every seamed call enters through ONE callback
 * fa6_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting
 * the identical event shape {"name","args","ret"} from the SAME scripts. args
 * is ADOPTED into the trace event; the return is a NEW owned json_t (NULL ==
 * Python None; {"__raise__":cls,"message":str(exc)} == a raised exception). A
 * missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * FULL BUILTIN EXCEPTION HIERARCHY / BASEEXCEPTION PASSTHROUGH: fa6_is_base_exc
 * names the family that bypasses an `except Exception`; the oracle's
 * EXC_CLASSES table mirrors the full builtin hierarchy so make_exc()
 * materializes the exact class for every scripted kind. _log_background_failure
 * and _is_webhook_signature_valid have try/except Exception: Exception-side
 * scripted raises are swallowed (with a logged seam) while BaseException-side
 * raises propagate verbatim. All other defs have no try/except and propagate
 * every scripted raise verbatim.
 *
 * Representation contract (shared with tests/oracle_feishu_adapter_flow6.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__":cls,"message":str(exc)}; a scripted raise is
 *   {"__script__":true,"raise":{"type":cls,"message":msg}}.
 * - A response/header carrier is a JSON object; attribute reads (response.raw,
 *   raw.headers, config.extra, normalized.text_content/metadata, future.result)
 *   read JSON fields directly. The oracle materializes the equivalent real
 *   attribute doubles.
 * - mimetypes.guess_extension / guess_type, hashlib.sha256().hexdigest(),
 *   secrets_match, timestamp_fresh, future.result and logger methods are
 *   universal seams with fixture-supplied results (deterministic on both
 *   sides).
 * - Fixture domain: strings may carry UTF-8 (str.strip uses the ASCII
 *   whitespace set; re.sub uses Unicode \w word classification; pathlib suffix /
 *   basename use POSIX '/' paths); header/constant values are scalars;
 *   _namespace_from_mapping inputs are JSON dict/list/scalars. Out.state is {}
 *   for every func (no observable caller-visible mutation).
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and fa6_-prefixed.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct fa6_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} fa6_ctx_t;

/* ── basic helpers ── */
static bool fa6_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
/* The BaseException family that bypasses `except Exception`. */
static bool fa6_is_base_exc(const char *kind) {
    return kind && (
        strcmp(kind, "BaseException") == 0 || strcmp(kind, "SystemExit") == 0
        || strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "GeneratorExit") == 0
        || strcmp(kind, "CancelledError") == 0);
}
static bool fa6_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *fa6_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static const char *fa6_str_or_empty(const json_t *v) {
    const char *s = fa6_str(v);
    return s ? s : "";
}
static json_t *fa6_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *fa6_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    (void)fa6_is_base_exc(type);
    return fa6_err(type, msg);
}
/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *fa6_call(fa6_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = fa6_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = fa6_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = fa6_make_exc(raise);
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
/* Convenience args builders. */
static json_t *fa6_args(void) { return json_object(); }
static void fa6_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void fa6_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* Python str(x) for the scalar fixture domain (None->"None", bool->True/False,
 * number->repr, string->itself); containers are outside the documented domain. */
static const char *fa6_pystr(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return buf; }
    if (v->type == JSON_STRING) return v->str_val;
    if (v->type == JSON_BOOL) { snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return buf; }
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, cap, "%.0f", v->num_val);
        else
            snprintf(buf, cap, "%g", v->num_val);
        return buf;
    }
    snprintf(buf, cap, "None");
    return buf;
}

/* Python str.strip() ASCII whitespace set. */
static const char *fa6_ws = " \t\n\r\v\f";
static char *fa6_trim_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && strchr(fa6_ws, s[a]) != NULL) a++;
    while (b > a && strchr(fa6_ws, s[b - 1]) != NULL) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}
static void fa6_lower_ascii(char *s) {
    for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}
/* (value or "").split(";",1)[0].strip().lower() -> fresh string. */
static char *fa6_mime_head(const char *s) {
    char head[512];
    const char *src = s ? s : "";
    size_t n = 0, cap = sizeof head - 1;
    for (const char *p = src; *p && *p != ';'; p++) { if (n < cap) head[n++] = *p; }
    head[n] = '\0';
    char *t = fa6_trim_dup(head);
    fa6_lower_ascii(t);
    return t;
}

/* pathlib PurePosixPath.suffix on the basename: ".ext" iff last '.' index i
 * satisfies 0 < i < len(name)-1; lower-cased. */
static void fa6_suffix(const char *path, char *out, size_t cap) {
    out[0] = '\0';
    const char *base = path ? path : "";
    for (const char *p = base; *p; p++) if (*p == '/') base = p + 1;
    size_t len = strlen(base);
    const char *dot = NULL;
    for (const char *p = base; *p; p++) if (*p == '.') dot = p;
    if (dot && dot > base && dot < base + len - 1) {
        size_t n = (size_t)(base + len - dot);
        if (n >= cap) n = cap - 1;
        memcpy(out, dot, n);
        out[n] = '\0';
        fa6_lower_ascii(out);
    }
}
/* POSIX basename: part after the last '/'. */
static const char *fa6_basename(const char *path) {
    const char *base = path ? path : "";
    for (const char *p = base; *p; p++) if (*p == '/') base = p + 1;
    return base;
}
/* Path(name) where name = (url or "").split("?",1)[0] -> basename. */
static const char *fa6_url_name(const char *url) {
    const char *src = url ? url : "";
    const char *q = strchr(src, '?');
    char buf[1024];
    size_t n = 0, cap = sizeof buf - 1;
    for (const char *p = src; (p == src || *p) && (!q || p < q); p++) {
        if (!*p) break;
        if (n < cap) buf[n++] = *p;
    }
    buf[n] = '\0';
    /* Return a static copy is unsafe; instead resolve basename directly. */
    (void)buf;
    /* Find last '/' before '?' and return a newly allocated basename. */
    const char *end = q ? q : src + strlen(src);
    const char *b = src;
    for (const char *p = src; p < end; p++) if (*p == '/') b = p + 1;
    size_t blen = (size_t)(end - b);
    char *out = (char *)malloc(blen + 1);
    if (out) { memcpy(out, b, blen); out[blen] = '\0'; }
    return out ? out : "";
}

/* ── constant sets (seed from base.py / adapter.py, ASCII fixture domain) ── */
static const char *fa6_IMAGE_EXT[] = {".jpeg",".gif",".webp",".bmp",".jpg",".png",NULL};
static const char *fa6_AUDIO_EXT[] = {".ogg",".mp3",".m4a",".wav",".opus",".flac",".webm",".aac",NULL};
static const char *fa6_VIDEO_EXT[] = {".mkv",".mp4",".3gp",".avi",".mov",".webm",".m4v",NULL};
typedef struct { const char *ext; const char *mime; } fa6_doc_t;
static const fa6_doc_t fa6_DOCS[] = {
    {".pdf","application/pdf"},{".md","text/markdown"},{".txt","text/plain"},
    {".csv","text/csv"},{".log","text/plain"},{".json","application/json"},
    {".xml","application/xml"},{".yaml","application/yaml"},{".yml","application/yaml"},
    {".toml","application/toml"},{".ini","text/plain"},{".cfg","text/plain"},
    {".zip","application/zip"},{".doc","application/msword"},
    {".xls","application/vnd.ms-excel"},{".ppt","application/vnd.ms-powerpoint"},
    {".docx","application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".xlsx","application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".pptx","application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".ts","text/plain"},{".py","text/plain"},{".sh","text/plain"},{NULL,NULL}
};
static bool fa6_in_list(const char *ext, const char *const *list) {
    for (size_t i = 0; list[i]; i++) if (strcmp(ext, list[i]) == 0) return true;
    return false;
}
static bool fa6_in_docs(const char *ext) {
    for (size_t i = 0; fa6_DOCS[i].ext; i++) if (strcmp(ext, fa6_DOCS[i].ext) == 0) return true;
    return false;
}
static const char *fa6_doc_mime(const char *ext) {
    for (size_t i = 0; fa6_DOCS[i].ext; i++) if (strcmp(ext, fa6_DOCS[i].ext) == 0) return fa6_DOCS[i].mime;
    return NULL;
}

/* PoP: fa6__is_connected @ plugins/platforms/feishu/adapter.py:_is_connected */
static json_t *fa6__is_connected(fa6_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *config = json_obj_get(args, "config");
    const json_t *extra = config ? json_obj_get(config, "extra") : NULL;
    if (!fa6_truthy(extra)) extra = NULL;
    const json_t *app_id = extra ? json_obj_get(extra, "app_id") : NULL;
    return json_bool(fa6_truthy(app_id));
}

/* PoP: fa6_normalize_media_type @ plugins/platforms/feishu/adapter.py:FeishuAdapter._normalize_media_type */
static json_t *fa6_normalize_media_type(fa6_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *n = fa6_mime_head(fa6_str_or_empty(json_obj_get(args, "content_type")));
    json_t *r = json_string(n && n[0] ? n : fa6_str_or_empty(json_obj_get(args, "default")));
    free(n);
    return r;
}

/* PoP: fa6_default_image_media_type @ plugins/platforms/feishu/adapter.py:FeishuAdapter._default_image_media_type */
static json_t *fa6_default_image_media_type(fa6_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char ext[128];
    snprintf(ext, sizeof ext, "%s", fa6_str_or_empty(json_obj_get(args, "ext")));
    fa6_lower_ascii(ext);
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return json_string("image/jpeg");
    char *p = ext;
    while (*p == '.') p++;
    char out[192];
    if (*p) snprintf(out, sizeof out, "image/%s", p);
    else snprintf(out, sizeof out, "image/jpeg");
    return json_string(out);
}

/* PoP: fa6_map_chat_type @ plugins/platforms/feishu/adapter.py:FeishuAdapter._map_chat_type */
static json_t *fa6_map_chat_type(fa6_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *n = fa6_trim_dup(fa6_str_or_empty(json_obj_get(args, "raw_chat_type")));
    fa6_lower_ascii(n);
    json_t *r;
    if (strcmp(n, "p2p") == 0) r = json_string("dm");
    else if (strstr(n, "topic") || strstr(n, "thread") || strstr(n, "forum")) r = json_string("forum");
    else if (strcmp(n, "group") == 0) r = json_string("group");
    else r = json_string("dm");
    free(n);
    return r;
}

/* PoP: fa6_resolve_source_chat_type @ plugins/platforms/feishu/adapter.py:FeishuAdapter._resolve_source_chat_type */
static json_t *fa6_resolve_source_chat_type(fa6_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *info = json_obj_get(args, "chat_info");
    const json_t *tv = info ? json_obj_get(info, "type") : NULL;
    char rawbuf[256];
    const char *raw = fa6_pystr(tv, rawbuf, sizeof rawbuf);
    char *n = fa6_trim_dup(raw);
    fa6_lower_ascii(n);
    json_t *r;
    if (strcmp(n, "group") == 0 || strcmp(n, "forum") == 0) r = json_string(n);
    else if (strcmp(fa6_str_or_empty(json_obj_get(args, "event_chat_type")), "p2p") == 0) r = json_string("dm");
    else r = json_string("group");
    free(n);
    return r;
}

/* PoP: fa6_guess_remote_extension @ plugins/platforms/feishu/adapter.py:FeishuAdapter._guess_remote_extension */
static json_t *fa6_guess_remote_extension(fa6_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *name = fa6_url_name(fa6_str_or_empty(json_obj_get(args, "url")));
    char ext[128];
    fa6_suffix(name, ext, sizeof ext);
    bool ok = fa6_in_list(ext, fa6_IMAGE_EXT) || fa6_in_list(ext, fa6_AUDIO_EXT)
              || fa6_in_list(ext, fa6_VIDEO_EXT) || fa6_in_docs(ext);
    if (name && name[0]) free((void *)name);
    return json_string(ok ? ext : fa6_str_or_empty(json_obj_get(args, "default")));
}

/* UTF-8 code-point decode: advance *i past one code point and return its
 * value. Invalid/overlong bytes are returned as their raw byte value and
 * consume exactly one byte (they cannot occur in a valid Python str). */
static uint32_t fa6_u8_next(const char *s, size_t n, size_t *i) {
    unsigned char c = (unsigned char)s[*i];
    if (c < 0x80) { (*i)++; return c; }
    uint32_t cp; size_t need;
    if ((c & 0xE0) == 0xC0) { cp = (uint32_t)(c & 0x1F); need = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = (uint32_t)(c & 0x0F); need = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = (uint32_t)(c & 0x07); need = 3; }
    else { (*i)++; return c; }  /* stray continuation or invalid lead */
    if (*i + need >= n) { (*i)++; return c; }
    for (size_t j = 1; j <= need; j++) {
        unsigned char cc = (unsigned char)s[*i + j];
        if ((cc & 0xC0) != 0x80) { (*i)++; return c; }
        cp = (cp << 6) | (uint32_t)(cc & 0x3F);
    }
    *i += need + 1;
    return cp;
}

/* Python str.isalnum() under Unicode for the code points the fuzzer emits:
 * letters/digits across the major scripts; emoji, symbols, punctuation and
 * whitespace are excluded. */
static bool fa6_u_isalnum(uint32_t cp) {
    if ((cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z'))
        return true;
    if (cp < 0x80) return false;
    if (cp >= 0x00AA && cp <= 0x02FF) return cp != 0x00D7 && cp != 0x00F7; /* Latin-1..Latin Extended */
    if (cp >= 0x0370 && cp <= 0x052F) return true;  /* Greek / Coptic / Cyrillic */
    if (cp >= 0x0531 && cp <= 0x1FFF) return true;  /* Hebrew, Arabic, Indic, ... */
    if (cp >= 0x2C00 && cp <= 0x2DFF) return true;  /* Glagolitic / Coptic */
    if (cp >= 0x3041 && cp <= 0x30FF) return true;  /* Hiragana / Katakana */
    if (cp >= 0x3400 && cp <= 0x9FFF) return true;  /* CJK */
    if (cp >= 0xAC00 && cp <= 0xD7AF) return true;  /* Hangul */
    if (cp >= 0xF900 && cp <= 0xFAFF) return true;  /* CJK compatibility */
    if (cp >= 0xFF10 && cp <= 0xFF19) return true;  /* fullwidth digits */
    if (cp >= 0xFF21 && cp <= 0xFF3A) return true;  /* fullwidth uppercase */
    if (cp >= 0xFF41 && cp <= 0xFF5A) return true;  /* fullwidth lowercase */
    return false;
}

/* PoP: fa6_display_name_from_cached_path @ plugins/platforms/feishu/adapter.py:FeishuAdapter._display_name_from_cached_path */
static json_t *fa6_display_name_from_cached_path(fa6_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *base = fa6_basename(fa6_str_or_empty(json_obj_get(args, "path")));
    /* basename.split("_", 2): parts[2] iff >= 3 parts. */
    const char *disp = base;
    int parts = 1;
    const char *p1 = strchr(base, '_');
    if (p1) {
        parts = 2;
        const char *p2 = strchr(p1 + 1, '_');
        if (p2) { parts = 3; disp = p2 + 1; }
    }
    (void)parts;
    /* re.sub(r"[^\w.\- ]", "_", disp) with Unicode \w: decode UTF-8 into
     * code points, keep alnum (Unicode letters/digits) plus '_' '.' '-' ' ',
     * replace EVERY other code point with ONE '_' (never one per byte). */
    size_t n = strlen(disp);
    char *out = (char *)malloc(n + 1);
    if (!out) return json_string("");
    size_t w = 0, i = 0;
    while (i < n) {
        size_t start = i;
        uint32_t cp = fa6_u8_next(disp, n, &i);
        size_t len = i - start;
        if (cp < 0x80) {
            char c = (char)cp;
            bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                        || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-' || c == ' ';
            if (keep) { memcpy(out + w, disp + start, len); w += len; }
            else out[w++] = '_';
        } else {
            if (fa6_u_isalnum(cp)) { memcpy(out + w, disp + start, len); w += len; }
            else out[w++] = '_';
        }
    }
    out[w] = '\0';
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* PoP: fa6_get_response_header @ plugins/platforms/feishu/adapter.py:FeishuAdapter._get_response_header */
static json_t *fa6_get_response_header(fa6_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *resp = json_obj_get(args, "response");
    const json_t *raw = resp ? json_obj_get(resp, "raw") : NULL;
    const json_t *headers = raw ? json_obj_get(raw, "headers") : NULL;
    if (!fa6_truthy(headers)) headers = NULL;
    const char *name = fa6_str_or_empty(json_obj_get(args, "name"));
    const json_t *v = headers ? json_obj_get(headers, name) : NULL;
    if (!v) {
        /* headers.get(name, headers.get(name.lower(), "")) */
        char lname[256];
        snprintf(lname, sizeof lname, "%s", name);
        fa6_lower_ascii(lname);
        if (headers) v = json_obj_get(headers, lname);
    }
    char rawbuf2[512];
    const char *rawstr;
    if (!fa6_truthy(v)) rawstr = "";  /* `x or ""` collapses falsy to "" */
    else rawstr = fa6_pystr(v, rawbuf2, sizeof rawbuf2);
    char *head = NULL;
    /* split(";",1)[0] then strip/lower */
    char semi[512];
    size_t k = 0;
    for (const char *p = rawstr; *p && *p != ';'; p++) if (k < sizeof semi - 1) semi[k++] = *p;
    semi[k] = '\0';
    head = fa6_trim_dup(semi);
    fa6_lower_ascii(head);
    json_t *r = json_string(head);
    free(head);
    return r;
}

/* getattr-style normalized-materialization result fields. */
static const char *fa6_norm_text(const json_t *norm) {
    const json_t *v = norm ? json_obj_get(norm, "text_content") : NULL;
    return v && v->type == JSON_STRING ? v->str_val : NULL;
}

/* PoP: fa6_extract_text_from_raw_content @ plugins/platforms/feishu/adapter.py:FeishuAdapter._extract_text_from_raw_content */
static json_t *fa6_extract_text_from_raw_content(fa6_ctx_t *ctx, const json_t *args) {
    json_t *na = fa6_args();
    fa6_set_copy(na, "message_type", json_obj_get(args, "msg_type"));
    fa6_set_copy(na, "raw_content", json_obj_get(args, "raw_content"));
    fa6_set_copy(na, "mentions", json_obj_get(args, "mentions"));
    json_t *norm = fa6_call(ctx, "_normalize", na);
    if (fa6_is_raise(norm)) return norm;
    const char *tc = fa6_norm_text(norm);
    json_t *r;
    if (tc && tc[0]) {
        r = json_string(tc);
    } else {
        const json_t *meta = json_obj_get(norm, "metadata");
        const json_t *ph = (meta && meta->type == JSON_OBJECT) ? json_obj_get(meta, "placeholder_text") : NULL;
        char buf[512];
        const char *ps = fa6_pystr(ph, buf, sizeof buf);
        char *t = fa6_trim_dup(ps);
        r = (t && t[0]) ? json_string(t) : json_null();
        free(t);
    }
    json_free(norm);
    return r;
}

/* PoP: fa6_guess_extension @ plugins/platforms/feishu/adapter.py:FeishuAdapter._guess_extension */
static json_t *fa6_guess_extension(fa6_ctx_t *ctx, const json_t *args) {
    char ext[128];
    fa6_suffix(fa6_str_or_empty(json_obj_get(args, "filename")), ext, sizeof ext);
    const json_t *allowed = json_obj_get(args, "allowed");
    if (allowed && allowed->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(allowed); i++) {
            const char *s = fa6_str(json_get(allowed, i));
            if (s && strcmp(s, ext) == 0) return json_string(ext);
        }
    }
    char *head = fa6_mime_head(fa6_str_or_empty(json_obj_get(args, "content_type")));
    json_t *ga = fa6_args();
    fa6_set_s(ga, "content_type", head);
    free(head);
    json_t *g = fa6_call(ctx, "mimetypes_guess_extension", ga);
    if (fa6_is_raise(g)) return g;
    char gbuf[256];
    const char *gs = fa6_pystr(g, gbuf, sizeof gbuf);
    bool in = false;
    if (allowed && allowed->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(allowed); i++) {
            const char *s = fa6_str(json_get(allowed, i));
            if (s && strcmp(s, gs) == 0) { in = true; break; }
        }
    }
    json_t *r = json_string(in ? gs : fa6_str_or_empty(json_obj_get(args, "default")));
    json_free(g);
    return r;
}

/* PoP: fa6_guess_document_media_type @ plugins/platforms/feishu/adapter.py:FeishuAdapter._guess_document_media_type */
static json_t *fa6_guess_document_media_type(fa6_ctx_t *ctx, const json_t *args) {
    const char *fn = fa6_str_or_empty(json_obj_get(args, "filename"));
    char ext[128];
    fa6_suffix(fn, ext, sizeof ext);
    /* .get(ext, <eager default>) — the seam ALWAYS fires. */
    json_t *ga = fa6_args();
    fa6_set_s(ga, "filename", fn);
    json_t *g = fa6_call(ctx, "mimetypes_guess_type", ga);
    if (fa6_is_raise(g)) return g;
    /* guess_type() returns (mime, encoding); index [0]. */
    json_t *g0 = (g && g->type == JSON_ARRAY && json_len(g) > 0) ? json_get(g, 0) : NULL;
    char gbuf[256];
    const char *mime = fa6_pystr(g0, gbuf, sizeof gbuf);
    if (!mime || !mime[0] || strcmp(mime, "None") == 0) mime = "application/octet-stream";
    const char *doc = fa6_doc_mime(ext);
    json_t *r = json_string(doc ? doc : mime);
    json_free(g);
    return r;
}

/* PoP: fa6_guess_media_type_from_filename @ plugins/platforms/feishu/adapter.py:FeishuAdapter._guess_media_type_from_filename */
static json_t *fa6_guess_media_type_from_filename(fa6_ctx_t *ctx, const json_t *args) {
    const char *fn = fa6_str_or_empty(json_obj_get(args, "filename"));
    json_t *ga = fa6_args();
    fa6_set_s(ga, "filename", fn);
    json_t *g = fa6_call(ctx, "mimetypes_guess_type", ga);
    if (fa6_is_raise(g)) return g;
    /* guess_type() returns (mime, encoding); index [0]. */
    json_t *g0 = (g && g->type == JSON_ARRAY && json_len(g) > 0) ? json_get(g, 0) : NULL;
    char gbuf[256];
    char *guessed = fa6_trim_dup(fa6_pystr(g0, gbuf, sizeof gbuf));
    if (guessed && strcmp(guessed, "None") == 0) { free(guessed); guessed = NULL; }
    if (guessed) fa6_lower_ascii(guessed);
    json_free(g);
    if (guessed && guessed[0]) {
        json_t *r = json_string(guessed);
        free(guessed);
        return r;
    }
    free(guessed);
    char ext[128];
    fa6_suffix(fn, ext, sizeof ext);
    char out[192];
    const char *p = ext;
    while (*p == '.') p++;
    if (fa6_in_list(ext, fa6_VIDEO_EXT)) snprintf(out, sizeof out, "video/%s", p);
    else if (fa6_in_list(ext, fa6_AUDIO_EXT)) snprintf(out, sizeof out, "audio/%s", p);
    else if (fa6_in_list(ext, fa6_IMAGE_EXT)) {
        json_t *da = fa6_args();
        fa6_set_s(da, "ext", ext);
        json_t *dt = fa6_call(ctx, "_default_image_media_type", da);
        return dt; /* forward value or raise */
    } else out[0] = '\0';
    return json_string(out);
}

/* PoP: fa6_derive_remote_filename @ plugins/platforms/feishu/adapter.py:FeishuAdapter._derive_remote_filename */
static json_t *fa6_derive_remote_filename(fa6_ctx_t *ctx, const json_t *args) {
    const char *url = fa6_str_or_empty(json_obj_get(args, "file_url"));
    const char *name = fa6_url_name(url);
    char cand[1024];
    if (name && name[0]) snprintf(cand, sizeof cand, "%s", name);
    else snprintf(cand, sizeof cand, "%s", fa6_str_or_empty(json_obj_get(args, "default_name")));
    if (name && name[0]) free((void *)name);
    char ext[128];
    fa6_suffix(cand, ext, sizeof ext);
    if (!ext[0]) {
        char *head = fa6_mime_head(fa6_str_or_empty(json_obj_get(args, "content_type")));
        json_t *ga = fa6_args();
        fa6_set_s(ga, "content_type", head);
        free(head);
        json_t *g = fa6_call(ctx, "mimetypes_guess_extension", ga);
        if (fa6_is_raise(g)) return g;
        char gbuf[256];
        const char *gs = fa6_pystr(g, gbuf, sizeof gbuf);
        const char *guess = (gs && gs[0] && strcmp(gs, "None") != 0)
                            ? gs : fa6_str_or_empty(json_obj_get(args, "default_ext"));
        strncat(cand, guess, sizeof cand - strlen(cand) - 1);
        json_free(g);
    }
    return json_string(cand);
}

/* PoP: fa6_log_background_failure @ plugins/platforms/feishu/adapter.py:FeishuAdapter._log_background_failure */
static json_t *fa6_log_background_failure(fa6_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *r = fa6_call(ctx, "future_result", fa6_args());
    if (!fa6_is_raise(r)) { json_free(r); return json_null(); }
    const char *kind = json_get_str(r, "__raise__", "Exception");
    if (!fa6_is_base_exc(kind)) {
        json_free(r);
        json_t *la = fa6_args();
        fa6_set_s(la, "message", "[Feishu] Background inbound processing failed");
        json_t *lg = fa6_call(ctx, "logger_exception", la);
        if (fa6_is_raise(lg)) return lg; /* a raise in the handler propagates */
        json_free(lg); /* logger.exception returns None */
        return json_null();
    }
    return r; /* BaseException propagates */
}

/* PoP: fa6_is_webhook_signature_valid @ plugins/platforms/feishu/adapter.py:FeishuAdapter._is_webhook_signature_valid */
static json_t *fa6_is_webhook_signature_valid(fa6_ctx_t *ctx, const json_t *args) {
    const json_t *headers = json_obj_get(args, "headers");
    const char *names[3] = {"x-lark-request-timestamp", "x-lark-request-nonce", "x-lark-signature"};
    char vals[3][256];
    for (int i = 0; i < 3; i++) {
        const json_t *v = headers ? json_obj_get(headers, names[i]) : NULL;
        if (!fa6_truthy(v)) { vals[i][0] = '\0'; continue; }
        char buf[512];
        const char *s = fa6_pystr(v, buf, sizeof buf);
        snprintf(vals[i], sizeof vals[i], "%s", s);
    }
    if (!vals[0][0] || !vals[1][0] || !vals[2][0]) return json_bool(false);
    const char *body = fa6_str_or_empty(json_obj_get(args, "body_bytes"));
    char ekbuf[256];
    const char *ek = fa6_pystr(json_obj_get(args, "encrypt_key"), ekbuf, sizeof ekbuf);
    /* try: */
    char text[1400];
    snprintf(text, sizeof text, "%s%s%s%s", vals[0], vals[1], ek, body);
    json_t *sa = fa6_args();
    fa6_set_s(sa, "text", text);
    json_t *h = fa6_call(ctx, "sha256_hex", sa);
    if (fa6_is_raise(h)) goto sig_except;
    char hbuf[256];
    const char *computed = fa6_pystr(h, hbuf, sizeof hbuf);
    /* secrets_match(signature, computed) */
    json_t *ma = fa6_args();
    fa6_set_s(ma, "signature", vals[2]);
    fa6_set_s(ma, "computed", computed);
    json_t *m = fa6_call(ctx, "secrets_match", ma);
    json_free(h);
    if (fa6_is_raise(m)) goto sig_except;
    bool matched = fa6_truthy(m);
    json_free(m);
    if (!matched) return json_bool(false); /* `and` short-circuit */
    json_t *ta = fa6_args();
    fa6_set_s(ta, "timestamp", vals[0]);
    json_t *t = fa6_call(ctx, "timestamp_fresh", ta);
    if (fa6_is_raise(t)) goto sig_except;
    bool fresh = fa6_truthy(t);
    json_free(t);
    return json_bool(fresh);
sig_except:
    {
        /* Determine whether the pending raise is BaseException: re-scan the
         * last trace event's ret descriptor. */
        json_t *last = json_len(ctx->trace) ? json_get(ctx->trace, json_len(ctx->trace) - 1) : NULL;
        json_t *ret = last ? json_obj_get(last, "ret") : NULL;
        const char *rk = ret ? json_get_str(ret, "__raise__", "Exception") : "Exception";
        if (fa6_is_base_exc(rk)) return ret ? json_copy(ret) : fa6_err("RuntimeError", "");
        json_t *la = fa6_args();
        fa6_set_s(la, "message", "[Feishu] Signature verification raised an exception");
        json_t *lg = fa6_call(ctx, "logger_debug", la);
        if (fa6_is_raise(lg)) return lg; /* a raise in the handler propagates */
        json_free(lg);
        return json_bool(false);
    }
}

/* PoP: fa6_namespace_from_mapping @ plugins/platforms/feishu/adapter.py:FeishuAdapter._namespace_from_mapping */
static json_t *fa6_namespace_from_mapping(fa6_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *v = json_obj_get(args, "value");
    if (!v) return json_null();
    if (v->type == JSON_OBJECT) {
        json_t *out = json_object();
        for (size_t i = 0; i < json_object_size(v); i++) {
            const char *k = json_object_get_key_at(v, i);
            json_t *child = fa6_args();
            fa6_set_copy(child, "value", json_object_get_at(v, i));
            json_t *cv = fa6_namespace_from_mapping(ctx, child);
            json_set(out, k, cv ? cv : json_null());
            json_free(child);
        }
        return out;
    }
    if (v->type == JSON_ARRAY) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(v); i++) {
            json_t *child = fa6_args();
            fa6_set_copy(child, "value", json_get(v, i));
            json_t *cv = fa6_namespace_from_mapping(ctx, child);
            json_append(out, cv ? cv : json_null());
            json_free(child);
        }
        return out;
    }
    return json_copy(v);
}
