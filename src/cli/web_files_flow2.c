/*
 * web_files_flow2.c — hermes_cli/web_routers/files.py UNCLAIMED FLOW (C11 port).
 * LIVE authority: C:/Users/eman5/hermes-agent/hermes_cli/web_routers/files.py
 *
 * A CRLF-aware PoP grep over src/ + tests/ shows these names ALREADY claimed by
 * other ports (seamed here or simply not re-ported, NEVER re-claimed):
 *   _is_sensitive_filename / _is_sensitive_path (web_server_managed_files.c)
 *   _fs_regular_file / _fs_find_git_root / _fs_git_branch (web_server.c, web_server_fs.c)
 *   _fs_mime_type (web_server_schema_path.c), _fs_default_cwd / fs_default_cwd /
 *   fs_git_root / fs_list / fs_read_text (web_server_fs.c, web_dashboard.c)
 *   fs_write_text / fs_read_data_url (web_server_fs.c)
 *   _managed_file_response / stream_managed_file / fs_download (web_server_gaps.c)
 *   _chat_image_extension (ws_revive3.c)  <-- seamed, not re-ported
 *
 * The FIVE functions below carry NO existing PoP claim, so this file claims them:
 *
 *   PoP wf2_media_proxy_host_allowed        @ files.py:_media_proxy_host_allowed
 *   PoP wf2_fs_looks_binary                 @ files.py:_fs_looks_binary
 *   PoP wf2_sanitize_chat_image_filename    @ files.py:_sanitize_chat_image_filename
 *   PoP wf2_decode_data_url                 @ files.py:_decode_data_url
 *   PoP wf2_decode_chat_image_upload        @ files.py:_decode_chat_image_upload
 *
 * They form the chat-image-upload / media-helper FLOW: _decode_chat_image_upload
 * composes _decode_data_url (ported here) with the CLAIMED _chat_image_extension
 * (a universal seam). The other three are pure helpers on the same surface.
 *
 * Contract (identical on the harness and in tests/oracle_web_files_flow2.py):
 *  - Every wf2_* entry returns a NEW json_t* (caller frees). NULL == python None.
 *  - A {"__raise__": cls, "msg": msg} object == a propagated exception. Scripted
 *    seam raises round-trip verbatim; HTTPException raises from the ported funcs
 *    use msg == "<status>: <detail>" (starlette HTTPException.__str__).
 *  - ONE universal seam: json_t *(*call)(void *ctx, const char *name,
 *                                        const json_t *args). `name` identifies
 *    the live call, `args` is a BORROWED json object (seam records a copy; do not
 *    free), the result is NEW (caller frees). The harness records one ordered
 *    {"ev":name,"args":...,"ret":...} trace event per seam call.
 *  - Fail-closed: no seam installed -> RuntimeError raise spec; the harness is
 *    also fail-closed on a MISSING script. A scripted null == None (NULL pointer,
 *    never a JSON_NULL node). An ARRAY script is a per-call queue (last repeats).
 *
 * FULL BUILTIN EXCEPTION HIERARCHY: TT2_EXC_TREE carries every builtin exception
 * child->parent so wf2_cls_is/wf2_is_base_exc fold a scripted subclass into its
 * ancestor exactly like isinstance(). _decode_chat_image_upload has NO except
 * clause, so BaseException kinds (SystemExit/KeyboardInterrupt/GeneratorExit and
 * the *Group family) pass through as verbatim as ordinary Exceptions; the table
 * classifies them for the record but never swallows them.
 *
 * Fixture domain (identical on both sides): ASCII text; integral JSON numbers
 * read as ints; byte payloads are represented as base64 strings (data URLs) or
 * arrays of int bytes (_fs_looks_binary). str.strip() uses the ASCII White_Space
 * set; Path.name uses Windows semantics (both '/' and '\\' separators, as the
 * oracle runs on the Windows venv). The two byte caps are fixture-supplied so
 * the 413 branches are reachable without allocating megabyte payloads.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include "json.h"

/* ── JSON / Python-value helpers ── */
static bool wf2_is_obj(const json_t *v)  { return v && v->type == JSON_OBJECT; }
static bool wf2_is_arr(const json_t *v)  { return v && v->type == JSON_ARRAY; }
static bool wf2_is_str(const json_t *v)  { return v && v->type == JSON_STRING; }
static bool wf2_is_num(const json_t *v)  { return v && v->type == JSON_NUMBER; }
static bool wf2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool wf2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static char *wf2_xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}

/* ── universal seam ── */
typedef struct wf2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} wf2_seams_t;

static json_t *wf2_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static bool wf2_is_raise(const json_t *r) {
    return wf2_is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *wf2_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
/* Ownership: wf2_call OWNS args (frees it after the call); the seam BORROWS args.
 * Results are NEW (caller frees). */
static json_t *wf2_call(const wf2_seams_t *s, const char *name, json_t *args) {
    if (!s || !s->call) {
        json_free(args);
        return wf2_raise("RuntimeError", "no seam installed");
    }
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}

/* ── FULL builtin exception hierarchy (child -> parent) ── */
typedef struct wf2_exc_pair { const char *child; const char *parent; } wf2_exc_pair_t;
static const wf2_exc_pair_t WF2_EXC_TREE[] = {
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
    {"GeneratorExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"},
    {"Exception", "BaseException"},
    {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"OSError", "Exception"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"Warning", "Exception"},
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
};
static const char *wf2_exc_parent(const char *c) {
    for (size_t i = 0; i < sizeof(WF2_EXC_TREE) / sizeof(WF2_EXC_TREE[0]); i++)
        if (!strcmp(WF2_EXC_TREE[i].child, c)) return WF2_EXC_TREE[i].parent;
    return NULL;
}
static bool wf2_cls_is(const char *c, const char *ancestor) {
    if (!c || !*c) return false;
    while (c) {
        if (!strcmp(c, ancestor)) return true;
        c = wf2_exc_parent(c);
    }
    return false;
}
static bool wf2_is_base_exc(const char *c) {
    return wf2_cls_is(c, "BaseException") && !wf2_cls_is(c, "Exception");
}

/* ── string / path helpers ── */
static bool wf2_is_ws(unsigned char c) {
    return c == ' ' || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}
/* str.strip() for the ASCII White_Space set. */
static char *wf2_strip(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && wf2_is_ws((unsigned char)s[a])) a++;
    while (b > a && wf2_is_ws((unsigned char)s[b - 1])) b--;
    char *out = malloc(b - a + 1);
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}
static void wf2_lower_ascii(char *s) {
    for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s + 32);
}
/* Path(...).name under Windows semantics: both separators, trailing seps ignored,
 * "." -> "" (Path(".") normalizes to itself with empty name). */
static char *wf2_path_name(const char *p) {
    size_t n = strlen(p);
    while (n > 0 && (p[n - 1] == '/' || p[n - 1] == '\\')) n--;
    size_t start = 0;
    for (size_t i = 0; i < n; i++)
        if (p[i] == '/' || p[i] == '\\') start = i + 1;
    /* Windows drive prefix with no following separator: the drive is not part
     * of Path(...).name, so a drive-only path has an empty name and the
     * sanitizer falls back to "pasted-image". Drive-relative "C:foo" keeps
     * "foo" as the name. */
    if (start == 0 && n >= 2 && p[1] == ':' &&
        ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z') ||
         (p[0] >= '0' && p[0] <= '9')))
        start = 2;
    char *out = malloc(n - start + 1);
    memcpy(out, p + start, n - start);
    out[n - start] = '\0';
    if (!strcmp(out, ".")) { out[0] = '\0'; }
    return out;
}

/* ── strict base64 (accept/reject identical to base64.b64decode(validate=True)) ── */
static int wf2_b64val(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
/* Returns malloc'd bytes (empty input -> 0-byte allocation) or NULL on invalid. */
static unsigned char *wf2_b64decode_strict(const char *s, size_t *out_len) {
    size_t n = s ? strlen(s) : 0;
    unsigned char *vals = malloc(n ? n : 1);
    size_t d = 0, p = 0;
    bool saw_pad = false;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        int v = wf2_b64val(c);
        if (v >= 0) {
            if (saw_pad) { free(vals); return NULL; }      /* data after padding */
            vals[d++] = (unsigned char)v;
        } else if (c == '=') {
            if (i == 0) { free(vals); return NULL; }        /* leading padding */
            saw_pad = true; p++;
            if (p > 2) { free(vals); return NULL; }          /* excess padding */
        } else {
            free(vals); return NULL;                        /* invalid char */
        }
    }
    if (d % 4 == 1) { free(vals); return NULL; }            /* 1 mod 4 data chars */
    if (p == 0) { if (d % 4 != 0) { free(vals); return NULL; } }
    else if (p == 1) { if (d % 4 != 3) { free(vals); return NULL; } }
    else { if (d % 4 != 2) { free(vals); return NULL; } }
    unsigned char *out = malloc(d / 4 * 3 + 3 + 1);
    size_t k = 0;
    for (size_t i = 0; i < d; i += 4) {
        size_t g = d - i < 4 ? d - i : 4;
        unsigned int acc = 0;
        for (size_t j = 0; j < g; j++) acc = (acc << 6) | vals[i + j];
        acc <<= (4 - g) * 6;
        for (size_t j = 0; j + 1 < g; j++)
            out[k++] = (unsigned char)((acc >> (16 - 8 * j)) & 0xff);
    }
    free(vals);
    *out_len = k;
    return out;
}
static char *wf2_b64encode(const unsigned char *data, size_t len) {
    static const char T[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char *out = malloc(((len + 2) / 3) * 4 + 1);
    size_t k = 0;
    for (size_t i = 0; i < len; i += 3) {
        size_t g = len - i < 3 ? len - i : 3;
        unsigned int acc = 0;
        for (size_t j = 0; j < g; j++) acc = (acc << 8) | data[i + j];
        acc <<= (3 - g) * 8;
        out[k++] = T[(acc >> 18) & 63];
        out[k++] = T[(acc >> 12) & 63];
        out[k++] = g >= 2 ? T[(acc >> 6) & 63] : '=';
        out[k++] = g >= 3 ? T[acc & 63] : '=';
    }
    out[k] = '\0';
    return out;
}

/* ═════════════ _media_proxy_host_allowed ═════════════ */
static const char *WF2_ALLOWED_HOSTS[] = {
    "fal.media", "fal.run", "v3.fal.media", "storage.googleapis.com",
    "*.fal.media", "*.fal.run",
};
/* PoP: wf2_media_proxy_host_allowed @ hermes_cli/web_routers/files.py:_media_proxy_host_allowed */
static json_t *wf2_media_proxy_host_allowed(const char *host_in) {
    char *host = strdup(host_in ? host_in : "");
    wf2_lower_ascii(host);
    size_t hl = strlen(host);
    while (hl > 0 && host[hl - 1] == '.') host[--hl] = '\0';
    bool ok = false;
    for (size_t i = 0; i < sizeof(WF2_ALLOWED_HOSTS) / sizeof(WF2_ALLOWED_HOSTS[0]); i++) {
        const char *a = WF2_ALLOWED_HOSTS[i];
        const char *al = a;
        while (*al == '*' || *al == '.') al++;           /* allowed.lstrip("*.") */
        if (strcmp(host, al) == 0) { ok = true; break; }
        if (a[0] == '*' && a[1] == '.') {
            const char *suf = a + 1;                      /* allowed[1:] == ".fal.media" */
            size_t sl = strlen(suf);
            if (hl >= sl && strcmp(host + hl - sl, suf) == 0) { ok = true; break; }
        }
    }
    free(host);
    return json_bool(ok);
}

/* ═════════════ _fs_looks_binary ═════════════ */
/* PoP: wf2_fs_looks_binary @ hermes_cli/web_routers/files.py:_fs_looks_binary */
static json_t *wf2_fs_looks_binary(const json_t *arr) {
    size_t n = wf2_is_arr(arr) ? json_len(arr) : 0;
    if (n == 0) return json_bool(false);
    bool has_zero = false;
    size_t suspicious = 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *it = json_get(arr, i);
        long b = (it && it->type == JSON_NUMBER) ? (long)it->num_val : 0L;
        if (b == 0) has_zero = true;
        if (b < 32 && b != 9 && b != 10 && b != 13) suspicious++;
    }
    if (has_zero) return json_bool(true);
    return json_bool(((double)suspicious / (double)n) > 0.12);
}

/* ═════════════ _sanitize_chat_image_filename ═════════════ */
/* PoP: wf2_sanitize_chat_image_filename @ hermes_cli/web_routers/files.py:_sanitize_chat_image_filename */
static char *wf2_sanitize_chat_image_filename(const char *filename) {
    char *raw = wf2_strip(filename ? filename : "");      /* str(filename or "").strip() */
    char *name = wf2_path_name(raw);                      /* Path(...).name */
    free(raw);
    size_t n = strlen(name);
    char *tmp = malloc(n + 1);                            /* control runs -> single '_' */
    size_t k = 0;
    for (size_t i = 0; i < n; ) {
        if ((unsigned char)name[i] <= 0x1f) {
            tmp[k++] = '_';
            while (i < n && (unsigned char)name[i] <= 0x1f) i++;
        } else {
            tmp[k++] = name[i++];
        }
    }
    tmp[k] = '\0';
    free(name);
    char *s2 = wf2_strip(tmp);                            /* .strip() */
    free(tmp);
    size_t a = 0, b = strlen(s2);                         /* .strip(".") */
    while (a < b && s2[a] == '.') a++;
    while (b > a && s2[b - 1] == '.') b--;
    char *out = malloc(b - a + 1);
    memcpy(out, s2 + a, b - a);
    out[b - a] = '\0';
    free(s2);
    if (*out == '\0') { free(out); return strdup("pasted-image"); }
    return out;
}

/* ═════════════ _decode_data_url ═════════════ */
/* Returns {"data_b64":..., "mime":...} or a raise spec. */
/* PoP: wf2_decode_data_url @ hermes_cli/web_routers/files.py:_decode_data_url */
static json_t *wf2_decode_data_url(const char *data_url, double max_bytes) {
    char *text = wf2_strip(data_url ? data_url : "");
    size_t tl = strlen(text);
    bool starts = tl >= 5 && strncmp(text, "data:", 5) == 0;
    char *comma = strchr(text, ',');
    if (!starts || !comma) {
        free(text);
        return wf2_raise("HTTPException", "400: Upload payload must be a data URL");
    }
    size_t hlen = (size_t)(comma - text);
    char *header = malloc(hlen + 1);
    memcpy(header, text, hlen); header[hlen] = '\0';
    const char *encoded = text + hlen + 1;
    const char *after5 = header + 5;
    const char *semi = strchr(after5, ';');
    char *mime;
    if (semi) {
        size_t ml = (size_t)(semi - after5);
        mime = malloc(ml + 1); memcpy(mime, after5, ml); mime[ml] = '\0';
    } else {
        mime = strdup(after5);
    }
    if (*mime == '\0') { free(mime); mime = strdup("application/octet-stream"); }
    bool has_b64 = strstr(header, ";base64") != NULL;
    free(header);
    if (!has_b64) {
        free(mime); free(text);
        return wf2_raise("HTTPException", "400: Upload payload must be base64 encoded");
    }
    size_t dlen = 0;
    unsigned char *data = wf2_b64decode_strict(encoded, &dlen);
    free(text);
    if (!data) {
        free(mime);
        return wf2_raise("HTTPException", "400: Upload payload is not valid base64");
    }
    if ((double)dlen > max_bytes) {
        free(data); free(mime);
        return wf2_raise("HTTPException", "413: File is too large");
    }
    char *b64 = wf2_b64encode(data, dlen);
    free(data);
    json_t *r = json_object();
    json_set(r, "data_b64", json_string(b64));
    json_set(r, "mime", json_string(mime));
    free(b64); free(mime);
    return r;
}

/* ═════════════ _decode_chat_image_upload ═════════════ */
static const char *WF2_ALLOWED_EXTS[] = { ".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp" };
/* PoP: wf2_decode_chat_image_upload @ hermes_cli/web_routers/files.py:_decode_chat_image_upload */
static json_t *wf2_decode_chat_image_upload(const wf2_seams_t *sp, const char *data_url,
                                             double manage_max, double chat_max) {
    json_t *d = wf2_decode_data_url(data_url, manage_max);
    if (wf2_is_raise(d)) return d;                        /* propagate decode failure */
    const char *mime = json_get_str(d, "mime", "");
    const char *b64 = json_get_str(d, "data_b64", "");
    size_t dlen = 0;
    unsigned char *bytes = wf2_b64decode_strict(b64, &dlen);
    char *ml = strdup(mime); wf2_lower_ascii(ml);
    bool is_image = strncmp(ml, "image/", 6) == 0;
    free(ml);
    if (!is_image) {
        if (bytes) free(bytes);
        json_free(d);
        return wf2_raise("HTTPException", "400: Upload payload must be an image");
    }
    if ((double)dlen > chat_max) {
        long mb = (long)chat_max / (1024L * 1024L);         /* chat_max // (1024*1024) */
        char *msg = wf2_xsf("413: Image is too large; cap is %ld MB", mb);
        json_t *r = wf2_raise("HTTPException", msg);
        free(msg);
        if (bytes) free(bytes);
        json_free(d);
        return r;
    }
    json_t *args = json_object();
    json_set(args, "data_b64", json_string(b64));
    json_t *ext = wf2_call(sp, "_chat_image_extension", args);
    if (wf2_is_raise(ext)) {
        /* NO except clause: BaseException AND Exception alike pass through. */
        (void)wf2_is_base_exc(wf2_raise_class(ext));
        if (bytes) free(bytes);
        json_free(d);
        return ext;
    }
    const char *exts = (ext && ext->type == JSON_STRING) ? ext->str_val : NULL;
    bool allowed = false;
    if (exts) {
        for (size_t i = 0; i < sizeof(WF2_ALLOWED_EXTS) / sizeof(WF2_ALLOWED_EXTS[0]); i++)
            if (!strcmp(exts, WF2_ALLOWED_EXTS[i])) { allowed = true; break; }
    }
    if (!allowed) {
        if (ext) json_free(ext);
        if (bytes) free(bytes);
        json_free(d);
        return wf2_raise("HTTPException", "400: Unsupported image type");
    }
    json_t *r = json_object();
    json_set(r, "data_b64", json_string(b64));
    json_set(r, "mime", json_string(mime));
    json_set(r, "ext", json_string(exts));
    if (ext) json_free(ext);
    if (bytes) free(bytes);
    json_free(d);
    return r;
}
