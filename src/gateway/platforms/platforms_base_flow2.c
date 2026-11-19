/*
 * platforms_base_flow2.c — UNCLAIMED-remainder FLOW cores of
 * gateway/platforms/base.py (media-cache bytes cluster, docker/env path
 * helpers, media-scan masking, and three small BasePlatformAdapter methods).
 *
 * Ports the DECISION sequences (call order, branch conditions, early returns,
 * exception -> fallback / suppress / re-raise paths, mutation order) of the
 * live module. All live I/O (cache-dir getters, size validation, image/audio
 * sniffing, uuid, os.open/fdopen/write, Path probes, terminal-env reads,
 * foreign-module helpers, asyncio.to_thread dispatch and logger emission)
 * enters through the universal seam below; the cores perform no I/O.
 *
 * Sequences are observable through a pb2_trace_t: the harness wires the seam
 * to a recording stub that appends one JSON event per call (arguments +
 * scripted result) in call order; tests/oracle_platforms_base_flow2.py drives
 * the LIVE AST-extracted Python with recording monkeypatch stubs fed the SAME
 * scripts and compares ordered trace + ret + propagated exception + self
 * state.
 *
 * ── Models ──────────────────────────────────────────────────────────────────
 * bytes:       fixture "data" is a JSON string materialized as its UTF-8
 *              bytes; len(data) is the BYTE length. Seam args project bytes
 *              back via decode("utf-8", errors="replace") (== the fixture
 *              string for every JSON-representable input). The non-image
 *              snippet data[:80] slices BYTES then decodes with replacement.
 * Path:        POSIX string doubles. The oracle binds ns["Path"] to a
 *              PurePosixPath subclass (RecPath) so str() keeps forward
 *              slashes on the Windows gate host; C models paths as verbatim
 *              normalized POSIX strings (pb2_posix_norm: collapse repeated
 *              '/', drop '.' components, keep '..', strip trailing '/'
 *              except root). Fixture paths are clean absolute POSIX; the
 *              cache_document filename domain excludes backslashes / drive
 *              letters so PurePosixPath.name == WindowsPath.name.
 * Path probes: expanduser / resolve / is_file / is_dir / iterdir / mkdir /
 *              write_bytes / is_relative_to are SEAMS (RecPath recording
 *              methods); .name and '/' join are pure.
 * self:        _session_task_is_stale runs on a JSON object {_session_tasks:
 *              {key: task}}; task is null (None), {} (object without .done)
 *              or {"done": <non-empty marker>} (callable .done). The flow is
 *              read-only: out.self echoes the input copy unmutated.
 * to_thread:   asyncio.to_thread is a BLACK-BOX seam {fn, args[, kwargs]}
 *              returning the scripted result (the inner cache_* functions
 *              are ported and tested separately); the wrapper's observable
 *              contract is the delegation projection + result/raise
 *              passthrough. fn projects via __name__.
 * CachedMedia: dataclass result projects to {path, media_type, kind,
 *              display_name} (dataclasses.asdict order).
 *
 * ── Universal seam ──────────────────────────────────────────────────────────
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * name identifies the live call (table below); args is a BORROWED object;
 * the result is a NEW json_t the caller frees. NULL == Python None. A
 * scripted exception is {"__raise__": "<Kind>", "msg": ...}; pb2_raise()
 * detects it. The harness records {ev:name, args:<copy>, ret:<script>} per
 * call; a scripted JSON null returns a NULL pointer (NOT a json_null node).
 * FAIL-CLOSED: a seam with NO script entry returns a RuntimeError raise spec
 * ("unscripted seam: <name>") on BOTH sides; a scripted null is a valid None.
 * pb2_call_raw takes OWNERSHIP of args (frees them); pb2_call additionally
 * parks a raise spec into *exc and returns NULL.
 *
 * Script semantics (harness + oracle identical): io[name] is a single value
 * reused for every call, OR a bare array consumed one element per call (last
 * element repeats when exhausted). A constant LIST result is wrapped as
 * {"__script__": true, "ret": [...]}.
 *
 * None-valued argument keys are OMITTED on both sides (drop_none). Logger
 * args are recorded VERBATIM (format specifiers apply only at emission);
 * exception args normalize on BOTH sides to {"__exc__": Kind, "msg": str(e)}
 * and Path logger args project to str (the C emits str(cache_dir)).
 *
 * Exception handlers are transcribed exactly; BaseException-only kinds
 * (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError /
 * BaseException) pass through every catch site.
 *
 * Import gates: in-function `from pkg.mod import name` failures are modeled
 * by per-case input flags (<gate>_import_fail). On a failed gate the import
 * raises ModuleNotFoundError("import of <pkg.mod> halted; None in sys.modules")
 * with NO seam event (the oracle sets sys.modules[full] = None, which halts
 * the import before any attribute read); on success the stub module's
 * recorder fires as the named seam. Gates:
 *   config_import_fail    hermes_cli.config (is_managed / _secure_dir)
 *   audio_import_fail     tools.audio_container (sniff_audio_ext)
 *   cred_import_fail      tools.credential_files (get_cache_directory_mounts)
 *   policy_import_fail    gateway.media_policy (trust_recent / seconds)
 *   chunking_import_fail  gateway.platforms.base_chunking (chunk_markdown_message)
 *
 * Seam name -> live call (args keys; ret):
 *   is_managed               hermes_cli.config.is_managed {}; truthy
 *   secure_dir               hermes_cli.config._secure_dir {path}; null
 *   make_user_dir            hermes_dir_acl.make_user_dir
 *                            {path,parents,exist_ok,mode}; null (may raise)
 *   mkdir                    Path.mkdir {path,parents,exist_ok}; null
 *   uuid4_hex                uuid.uuid4().hex {}; str (core slices [:12])
 *   os_open                  os.open {path,flags,mode}; int fd (may raise).
 *                            flags == O_WRONLY|O_CREAT|O_TRUNC == 769 on the
 *                            Windows gate host (probed); mode 0o600 == 384.
 *   fdopen_write             os.fdopen(fd,"wb") + write + close {fd,data};
 *                            null (may raise)
 *   write_bytes              Path.write_bytes {path,data}; null (may raise)
 *   validate_inbound_media_size  claimed sibling {size,media_type}; null
 *   looks_like_image         base_image_sniff._looks_like_image {data}; bool
 *   sniff_audio_ext          tools.audio_container.sniff_audio_ext
 *                            {data,ext}; str
 *   get_image_cache_dir / get_audio_cache_dir / get_video_cache_dir /
 *   get_document_cache_dir   module cache-dir getters {}; str (RecPath)
 *   resolve                  Path.resolve {path,strict}; str|null (may raise)
 *   is_relative_to           Path.is_relative_to {path,other}; bool
 *   expanduser               os.path.expanduser / Path.expanduser {path};
 *                            str (may raise)
 *   is_file / is_dir         Path.is_file / Path.is_dir {path}; bool
 *   iterdir                  Path.iterdir {path}; [str,...] (list result:
 *                            script one level deeper or __script__)
 *   tenv                     claimed sibling _tenv {name,default}; str
 *   get_cache_directory_mounts  tools.credential_files
 *                            .get_cache_directory_mounts {}; [{host_path,
 *                            container_path},...] (may raise)
 *   media_delivery_trust_recent  gateway.media_policy {}; truthy
 *   media_delivery_trust_recent_seconds  gateway.media_policy {}; str
 *   get_hermes_home          hermes_constants.get_hermes_home {}; str
 *   mask_protected_spans     BasePlatformAdapter._mask_protected_spans
 *                            {text}; str (claimed sibling)
 *   mask_json_string_media   BasePlatformAdapter._mask_json_string_media
 *                            {text}; str (claimed sibling)
 *   terminal_sentinel_start  claimed sibling {text}; int
 *   blank_spans              claimed sibling _blank_spans {text,spans}; str
 *   task_done                session task .done() {}; bool
 *   chunk_markdown_message   gateway.platforms.base_chunking
 *                            .chunk_markdown_message {content,max_length,
 *                            len_fn}; [str,...] (len_fn is a presence bool)
 *   to_thread                asyncio.to_thread {fn,args[,kwargs]}; scripted
 *   log_debug / log_warning  logger.<level> {msg,args:[...]}; null
 *
 * Claimed siblings stay SEAMED, never re-ported: validate_inbound_media_size,
 * _tenv, _terminal_sentinel_start, _blank_spans, _mask_protected_spans,
 * _mask_json_string_media, _normalize_media_tag_path, _or_default (the last
 * is I/O-free: its try/except(TypeError,ValueError) semantics are inlined at
 * the single _media_delivery_recency_seconds call site with no seam event,
 * matching the live trace).
 *
 * ── Fixture domains (documented exclusions) ─────────────────────────────────
 * - data strings are ASCII except one UTF-8 snippet case (byte-slice cut).
 * - paths are clean absolute POSIX (no '..', no trailing '/', no drive
 *   letters, no backslashes) so string normalization == PurePosixPath parts
 *   equality; _parse_docker_volume_mounts container paths may carry '.' /
 *   '//' components (normalized on both sides).
 * - float results are finite: no "inf"/"1e999" custom inputs (nan IS allowed:
 *   max(0.0, nan) == 0.0). Integral floats project as JSON ints.
 * - _resolve_media_ext filenames exercise ntpath._splitext (the Windows gate
 *   host algorithm, both separators, the sepIndex==0 leading-dot scan).
 * - cache_media_bytes display re.sub fixtures are ASCII (\w == [A-Za-z0-9_]).
 * - strict json.loads grammar is pre-validated before parsing (libjson
 *   json_parse is lenient); no NaN/Infinity/surrogate fixtures.
 * - task .done markers: absent | null | non-empty dict (truthy callable).
 * - supports_exec_approval_buttons: present base/override identity tokens.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <errno.h>

#include "libjson/json.h"

/* ── trace recorder (harness infrastructure, no Python counterpart) ── */
typedef struct pb2_trace {
    json_t *events;   /* owned JSON_ARRAY; NULL until first add */
} pb2_trace_t;

static void pb2_trace_add(pb2_trace_t *t, json_t *ev) {
    if (!t) { json_free(ev); return; }
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}

/* ── universal seam ── */
typedef struct pb2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} pb2_seams_t;

/* ── raise spec ── */
static const char *pb2_raise(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return NULL;
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : NULL;
}
static const char *pb2_raise_msg(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return "";
    return json_get_str(r, "msg", "");
}
static json_t *pb2_raise_spec(const char *kind, const char *msg) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(kind));
    json_set(e, "msg", json_string(msg ? msg : ""));
    return e;
}
/* {"__exc__": kind, "msg": str(e)} — normalized exception VALUE (log args). */
static json_t *pb2_exc_norm(const json_t *spec) {
    json_t *e = json_object();
    json_set(e, "__exc__", json_string(pb2_raise(spec)));
    json_set(e, "msg", json_string(pb2_raise_msg(spec)));
    return e;
}

/* ── exception hierarchy (scripted kinds) ── */
static bool pb2_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0
                 || strcmp(kind, "KeyboardInterrupt") == 0
                 || strcmp(kind, "GeneratorExit") == 0
                 || strcmp(kind, "CancelledError") == 0
                 || strcmp(kind, "BaseException") == 0);
}
static bool pb2_is_oserror_kind(const char *kind) {
    static const char *fam[] = {
        "OSError", "ProcessLookupError", "PermissionError", "FileNotFoundError",
        "FileExistsError", "NotADirectoryError", "IsADirectoryError",
        "InterruptedError", "TimeoutError", "BlockingIOError", "ChildProcessError",
        "ConnectionError", "BrokenPipeError", "ConnectionAbortedError",
        "ConnectionRefusedError", "ConnectionResetError",
    };
    if (!kind) return false;
    for (size_t i = 0; i < sizeof(fam) / sizeof(fam[0]); i++)
        if (strcmp(kind, fam[i]) == 0) return true;
    return false;
}
/* handler: "Exception", "OSError", "OSError|RuntimeError|ValueError",
 * "TypeError|ValueError", or an exact kind name. */
static bool pb2_exc_matches(const char *kind, const char *handler) {
    if (!kind) return false;
    if (strcmp(handler, "Exception") == 0) return !pb2_is_base_exc(kind);
    if (strcmp(handler, "OSError") == 0) return pb2_is_oserror_kind(kind);
    if (strcmp(handler, "OSError|RuntimeError|ValueError") == 0)
        return pb2_is_oserror_kind(kind) || strcmp(kind, "RuntimeError") == 0
            || strcmp(kind, "ValueError") == 0;
    if (strcmp(handler, "TypeError|ValueError") == 0)
        return strcmp(kind, "TypeError") == 0 || strcmp(kind, "ValueError") == 0;
    return strcmp(kind, handler) == 0;
}

/* ── Python truthiness / None ── */
static bool pb2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool pb2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }

/* ── dynamic string buffer ── */
typedef struct { char *buf; size_t len, cap; } pb2_sb_t;
static void pb2_sb_init(pb2_sb_t *sb) { sb->buf = NULL; sb->len = 0; sb->cap = 0; }
static void pb2_sb_grow(pb2_sb_t *sb, size_t need) {
    if (sb->len + need + 1 <= sb->cap) return;
    size_t nc = sb->cap ? sb->cap : 64;
    while (nc < sb->len + need + 1) nc *= 2;
    char *nb = realloc(sb->buf, nc);
    if (!nb) return;
    sb->buf = nb; sb->cap = nc;
}
static void pb2_sb_addn(pb2_sb_t *sb, const char *s, size_t n) {
    if (!s || !n) return;
    pb2_sb_grow(sb, n);
    if (!sb->buf) return;
    memcpy(sb->buf + sb->len, s, n);
    sb->len += n;
    sb->buf[sb->len] = '\0';
}
static void pb2_sb_add(pb2_sb_t *sb, const char *s) { if (s) pb2_sb_addn(sb, s, strlen(s)); }
static void pb2_sb_addch(pb2_sb_t *sb, char c) { pb2_sb_addn(sb, &c, 1); }
static char *pb2_sb_take(pb2_sb_t *sb) {
    char *r = sb->buf ? sb->buf : strdup("");
    sb->buf = NULL; sb->len = 0; sb->cap = 0;
    return r;
}
static char *pb2_xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}
static bool pb2_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* ── UTF-8 + Unicode White_Space (str.strip / rstrip no-arg set) ── */
static size_t pb2_utf8_next(const char *s, size_t len, size_t i, unsigned *cp) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) { *cp = c; return i + 1; }
    unsigned n; unsigned v;
    if ((c & 0xE0) == 0xC0) { n = 2; v = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { n = 3; v = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { n = 4; v = c & 0x07; }
    else { *cp = c; return i + 1; }
    if (i + n > len) { *cp = c; return i + 1; }
    for (unsigned k = 1; k < n; k++) {
        unsigned char cc = (unsigned char)s[i + k];
        if ((cc & 0xC0) != 0x80) { *cp = c; return i + 1; }
        v = (v << 6) | (cc & 0x3F);
    }
    *cp = v;
    return i + n;
}
static bool pb2_is_ws_cp(unsigned cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F))
        return true;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200A) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000)
        return true;
    return false;
}
static char *pb2_strip(const char *s) {
    if (!s) return pb2_xstrdup("");
    size_t len = strlen(s);
    size_t a = 0, b = len;
    while (a < b) {
        unsigned cp; size_t nx = pb2_utf8_next(s, len, a, &cp);
        if (!pb2_is_ws_cp(cp)) break;
        a = nx;
    }
    while (b > a) {
        size_t st = b - 1;
        while (st > a && ((unsigned char)s[st] & 0xC0) == 0x80) st--;
        unsigned cp; pb2_utf8_next(s, len, st, &cp);
        if (!pb2_is_ws_cp(cp)) break;
        b = st;
    }
    char *d = malloc(b - a + 1);
    if (d) { memcpy(d, s + a, b - a); d[b - a] = '\0'; }
    return d;
}
static char *pb2_rstrip(const char *s) {
    if (!s) return pb2_xstrdup("");
    size_t len = strlen(s), b = len;
    while (b > 0) {
        size_t st = b - 1;
        while (st > 0 && ((unsigned char)s[st] & 0xC0) == 0x80) st--;
        unsigned cp; pb2_utf8_next(s, len, st, &cp);
        if (!pb2_is_ws_cp(cp)) break;
        b = st;
    }
    char *d = malloc(b + 1);
    if (d) { memcpy(d, s, b); d[b] = '\0'; }
    return d;
}
/* len(str) — codepoint count. */
static size_t pb2_utf8_len(const char *s) {
    if (!s) return 0;
    size_t len = strlen(s), i = 0, n = 0;
    while (i < len) { unsigned cp; i = pb2_utf8_next(s, len, i, &cp); n++; }
    return n;
}
/* ASCII lower (fixture domain). */
static char *pb2_lower(const char *s) {
    char *d = pb2_xstrdup(s ? s : "");
    if (d) for (char *p = d; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return d;
}

/* ── PurePosixPath str normalization + join ──
 * Exactly two leading slashes are preserved ('//a' -> '//a'); one or three+
 * collapse to '/'. '.' components drop, '..' kept, trailing '/' stripped. */
static char *pb2_posix_norm(const char *s) {
    if (!s) return pb2_xstrdup("");
    size_t len = strlen(s);
    size_t lead = 0;
    while (lead < len && s[lead] == '/') lead++;
    bool abs = lead > 0;
    const char *root = (lead == 2) ? "//" : "/";
    pb2_sb_t sb; pb2_sb_init(&sb);
    if (abs) pb2_sb_add(&sb, root);
    size_t i = lead;
    bool first = true;
    while (i < len) {
        while (i < len && s[i] == '/') i++;
        size_t st = i;
        while (i < len && s[i] != '/') i++;
        if (i == st) break;
        size_t n = i - st;
        if (n == 1 && s[st] == '.') continue;   /* drop '.' components */
        if (!first) pb2_sb_addch(&sb, '/');
        pb2_sb_addn(&sb, s + st, n);
        first = false;
    }
    return pb2_sb_take(&sb);
}
/* PurePosixPath(dir) / name : an absolute name replaces; else join by parts
 * (normalize dir first; a root '/' or '//' already ends in '/', so no extra
 * separator is added — '/' / 'b' == '/b', not '//b'). */
static char *pb2_posix_join(const char *dir, const char *name) {
    if (name && name[0] == '/') return pb2_posix_norm(name);
    char *dn = pb2_posix_norm(dir ? dir : "");
    size_t dl = strlen(dn);
    pb2_sb_t sb; pb2_sb_init(&sb);
    pb2_sb_add(&sb, dn);
    if (dl == 0 || dn[dl - 1] != '/') pb2_sb_addch(&sb, '/');
    pb2_sb_add(&sb, name ? name : "");
    free(dn);
    char *j = pb2_sb_take(&sb);
    char *r = pb2_posix_norm(j);
    free(j);
    return r;
}
/* PurePosixPath(p).name — last component after normalization ('' for root,
 * '.'; '..' kept). */
static char *pb2_posix_name(const char *p) {
    char *n = pb2_posix_norm(p ? p : "");
    size_t l = strlen(n);
    if (l == 0 || (l == 1 && n[0] == '/')) { free(n); return pb2_xstrdup(""); }
    char *slash = strrchr(n, '/');
    char *r = pb2_xstrdup(slash ? slash + 1 : n);
    free(n);
    return r;
}

/* ── Python repr() for str (ASCII fixture domain; printable non-ASCII kept) ── */
static char *pb2_py_repr_str(const char *s) {
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    pb2_sb_t sb; pb2_sb_init(&sb);
    pb2_sb_addch(&sb, q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { pb2_sb_addch(&sb, '\\'); pb2_sb_addch(&sb, (char)c); }
        else if (c == '\n') pb2_sb_add(&sb, "\\n");
        else if (c == '\r') pb2_sb_add(&sb, "\\r");
        else if (c == '\t') pb2_sb_add(&sb, "\\t");
        else if (c < 0x20 || c == 0x7F) {
            char hx[8]; snprintf(hx, sizeof(hx), "\\x%02x", c);
            pb2_sb_add(&sb, hx);
        } else pb2_sb_addch(&sb, (char)c);
    }
    pb2_sb_addch(&sb, q);
    return pb2_sb_take(&sb);
}

/* ── Python float() grammar (finite-result fixture domain) ──
 * [+-]? (inf|infinity|nan case-insensitive | number). number: int-part
 * digits with single underscores BETWEEN digits, optional '.' frac digits
 * (underscores between digits), optional [eE][+-]?exp digits; at least one
 * digit in int or frac. Returns false on ValueError. Overflow -> inf. */
static bool pb2_digits_us(const char *s, size_t *i, size_t len, bool need_first) {
    size_t st = *i;
    if (st >= len || s[st] < '0' || s[st] > '9') return false;
    (void)need_first;
    while (*i < len) {
        char c = s[*i];
        if (c >= '0' && c <= '9') { (*i)++; continue; }
        if (c == '_') {
            if (*i + 1 >= len || s[*i + 1] < '0' || s[*i + 1] > '9') return false;
            (*i)++; continue;
        }
        break;
    }
    return *i > st;
}
static bool pb2_py_float(const char *s, double *out) {
    if (!s) return false;
    char *t = pb2_strip(s);
    if (!t) return false;
    size_t len = strlen(t), i = 0;
    bool ok = false;
    double v = 0.0;
    int sign = 1;
    if (i < len && (t[i] == '+' || t[i] == '-')) { if (t[i] == '-') sign = -1; i++; }
    /* special literals */
    char *low = pb2_lower(t + i);
    if (low && (strcmp(low, "inf") == 0 || strcmp(low, "infinity") == 0)) {
        v = sign * INFINITY; ok = true;
    } else if (low && strcmp(low, "nan") == 0) {
        v = NAN; ok = true;
    }
    free(low);
    if (!ok) {
        size_t int_start = i;
        bool has_int = pb2_digits_us(t, &i, len, true);
        bool has_frac = false;
        if (i < len && t[i] == '.') {
            i++;
            has_frac = pb2_digits_us(t, &i, len, false);
        }
        if (!has_int && !has_frac) goto done;
        if (i < len && (t[i] == 'e' || t[i] == 'E')) {
            i++;
            if (i < len && (t[i] == '+' || t[i] == '-')) i++;
            if (!pb2_digits_us(t, &i, len, true)) goto done;
        }
        if (i != len) goto done;
        /* rebuild a clean C literal (drop underscores) */
        pb2_sb_t sb; pb2_sb_init(&sb);
        if (sign < 0) pb2_sb_addch(&sb, '-');
        for (size_t k = int_start; k < i; k++) if (t[k] != '_') pb2_sb_addch(&sb, t[k]);
        char *clean = pb2_sb_take(&sb);
        errno = 0;
        v = strtod(clean, NULL);
        free(clean);
        ok = true;
    }
done:
    free(t);
    if (ok && out) *out = v;
    return ok;
}

/* ── ntpath._splitext (Windows gate host; genericpath._splitext verbatim,
 * sep='\\', altsep='/', extsep='.') ── */
static void pb2_splitext(const char *p, char **root, char **ext) {
    if (!p) p = "";
    size_t len = strlen(p);
    long sepIndex = -1, dotIndex = -1;
    for (size_t i = 0; i < len; i++) {
        if (p[i] == '\\' || p[i] == '/') sepIndex = (long)i;
        if (p[i] == '.') dotIndex = (long)i;
    }
    if (dotIndex > sepIndex) {
        long filenameIndex = sepIndex + 1;
        while (filenameIndex < dotIndex) {
            if (p[filenameIndex] != '.') break;
            filenameIndex++;
        }
        if (filenameIndex < dotIndex) {
            *root = malloc((size_t)dotIndex + 1);
            if (*root) { memcpy(*root, p, (size_t)dotIndex); (*root)[dotIndex] = '\0'; }
            *ext = pb2_xstrdup(p + dotIndex);
            return;
        }
    }
    *root = pb2_xstrdup(p);
    *ext = pb2_xstrdup("");
}

/* ── strict json.loads grammar validator + parser ──
 * libjson json_parse is LENIENT (leading zeros, invalid escapes, raw control
 * chars, bare '-'/'1.'/'1e'); Python json.loads is strict. Pre-validate the
 * Python grammar here; on success hand the text to json_parse. WS is exactly
 * space/tab/nl/cr; strings forbid raw control chars and admit only the
 * \" \\ \/ \b \f \n \r \t \uXXXX escape set; no trailing commas; numbers are
 * -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?; NaN/Infinity/-Infinity
 * literals are accepted by json.loads but outside the fixture domain.
 * Duplicate keys: last value wins (json_set keeps first position). */
typedef struct { const char *s; size_t i, len; int depth; bool bad; } pb2_jp_t;
static void pb2_jp_ws(pb2_jp_t *p) {
    while (p->i < p->len) {
        char c = p->s[p->i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') p->i++;
        else break;
    }
}
static bool pb2_jp_hex4(pb2_jp_t *p) {
    if (p->i + 4 > p->len) return false;
    for (int k = 0; k < 4; k++) {
        char c = p->s[p->i + k];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    }
    p->i += 4;
    return true;
}
static bool pb2_jp_string(pb2_jp_t *p) {
    if (p->i >= p->len || p->s[p->i] != '"') return false;
    p->i++;
    while (p->i < p->len) {
        unsigned char c = (unsigned char)p->s[p->i];
        if (c == '"') { p->i++; return true; }
        if (c < 0x20) return false;          /* raw control char: invalid */
        if (c == '\\') {
            p->i++;
            if (p->i >= p->len) return false;
            char e = p->s[p->i];
            if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f'
                || e == 'n' || e == 'r' || e == 't') { p->i++; continue; }
            if (e == 'u') { p->i++; if (!pb2_jp_hex4(p)) return false; continue; }
            return false;                    /* invalid escape */
        }
        p->i++;
    }
    return false;                            /* unterminated */
}
static bool pb2_jp_number(pb2_jp_t *p) {
    size_t st = p->i;
    if (p->i < p->len && p->s[p->i] == '-') p->i++;
    if (p->i >= p->len) return false;
    if (p->s[p->i] == '0') {
        p->i++;
        if (p->i < p->len && p->s[p->i] >= '0' && p->s[p->i] <= '9') return false;
    } else if (p->s[p->i] >= '1' && p->s[p->i] <= '9') {
        while (p->i < p->len && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
    } else return false;
    if (p->i < p->len && p->s[p->i] == '.') {
        p->i++;
        if (p->i >= p->len || p->s[p->i] < '0' || p->s[p->i] > '9') return false;
        while (p->i < p->len && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
    }
    if (p->i < p->len && (p->s[p->i] == 'e' || p->s[p->i] == 'E')) {
        p->i++;
        if (p->i < p->len && (p->s[p->i] == '+' || p->s[p->i] == '-')) p->i++;
        if (p->i >= p->len || p->s[p->i] < '0' || p->s[p->i] > '9') return false;
        while (p->i < p->len && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
    }
    return p->i > st;
}
static bool pb2_jp_lit(pb2_jp_t *p, const char *lit) {
    size_t n = strlen(lit);
    if (p->i + n > p->len) return false;
    if (strncmp(p->s + p->i, lit, n) != 0) return false;
    p->i += n;
    return true;
}
static bool pb2_jp_value(pb2_jp_t *p) {
    if (p->bad || p->depth > 200) { p->bad = true; return false; }
    pb2_jp_ws(p);
    if (p->i >= p->len) return false;
    char c = p->s[p->i];
    if (c == '"') return pb2_jp_string(p);
    if (c == '{') {
        p->i++; p->depth++;
        pb2_jp_ws(p);
        if (p->i < p->len && p->s[p->i] == '}') { p->i++; p->depth--; return true; }
        for (;;) {
            pb2_jp_ws(p);
            if (!pb2_jp_string(p)) return false;
            pb2_jp_ws(p);
            if (p->i >= p->len || p->s[p->i] != ':') return false;
            p->i++;
            if (!pb2_jp_value(p)) return false;
            pb2_jp_ws(p);
            if (p->i < p->len && p->s[p->i] == ',') { p->i++; continue; }
            if (p->i < p->len && p->s[p->i] == '}') { p->i++; p->depth--; return true; }
            return false;
        }
    }
    if (c == '[') {
        p->i++; p->depth++;
        pb2_jp_ws(p);
        if (p->i < p->len && p->s[p->i] == ']') { p->i++; p->depth--; return true; }
        for (;;) {
            if (!pb2_jp_value(p)) return false;
            pb2_jp_ws(p);
            if (p->i < p->len && p->s[p->i] == ',') { p->i++; continue; }
            if (p->i < p->len && p->s[p->i] == ']') { p->i++; p->depth--; return true; }
            return false;
        }
    }
    if (c == 't') return pb2_jp_lit(p, "true");
    if (c == 'f') return pb2_jp_lit(p, "false");
    if (c == 'n') return pb2_jp_lit(p, "null");
    if (c == 'N') return pb2_jp_lit(p, "NaN");
    if (c == 'I') return pb2_jp_lit(p, "Infinity");
    if (c == '-' && p->i + 1 < p->len && p->s[p->i + 1] == 'I') {
        p->i++; return pb2_jp_lit(p, "Infinity");
    }
    if (c == '-' || (c >= '0' && c <= '9')) return pb2_jp_number(p);
    return false;
}
/* Strict parse: NULL when the text is not valid Python json.loads grammar. */
static json_t *pb2_json_strict_parse(const char *text) {
    if (!text) return NULL;
    pb2_jp_t p = { text, 0, strlen(text), 0, false };
    if (!pb2_jp_value(&p)) return NULL;
    pb2_jp_ws(&p);
    if (p.i != p.len) return NULL;
    char *err = NULL;
    json_t *v = json_parse(text, &err);
    if (err) { free(err); json_free(v); return NULL; }
    return v;
}

/* ── seam call helpers ── */
static json_t *pb2_call_raw(const pb2_seams_t *s, const char *name, json_t *args) {
    json_t *a = args ? args : json_object();
    json_t *r = s->call(s->ctx, name, a);
    json_free(a);
    return r;
}
static json_t *pb2_call(const pb2_seams_t *s, const char *name, json_t *args, json_t **exc) {
    json_t *r = pb2_call_raw(s, name, args);
    if (pb2_raise(r)) { *exc = r; return NULL; }
    return r;
}
static void pb2_arg_str(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void pb2_arg_node(json_t *a, const char *k, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
/* logger.<level>(msg, *args) — args recorded verbatim; takes ownership of
 * args_array (NULL == empty). Returns false when the seam raised. */
static bool pb2_log(const pb2_seams_t *s, json_t **exc, const char *level,
                    const char *msg, json_t *args_array) {
    char name[32];
    snprintf(name, sizeof(name), "log_%s", level);
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_array ? args_array : json_array());
    json_t *r = pb2_call(s, name, a, exc);
    json_free(r);
    return *exc == NULL;
}
/* str seam result reader: returns a strdup'd string or NULL (None/non-str). */
static char *pb2_str_result(const json_t *r) {
    if (r && r->type == JSON_STRING) return pb2_xstrdup(r->str_val);
    return NULL;
}
/* Import-gate spec (no seam event on a failed import). The None-in-sys.modules
 * halt raises ModuleNotFoundError (an ImportError subclass) on py3.14. */
static json_t *pb2_import_exc(const char *modname) {
    char msg[256];
    snprintf(msg, sizeof(msg), "import of %s halted; None in sys.modules", modname);
    return pb2_raise_spec("ModuleNotFoundError", msg);
}

/* bytes[:80].decode("utf-8", errors="replace") — ASCII fixture domain; a
 * truncated/invalid trailing sequence becomes U+FFFD. */
static char *pb2_snippet_decode(const char *data, size_t nbytes) {
    if (!data) return pb2_xstrdup("");
    size_t len = strlen(data);
    if (nbytes < len) len = nbytes;
    pb2_sb_t sb; pb2_sb_init(&sb);
    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)data[i];
        if (c < 0x80) { pb2_sb_addch(&sb, (char)c); i++; continue; }
        unsigned n; unsigned v;
        if ((c & 0xE0) == 0xC0) { n = 2; v = c & 0x1F; }
        else if ((c & 0xF0) == 0xE0) { n = 3; v = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { n = 4; v = c & 0x07; }
        else { pb2_sb_add(&sb, "\xEF\xBF\xBD"); i++; continue; }
        if (i + n > len) { pb2_sb_add(&sb, "\xEF\xBF\xBD"); break; }
        bool ok = true;
        for (unsigned k = 1; k < n; k++) {
            unsigned char cc = (unsigned char)data[i + k];
            if ((cc & 0xC0) != 0x80) { ok = false; break; }
            v = (v << 6) | (cc & 0x3F);
        }
        if (!ok) { pb2_sb_add(&sb, "\xEF\xBF\xBD"); i++; continue; }
        /* re-encode the codepoint */
        char enc[5]; int el = 0;
        if (v < 0x80) enc[el++] = (char)v;
        else if (v < 0x800) { enc[el++] = (char)(0xC0 | (v >> 6)); enc[el++] = (char)(0x80 | (v & 0x3F)); }
        else if (v < 0x10000) { enc[el++] = (char)(0xE0 | (v >> 12)); enc[el++] = (char)(0x80 | ((v >> 6) & 0x3F)); enc[el++] = (char)(0x80 | (v & 0x3F)); }
        else { enc[el++] = (char)(0xF0 | (v >> 18)); enc[el++] = (char)(0x80 | ((v >> 12) & 0x3F)); enc[el++] = (char)(0x80 | ((v >> 6) & 0x3F)); enc[el++] = (char)(0x80 | (v & 0x3F)); }
        pb2_sb_addn(&sb, enc, (size_t)el);
        i += n;
    }
    return pb2_sb_take(&sb);
}

/* first n bytes of an ASCII hex string (uuid hex[:12]). */
static char *pb2_first_n(const char *s, size_t n) {
    if (!s) return pb2_xstrdup("");
    size_t l = strlen(s);
    if (n < l) l = n;
    char *d = malloc(l + 1);
    if (d) { memcpy(d, s, l); d[l] = '\0'; }
    return d;
}

/* ── media-cache dir hardening ── */
/* PoP: pb2__secure_media_cache_dir @ gateway/platforms/base.py:_secure_media_cache_dir */
static json_t *pb2__secure_media_cache_dir(const char *cache_dir, bool cfg_fail,
                                           const pb2_seams_t *s, json_t **exc) {
    const char *cd = cache_dir ? cache_dir : "";
    json_t *caught = NULL;
    bool managed = false;
    /* inner try: from hermes_cli.config import is_managed; managed = bool(is_managed()) */
    {
        json_t *ie = NULL;
        if (cfg_fail) ie = pb2_import_exc("hermes_cli.config");
        else {
            json_t *r = pb2_call_raw(s, "is_managed", json_object());
            if (pb2_raise(r)) ie = r;
            else { managed = pb2_truthy(r); json_free(r); }
        }
        if (ie) {
            if (pb2_exc_matches(pb2_raise(ie), "Exception")) json_free(ie);
            else { *exc = ie; return NULL; }
        }
    }
    if (managed) {
        json_t *a = json_object();
        pb2_arg_str(a, "path", cd);
        json_set(a, "parents", json_bool(true));
        json_set(a, "exist_ok", json_bool(true));
        json_t *r = pb2_call_raw(s, "mkdir", a);
        if (pb2_raise(r)) {
            if (pb2_exc_matches(pb2_raise(r), "Exception")) caught = r;
            else { *exc = r; return NULL; }
        } else { json_free(r); return NULL; }
    } else {
        json_t *a = json_object();
        pb2_arg_str(a, "path", cd);
        json_set(a, "parents", json_bool(true));
        json_set(a, "exist_ok", json_bool(true));
        json_set(a, "mode", json_number(448));   /* 0o700 */
        json_t *r = pb2_call_raw(s, "make_user_dir", a);
        if (pb2_raise(r)) {
            if (pb2_exc_matches(pb2_raise(r), "Exception")) caught = r;
            else { *exc = r; return NULL; }
        } else json_free(r);
        if (!caught) {
            /* inner try: from hermes_cli.config import _secure_dir; _secure_dir(cache_dir) */
            json_t *ie = NULL;
            if (cfg_fail) ie = pb2_import_exc("hermes_cli.config");
            else {
                json_t *sa = json_object();
                pb2_arg_str(sa, "path", cd);
                json_t *sr = pb2_call_raw(s, "secure_dir", sa);
                if (pb2_raise(sr)) ie = sr; else json_free(sr);
            }
            if (ie) {
                if (pb2_exc_matches(pb2_raise(ie), "Exception")) {
                    json_t *la = json_array();
                    json_append(la, pb2_exc_norm(ie));
                    json_free(ie);
                    json_t *le = NULL;
                    pb2_log(s, &le, "debug", "media cache dir chmod skipped: %s", la);
                    if (le) {
                        if (pb2_exc_matches(pb2_raise(le), "Exception")) caught = le;
                        else { *exc = le; return NULL; }
                    }
                } else { *exc = ie; return NULL; }
            }
        }
    }
    if (caught) {
        /* outer except Exception: log + best-effort mkdir fallback */
        json_t *la = json_array();
        json_append(la, json_string(cd));
        json_append(la, pb2_exc_norm(caught));
        json_free(caught);
        json_t *le = NULL;
        pb2_log(s, &le, "debug", "media cache dir creation failed for %s: %s", la);
        if (le) { *exc = le; return NULL; }
        json_t *a = json_object();
        pb2_arg_str(a, "path", cd);
        json_set(a, "parents", json_bool(true));
        json_set(a, "exist_ok", json_bool(true));
        json_t *r = pb2_call(s, "mkdir", a, exc);
        json_free(r);
        return NULL;
    }
    return NULL;
}

/* ── owner-only cache file write ── */
/* PoP: pb2__write_cache_file @ gateway/platforms/base.py:_write_cache_file */
static json_t *pb2__write_cache_file(const char *cache_dir, const char *prefix,
                                     const char *ext, const char *data,
                                     const pb2_seams_t *s, json_t **exc) {
    json_t *ur = pb2_call(s, "uuid4_hex", json_object(), exc);
    if (*exc) return NULL;
    char *hex = pb2_str_result(ur);
    json_free(ur);
    if (!hex) {
        *exc = pb2_raise_spec("TypeError", "'NoneType' object is not subscriptable");
        return NULL;
    }
    char *hex12 = pb2_first_n(hex, 12);
    free(hex);
    pb2_sb_t nb; pb2_sb_init(&nb);
    pb2_sb_add(&nb, prefix ? prefix : "");
    pb2_sb_addch(&nb, '_');
    pb2_sb_add(&nb, hex12);
    pb2_sb_add(&nb, ext ? ext : "");
    char *name = pb2_sb_take(&nb);
    free(hex12);
    char *filepath = pb2_posix_join(cache_dir, name);
    free(name);
    /* try: os.open(str(filepath), O_WRONLY|O_CREAT|O_TRUNC, 0o600) */
    json_t *oa = json_object();
    pb2_arg_str(oa, "path", filepath);
    json_set(oa, "flags", json_number(769));   /* probed on the Windows gate host */
    json_set(oa, "mode", json_number(384));    /* 0o600 */
    json_t *or_ = pb2_call_raw(s, "os_open", oa);
    if (pb2_raise(or_)) {
        if (pb2_exc_matches(pb2_raise(or_), "OSError")) {
            json_free(or_);
            json_t *wa = json_object();
            pb2_arg_str(wa, "path", filepath);
            pb2_arg_str(wa, "data", data);
            json_t *wr = pb2_call(s, "write_bytes", wa, exc);
            json_free(wr);
            if (*exc) { free(filepath); return NULL; }
            json_t *ret = json_string(filepath);
            free(filepath);
            return ret;
        }
        *exc = or_;
        free(filepath);
        return NULL;
    }
    double fd = (or_ && or_->type == JSON_NUMBER) ? or_->num_val : 0;
    json_free(or_);
    json_t *fa = json_object();
    json_set(fa, "fd", json_number(fd));
    pb2_arg_str(fa, "data", data);
    json_t *fr = pb2_call(s, "fdopen_write", fa, exc);
    json_free(fr);
    if (*exc) { free(filepath); return NULL; }
    json_t *ret = json_string(filepath);
    free(filepath);
    return ret;
}

/* PoP: pb2_cache_image_from_bytes @ gateway/platforms/base.py:cache_image_from_bytes */
static json_t *pb2_cache_image_from_bytes(const char *data, const char *ext,
                                          const pb2_seams_t *s, json_t **exc) {
    json_t *va = json_object();
    json_set(va, "size", json_number((double)strlen(data ? data : "")));
    pb2_arg_str(va, "media_type", "image");
    json_t *vr = pb2_call(s, "validate_inbound_media_size", va, exc);
    json_free(vr);
    if (*exc) return NULL;
    json_t *la = json_object();
    pb2_arg_str(la, "data", data);
    json_t *lr = pb2_call(s, "looks_like_image", la, exc);
    if (*exc) return NULL;
    bool is_img = pb2_truthy(lr);
    json_free(lr);
    if (!is_img) {
        char *snippet = pb2_snippet_decode(data, 80);
        char *repr = pb2_py_repr_str(snippet);
        free(snippet);
        pb2_sb_t mb; pb2_sb_init(&mb);
        pb2_sb_add(&mb, "Refusing to cache non-image data as ");
        pb2_sb_add(&mb, ext ? ext : "");
        pb2_sb_add(&mb, " (starts with: ");
        pb2_sb_add(&mb, repr);
        pb2_sb_addch(&mb, ')');
        char *msg = pb2_sb_take(&mb);
        free(repr);
        *exc = pb2_raise_spec("ValueError", msg);
        free(msg);
        return NULL;
    }
    json_t *gr = pb2_call(s, "get_image_cache_dir", json_object(), exc);
    if (*exc) return NULL;
    char *cd = pb2_str_result(gr);
    json_free(gr);
    char *cdn = pb2_posix_norm(cd ? cd : "");
    free(cd);
    json_t *ret = pb2__write_cache_file(cdn, "img", ext, data, s, exc);
    free(cdn);
    return ret;
}

/* PoP: pb2_cache_image_from_bytes_async @ gateway/platforms/base.py:cache_image_from_bytes_async */
static json_t *pb2_cache_image_from_bytes_async(const char *data, const char *ext,
                                                const pb2_seams_t *s, json_t **exc) {
    json_t *a = json_object();
    pb2_arg_str(a, "fn", "cache_image_from_bytes");
    json_t *args = json_array();
    json_append(args, json_string(data ? data : ""));
    json_append(args, json_string(ext ? ext : ""));
    json_set(a, "args", args);
    return pb2_call(s, "to_thread", a, exc);
}

/* PoP: pb2_cache_audio_from_bytes @ gateway/platforms/base.py:cache_audio_from_bytes */
static json_t *pb2_cache_audio_from_bytes(const char *data, const char *ext, bool audio_fail,
                                          const pb2_seams_t *s, json_t **exc) {
    if (audio_fail) { *exc = pb2_import_exc("tools.audio_container"); return NULL; }
    json_t *va = json_object();
    json_set(va, "size", json_number((double)strlen(data ? data : "")));
    pb2_arg_str(va, "media_type", "audio");
    json_t *vr = pb2_call(s, "validate_inbound_media_size", va, exc);
    json_free(vr);
    if (*exc) return NULL;
    /* _write_cache_file(get_audio_cache_dir(), "audio", sniff_audio_ext(data, ext), data):
     * the dir getter evaluates BEFORE the sniff argument. */
    json_t *gr = pb2_call(s, "get_audio_cache_dir", json_object(), exc);
    if (*exc) return NULL;
    char *cd = pb2_str_result(gr);
    json_free(gr);
    json_t *sa = json_object();
    pb2_arg_str(sa, "data", data);
    pb2_arg_str(sa, "ext", ext);
    json_t *sr = pb2_call(s, "sniff_audio_ext", sa, exc);
    if (*exc) { free(cd); return NULL; }
    char *sniffed = pb2_str_result(sr);
    json_free(sr);
    char *cdn = pb2_posix_norm(cd ? cd : "");
    free(cd);
    json_t *ret = pb2__write_cache_file(cdn, "audio", sniffed ? sniffed : "", data, s, exc);
    free(cdn);
    free(sniffed);
    return ret;
}

/* PoP: pb2_cache_audio_from_bytes_async @ gateway/platforms/base.py:cache_audio_from_bytes_async */
static json_t *pb2_cache_audio_from_bytes_async(const char *data, const char *ext,
                                                const pb2_seams_t *s, json_t **exc) {
    json_t *a = json_object();
    pb2_arg_str(a, "fn", "cache_audio_from_bytes");
    json_t *args = json_array();
    json_append(args, json_string(data ? data : ""));
    json_append(args, json_string(ext ? ext : ""));
    json_set(a, "args", args);
    return pb2_call(s, "to_thread", a, exc);
}

/* PoP: pb2_cache_video_from_bytes @ gateway/platforms/base.py:cache_video_from_bytes */
static json_t *pb2_cache_video_from_bytes(const char *data, const char *ext,
                                          const pb2_seams_t *s, json_t **exc) {
    json_t *va = json_object();
    json_set(va, "size", json_number((double)strlen(data ? data : "")));
    pb2_arg_str(va, "media_type", "video");
    json_t *vr = pb2_call(s, "validate_inbound_media_size", va, exc);
    json_free(vr);
    if (*exc) return NULL;
    json_t *gr = pb2_call(s, "get_video_cache_dir", json_object(), exc);
    if (*exc) return NULL;
    char *cd = pb2_str_result(gr);
    json_free(gr);
    char *cdn = pb2_posix_norm(cd ? cd : "");
    free(cd);
    json_t *ret = pb2__write_cache_file(cdn, "video", ext, data, s, exc);
    free(cdn);
    return ret;
}

/* PoP: pb2_cache_video_from_bytes_async @ gateway/platforms/base.py:cache_video_from_bytes_async */
static json_t *pb2_cache_video_from_bytes_async(const char *data, const char *ext,
                                                const pb2_seams_t *s, json_t **exc) {
    json_t *a = json_object();
    pb2_arg_str(a, "fn", "cache_video_from_bytes");
    json_t *args = json_array();
    json_append(args, json_string(data ? data : ""));
    json_append(args, json_string(ext ? ext : ""));
    json_set(a, "args", args);
    return pb2_call(s, "to_thread", a, exc);
}

/* PoP: pb2_cache_document_from_bytes @ gateway/platforms/base.py:cache_document_from_bytes */
static json_t *pb2_cache_document_from_bytes(const char *data, const char *filename,
                                             const pb2_seams_t *s, json_t **exc) {
    json_t *gr = pb2_call(s, "get_document_cache_dir", json_object(), exc);
    if (*exc) return NULL;
    char *cd = pb2_str_result(gr);
    json_free(gr);
    char *cdn = pb2_posix_norm(cd ? cd : "");
    free(cd);
    /* safe_name = (Path(filename).name if filename else "document").replace("\0","").strip() */
    char *base;
    if (filename && filename[0]) base = pb2_posix_name(filename);
    else base = pb2_xstrdup("document");
    pb2_sb_t sb; pb2_sb_init(&sb);
    for (const char *p = base; *p; p++) if (*p != '\0') pb2_sb_addch(&sb, *p);
    free(base);
    char *nonul = pb2_sb_take(&sb);
    char *safe = pb2_strip(nonul);
    free(nonul);
    if (!safe[0] || strcmp(safe, ".") == 0 || strcmp(safe, "..") == 0) {
        free(safe); safe = pb2_xstrdup("document");
    }
    json_t *ur = pb2_call(s, "uuid4_hex", json_object(), exc);
    if (*exc) { free(cdn); free(safe); return NULL; }
    char *hex = pb2_str_result(ur);
    json_free(ur);
    if (!hex) {
        *exc = pb2_raise_spec("TypeError", "'NoneType' object is not subscriptable");
        free(cdn); free(safe); return NULL;
    }
    char *hex12 = pb2_first_n(hex, 12);
    free(hex);
    pb2_sb_t nb; pb2_sb_init(&nb);
    pb2_sb_add(&nb, "doc_");
    pb2_sb_add(&nb, hex12);
    pb2_sb_addch(&nb, '_');
    pb2_sb_add(&nb, safe);
    char *name = pb2_sb_take(&nb);
    free(hex12); free(safe);
    char *filepath = pb2_posix_join(cdn, name);
    free(name);
    /* filepath.resolve().is_relative_to(cache_dir.resolve()) — resolve order:
     * filepath first, then cache_dir (the argument). */
    json_t *ra = json_object();
    pb2_arg_str(ra, "path", filepath);
    json_set(ra, "strict", json_bool(false));
    json_t *r1 = pb2_call(s, "resolve", ra, exc);
    if (*exc) { free(cdn); free(filepath); return NULL; }
    json_t *rb = json_object();
    pb2_arg_str(rb, "path", cdn);
    json_set(rb, "strict", json_bool(false));
    json_t *r2 = pb2_call(s, "resolve", rb, exc);
    if (*exc) { json_free(r1); free(cdn); free(filepath); return NULL; }
    char *p1 = pb2_str_result(r1);
    char *p2 = pb2_str_result(r2);
    json_free(r1); json_free(r2);
    char *n1 = pb2_posix_norm(p1 ? p1 : "");
    char *n2 = pb2_posix_norm(p2 ? p2 : "");
    free(p1); free(p2);
    json_t *ia = json_object();
    pb2_arg_str(ia, "path", n1);
    pb2_arg_str(ia, "other", n2);
    free(n1); free(n2);
    json_t *ir = pb2_call(s, "is_relative_to", ia, exc);
    if (*exc) { free(cdn); free(filepath); return NULL; }
    bool within = pb2_truthy(ir);
    json_free(ir);
    if (!within) {
        char *repr = pb2_py_repr_str(filename ? filename : "");
        pb2_sb_t mb; pb2_sb_init(&mb);
        pb2_sb_add(&mb, "Path traversal rejected: ");
        pb2_sb_add(&mb, repr);
        char *msg = pb2_sb_take(&mb);
        free(repr);
        *exc = pb2_raise_spec("ValueError", msg);
        free(msg);
        free(cdn); free(filepath);
        return NULL;
    }
    json_t *wa = json_object();
    pb2_arg_str(wa, "path", filepath);
    pb2_arg_str(wa, "data", data);
    json_t *wr = pb2_call(s, "write_bytes", wa, exc);
    json_free(wr);
    free(cdn);
    if (*exc) { free(filepath); return NULL; }
    json_t *ret = json_string(filepath);
    free(filepath);
    return ret;
}

/* PoP: pb2_cache_document_from_bytes_async @ gateway/platforms/base.py:cache_document_from_bytes_async */
static json_t *pb2_cache_document_from_bytes_async(const char *data, const char *filename,
                                                   const pb2_seams_t *s, json_t **exc) {
    json_t *a = json_object();
    pb2_arg_str(a, "fn", "cache_document_from_bytes");
    json_t *args = json_array();
    json_append(args, json_string(data ? data : ""));
    json_append(args, json_string(filename ? filename : ""));
    json_set(a, "args", args);
    return pb2_call(s, "to_thread", a, exc);
}

/* ── MIME -> extension reverse lookup (probed live construction: FIRST match
 * across document, video, image-document tables, reversed items per table;
 * later writes win, so the earliest extension of the last table survives). ── */
typedef struct { const char *mime; const char *ext; } pb2_mime_ext_t;
static const pb2_mime_ext_t PB2_MIME_TO_EXT[] = {
    {"text/plain", ".txt"},
    {"application/vnd.openxmlformats-officedocument.presentationml.presentation", ".pptx"},
    {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", ".xlsx"},
    {"application/vnd.openxmlformats-officedocument.wordprocessingml.document", ".docx"},
    {"application/vnd.ms-powerpoint", ".ppt"},
    {"application/vnd.ms-excel", ".xls"},
    {"application/msword", ".doc"},
    {"application/zip", ".zip"},
    {"application/toml", ".toml"},
    {"application/yaml", ".yaml"},
    {"application/xml", ".xml"},
    {"application/json", ".json"},
    {"text/csv", ".csv"},
    {"text/markdown", ".md"},
    {"application/pdf", ".pdf"},
    {"video/x-msvideo", ".avi"},
    {"video/x-matroska", ".mkv"},
    {"video/webm", ".webm"},
    {"video/quicktime", ".mov"},
    {"video/mp4", ".mp4"},
    {"image/gif", ".gif"},
    {"image/webp", ".webp"},
    {"image/png", ".png"},
    {"image/jpeg", ".jpg"},
};
static const char *pb2_mime_to_ext(const char *mime) {
    for (size_t i = 0; i < sizeof(PB2_MIME_TO_EXT) / sizeof(PB2_MIME_TO_EXT[0]); i++)
        if (strcmp(PB2_MIME_TO_EXT[i].mime, mime) == 0) return PB2_MIME_TO_EXT[i].ext;
    return NULL;
}

/* PoP: pb2__resolve_media_ext @ gateway/platforms/base.py:_resolve_media_ext */
static json_t *pb2__resolve_media_ext(const char *filename, const char *mime_type) {
    char *ext = pb2_xstrdup("");
    if (filename && filename[0]) {
        char *root = NULL, *e = NULL;
        pb2_splitext(filename, &root, &e);
        free(root);
        free(ext);
        ext = pb2_lower(e);
        free(e);
    }
    if (ext[0]) return json_string(ext);
    free(ext);
    char *ml = pb2_lower(mime_type ? mime_type : "");
    const char *hit = pb2_mime_to_ext(ml);
    free(ml);
    return json_string(hit ? hit : "");
}

/* ── ext tables for cache_media_bytes (live dict membership) ── */
static bool pb2_in_image_table(const char *e) {
    static const char *t[] = {".jpg", ".jpeg", ".png", ".webp", ".gif"};
    for (size_t i = 0; i < 5; i++) if (strcmp(t[i], e) == 0) return true;
    return false;
}
static bool pb2_in_video_table(const char *e) {
    static const char *t[] = {".mp4", ".mov", ".webm", ".mkv", ".avi"};
    for (size_t i = 0; i < 5; i++) if (strcmp(t[i], e) == 0) return true;
    return false;
}
static bool pb2_in_audio_table(const char *e) {
    static const char *t[] = {".ogg", ".opus", ".mp3", ".m2a", ".wav", ".m4a", ".flac"};
    for (size_t i = 0; i < 7; i++) if (strcmp(t[i], e) == 0) return true;
    return false;
}
static const char *pb2_image_mime(const char *e) {
    if (strcmp(e, ".jpg") == 0 || strcmp(e, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(e, ".png") == 0) return "image/png";
    if (strcmp(e, ".webp") == 0) return "image/webp";
    if (strcmp(e, ".gif") == 0) return "image/gif";
    return "";
}
static const char *pb2_video_mime(const char *e) {
    if (strcmp(e, ".mp4") == 0) return "video/mp4";
    if (strcmp(e, ".mov") == 0) return "video/quicktime";
    if (strcmp(e, ".webm") == 0) return "video/webm";
    if (strcmp(e, ".mkv") == 0) return "video/x-matroska";
    if (strcmp(e, ".avi") == 0) return "video/x-msvideo";
    return "";
}
static const char *pb2_audio_mime(const char *e) {
    if (strcmp(e, ".ogg") == 0) return "audio/ogg";
    if (strcmp(e, ".opus") == 0) return "audio/opus";
    if (strcmp(e, ".mp3") == 0 || strcmp(e, ".m2a") == 0) return "audio/mpeg";
    if (strcmp(e, ".wav") == 0) return "audio/wav";
    if (strcmp(e, ".m4a") == 0) return "audio/m4a";
    if (strcmp(e, ".flac") == 0) return "audio/flac";
    return "";
}
static const char *pb2_document_mime(const char *e) {
    /* pb2_mime_ext_t is {mime, ext} — initializers MUST be mime-first
     * (SUPPORTED_DOCUMENT_TYPES items are ext->mime; reversed here). */
    static const pb2_mime_ext_t t[] = {
        {"application/pdf", ".pdf"}, {"text/markdown", ".md"}, {"text/plain", ".txt"},
        {"text/csv", ".csv"}, {"text/plain", ".log"}, {"application/json", ".json"},
        {"application/xml", ".xml"}, {"application/yaml", ".yaml"}, {"application/yaml", ".yml"},
        {"application/toml", ".toml"}, {"text/plain", ".ini"}, {"text/plain", ".cfg"},
        {"application/zip", ".zip"}, {"application/msword", ".doc"}, {"application/vnd.ms-excel", ".xls"},
        {"application/vnd.ms-powerpoint", ".ppt"},
        {"application/vnd.openxmlformats-officedocument.wordprocessingml.document", ".docx"},
        {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", ".xlsx"},
        {"application/vnd.openxmlformats-officedocument.presentationml.presentation", ".pptx"},
        {"text/plain", ".ts"}, {"text/plain", ".py"}, {"text/plain", ".sh"},
    };
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++)
        if (strcmp(t[i].ext, e) == 0) return t[i].mime;
    return NULL;
}
/* re.sub(r"[^\w.\- ]", "_", filename) — ASCII fixture domain (\w == [A-Za-z0-9_]). */
static char *pb2_display_sub(const char *filename) {
    pb2_sb_t sb; pb2_sb_init(&sb);
    for (const char *p = filename; *p; p++) {
        unsigned char c = (unsigned char)*p;
        bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                 || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-' || c == ' ';
        pb2_sb_addch(&sb, keep ? (char)c : '_');
    }
    return pb2_sb_take(&sb);
}
static char *pb2_lstrip_dots(const char *s) {
    while (*s == '.') s++;
    return pb2_xstrdup(s);
}
static json_t *pb2_cached_media(const char *path, const char *mime, const char *kind, const char *display) {
    json_t *o = json_object();
    pb2_arg_str(o, "path", path);
    pb2_arg_str(o, "media_type", mime);
    pb2_arg_str(o, "kind", kind);
    pb2_arg_str(o, "display_name", display);
    return o;
}

/* PoP: pb2_cache_media_bytes @ gateway/platforms/base.py:cache_media_bytes */
static json_t *pb2_cache_media_bytes(const char *data, const char *filename,
                                     const char *mime_type, const char *default_kind,
                                     const pb2_seams_t *s, json_t **exc) {
    json_t *er = pb2__resolve_media_ext(filename, mime_type);
    char *ext = pb2_xstrdup(er->str_val);
    json_free(er);
    char *mime = pb2_lower(mime_type ? mime_type : "");
    char *display;
    if (filename && filename[0]) display = pb2_display_sub(filename);
    else {
        char *ls = pb2_lstrip_dots(ext);
        display = ls[0] ? ls : pb2_xstrdup("file");
        if (display != ls) free(ls);
    }
    /* (kind, table, default_ext, cache_fn, passthrough) loop */
    for (int k = 0; k < 3; k++) {
        const char *kind = k == 0 ? "image" : (k == 1 ? "video" : "audio");
        const char *default_ext = k == 0 ? ".jpg" : (k == 1 ? ".mp4" : ".ogg");
        bool passthrough = (k != 1);
        bool in_table = k == 0 ? pb2_in_image_table(ext)
                      : (k == 1 ? pb2_in_video_table(ext) : pb2_in_audio_table(ext));
        char prefix[16];
        snprintf(prefix, sizeof(prefix), "%s/", kind);
        bool mime_match = pb2_starts_with(mime, prefix);
        bool kind_match = default_kind && strcmp(default_kind, kind) == 0;
        if (!(mime_match || in_table || kind_match)) continue;
        const char *kind_ext = in_table ? ext : default_ext;
        json_t *path = NULL;
        if (k == 0) path = pb2_cache_image_from_bytes(data, kind_ext, s, exc);
        else if (k == 1) path = pb2_cache_video_from_bytes(data, kind_ext, s, exc);
        else path = pb2_cache_audio_from_bytes(data, kind_ext, false, s, exc);
        if (*exc) {
            /* except ValueError: image -> None; other kinds re-raise */
            if (k == 0 && strcmp(pb2_raise(*exc), "ValueError") == 0) {
                json_free(*exc); *exc = NULL;
                free(ext); free(mime); free(display);
                return NULL;
            }
            free(ext); free(mime); free(display);
            return NULL;
        }
        const char *out_mime;
        if (passthrough && mime_match) out_mime = mime;
        else out_mime = k == 0 ? pb2_image_mime(kind_ext)
                      : (k == 1 ? pb2_video_mime(kind_ext) : pb2_audio_mime(kind_ext));
        json_t *ret = pb2_cached_media(path ? path->str_val : "", out_mime, kind, display);
        json_free(path);
        free(ext); free(mime); free(display);
        return ret;
    }
    /* document fallback */
    char *fallback_name;
    if (filename && filename[0]) fallback_name = pb2_xstrdup(filename);
    else if (ext[0]) {
        pb2_sb_t fb; pb2_sb_init(&fb);
        pb2_sb_add(&fb, "document");
        pb2_sb_add(&fb, ext);
        fallback_name = pb2_sb_take(&fb);
    } else fallback_name = pb2_xstrdup("document.bin");
    json_t *path = pb2_cache_document_from_bytes(data, fallback_name, s, exc);
    if (*exc) { free(ext); free(mime); free(display); free(fallback_name); return NULL; }
    const char *doc_mime = pb2_document_mime(ext);
    const char *out_mime = doc_mime ? doc_mime : (mime[0] ? mime : "application/octet-stream");
    const char *disp = display[0] ? display : fallback_name;
    json_t *ret = pb2_cached_media(path ? path->str_val : "", out_mime, "document", disp);
    json_free(path);
    free(ext); free(mime); free(display); free(fallback_name);
    return ret;
}

/* PoP: pb2_cache_media_bytes_async @ gateway/platforms/base.py:cache_media_bytes_async */
static json_t *pb2_cache_media_bytes_async(const char *data, const char *filename,
                                           const char *mime_type, const char *default_kind,
                                           const pb2_seams_t *s, json_t **exc) {
    json_t *a = json_object();
    pb2_arg_str(a, "fn", "cache_media_bytes");
    json_t *args = json_array();
    json_append(args, json_string(data ? data : ""));
    json_set(a, "args", args);
    json_t *kw = json_object();
    pb2_arg_str(kw, "filename", filename ? filename : "");
    pb2_arg_str(kw, "mime_type", mime_type ? mime_type : "");
    if (default_kind) json_set(kw, "default_kind", json_string(default_kind));
    json_set(a, "kwargs", kw);
    return pb2_call(s, "to_thread", a, exc);
}

/* PoP: pb2__sqlite_files @ gateway/platforms/base.py:_sqlite_files */
static json_t *pb2__sqlite_files(const char *name) {
    json_t *a = json_array();
    const char *sufs[] = {"", "-wal", "-shm", "-journal"};
    for (int i = 0; i < 4; i++) {
        pb2_sb_t sb; pb2_sb_init(&sb);
        pb2_sb_add(&sb, name ? name : "");
        pb2_sb_add(&sb, sufs[i]);
        char *s = pb2_sb_take(&sb);
        json_append(a, json_string(s));
        free(s);
    }
    return a;
}

/* tenv seam result -> stripped string; a None result raises AttributeError
 * on .strip() (live). Returns NULL with *exc set on a raise. */
static char *pb2_tenv_strip(const pb2_seams_t *s, json_t **exc, const char *name, const char *def) {
    json_t *a = json_object();
    pb2_arg_str(a, "name", name);
    pb2_arg_str(a, "default", def);
    json_t *r = pb2_call(s, "tenv", a, exc);
    if (*exc) return NULL;
    if (!r || r->type != JSON_STRING) {
        json_free(r);
        *exc = pb2_raise_spec("AttributeError", "'NoneType' object has no attribute 'strip'");
        return NULL;
    }
    char *st = pb2_strip(r->str_val);
    json_free(r);
    return st;
}

/* PoP: pb2__docker_env_active @ gateway/platforms/base.py:_docker_env_active */
static json_t *pb2__docker_env_active(const pb2_seams_t *s, json_t **exc) {
    char *v = pb2_tenv_strip(s, exc, "TERMINAL_ENV", "");
    if (*exc) return NULL;
    char *lv = pb2_lower(v);
    free(v);
    bool active = strcmp(lv, "docker") == 0;
    free(lv);
    return json_bool(active);
}

/* PoP: pb2__docker_persistent_active @ gateway/platforms/base.py:_docker_persistent_active */
static json_t *pb2__docker_persistent_active(const pb2_seams_t *s, json_t **exc) {
    json_t *act = pb2__docker_env_active(s, exc);
    if (*exc) return NULL;
    bool active = pb2_truthy(act);
    json_free(act);
    if (!active) return json_bool(false);
    char *v = pb2_tenv_strip(s, exc, "TERMINAL_CONTAINER_PERSISTENT", "true");
    if (*exc) return NULL;
    char *lv = pb2_lower(v);
    free(v);
    bool truthy = strcmp(lv, "1") == 0 || strcmp(lv, "true") == 0
               || strcmp(lv, "yes") == 0 || strcmp(lv, "on") == 0;
    free(lv);
    return json_bool(truthy);
}

/* PoP: pb2__cache_dir_container_mounts @ gateway/platforms/base.py:_cache_dir_container_mounts */
static json_t *pb2__cache_dir_container_mounts(bool cred_fail, const pb2_seams_t *s, json_t **exc) {
    json_t *act = pb2__docker_env_active(s, exc);
    if (*exc) return NULL;
    bool active = pb2_truthy(act);
    json_free(act);
    if (!active) return json_array();
    /* try: from tools.credential_files import get_cache_directory_mounts; ... */
    if (cred_fail) return json_array();   /* ImportError caught by except Exception */
    json_t *r = pb2_call_raw(s, "get_cache_directory_mounts", json_object());
    if (pb2_raise(r)) {
        if (pb2_exc_matches(pb2_raise(r), "Exception")) { json_free(r); return json_array(); }
        *exc = r; return NULL;
    }
    json_t *out = json_array();
    if (r && r->type == JSON_ARRAY) {
        for (size_t i = 0; i < r->c.count; i++) {
            const json_t *m = r->c.items[i];
            /* m["host_path"], m["container_path"] — KeyError/TypeError -> [] */
            if (!m || m->type != JSON_OBJECT) { json_free(r); json_free(out); return json_array(); }
            const json_t *h = json_obj_get(m, "host_path");
            const json_t *c = json_obj_get(m, "container_path");
            if (!h || !c || h->type != JSON_STRING || c->type != JSON_STRING) {
                json_free(r); json_free(out); return json_array();
            }
            char *hn = pb2_posix_norm(h->str_val);
            char *cn = pb2_posix_norm(c->str_val);
            json_t *pair = json_array();
            json_append(pair, json_string(hn));
            json_append(pair, json_string(cn));
            free(hn); free(cn);
            json_append(out, pair);
        }
    }
    json_free(r);
    return out;
}

/* PoP: pb2__media_delivery_recency_seconds @ gateway/platforms/base.py:_media_delivery_recency_seconds */
static json_t *pb2__media_delivery_recency_seconds(bool policy_fail, const pb2_seams_t *s, json_t **exc) {
    if (policy_fail) { *exc = pb2_import_exc("gateway.media_policy"); return NULL; }
    json_t *tr = pb2_call(s, "media_delivery_trust_recent", json_object(), exc);
    if (*exc) return NULL;
    bool trust = pb2_truthy(tr);
    json_free(tr);
    if (!trust) return json_number(0.0);
    json_t *sr = pb2_call(s, "media_delivery_trust_recent_seconds", json_object(), exc);
    if (*exc) return NULL;
    if (!sr || sr->type != JSON_STRING) {
        json_free(sr);
        *exc = pb2_raise_spec("AttributeError", "'NoneType' object has no attribute 'strip'");
        return NULL;
    }
    char *custom = pb2_strip(sr->str_val);
    json_free(sr);
    double default_v = 600.0;   /* float(_MEDIA_DELIVERY_TRUST_RECENT_DEFAULT_SECONDS) */
    double result = default_v;
    /* _or_default(lambda: max(0.0, float(custom)) if custom else default, default)
     * — exc=(TypeError, ValueError); I/O-free, inlined (no seam event). */
    if (custom[0]) {
        double v;
        if (pb2_py_float(custom, &v)) result = (v > 0.0) ? v : 0.0;
        /* float() ValueError -> default (TypeError cannot arise from str) */
    }
    free(custom);
    return json_number(result);
}

/* PoP: pb2__resolve_path @ gateway/platforms/base.py:_resolve_path */
static json_t *pb2__resolve_path(const char *path, bool strict, bool expand,
                                 const pb2_seams_t *s, json_t **exc) {
    char *cur = pb2_posix_norm(path ? path : "");
    if (expand) {
        json_t *a = json_object();
        pb2_arg_str(a, "path", cur);
        json_t *r = pb2_call_raw(s, "expanduser", a);
        if (pb2_raise(r)) {
            if (pb2_exc_matches(pb2_raise(r), "OSError|RuntimeError|ValueError")) {
                json_free(r); free(cur); return NULL;
            }
            *exc = r; free(cur); return NULL;
        }
        char *ns = pb2_str_result(r);
        json_free(r);
        free(cur);
        cur = pb2_posix_norm(ns ? ns : "");
        free(ns);
    }
    json_t *a = json_object();
    pb2_arg_str(a, "path", cur);
    json_set(a, "strict", json_bool(strict));
    json_t *r = pb2_call_raw(s, "resolve", a);
    free(cur);
    if (pb2_raise(r)) {
        if (pb2_exc_matches(pb2_raise(r), "OSError|RuntimeError|ValueError")) {
            json_free(r); return NULL;
        }
        *exc = r; return NULL;
    }
    char *ns = pb2_str_result(r);
    json_free(r);
    if (!ns) return NULL;
    char *nn = pb2_posix_norm(ns);
    free(ns);
    json_t *ret = json_string(nn);
    free(nn);
    return ret;
}

/* find(":/", start) — index relative to s, or -1. */
static long pb2_find_colon_slash(const char *s, size_t start) {
    if (!s) return -1;
    size_t len = strlen(s);
    if (start >= len) return -1;
    const char *hit = strstr(s + start, ":/");
    return hit ? (long)(hit - s) : -1;
}

/* PoP: pb2__parse_docker_volume_mounts @ gateway/platforms/base.py:_parse_docker_volume_mounts */
static json_t *pb2__parse_docker_volume_mounts(const pb2_seams_t *s, json_t **exc) {
    char *raw = pb2_tenv_strip(s, exc, "TERMINAL_DOCKER_VOLUMES", "");
    if (*exc) return NULL;
    json_t *parsed = NULL;
    if (raw[0]) {
        parsed = pb2_json_strict_parse(raw);
        if (!parsed) { free(raw); return json_array(); }   /* except Exception: [] */
    }
    free(raw);
    json_t *mounts = json_array();
    if (parsed && parsed->type == JSON_ARRAY) {
        for (size_t i = 0; i < parsed->c.count; i++) {
            const json_t *entry = parsed->c.items[i];
            char *spec;
            if (entry && entry->type == JSON_STRING) spec = pb2_strip(entry->str_val);
            else spec = pb2_xstrdup("");
            /* re.match(r"[A-Za-z]:[/\\]", spec) */
            size_t sl = strlen(spec);
            bool drive = sl >= 3
                && ((spec[0] >= 'A' && spec[0] <= 'Z') || (spec[0] >= 'a' && spec[0] <= 'z'))
                && spec[1] == ':' && (spec[2] == '/' || spec[2] == '\\');
            long sep = pb2_find_colon_slash(spec, drive ? 2 : 0);
            if (sep <= 0) { free(spec); continue; }
            /* container_raw = spec[sep+1:].split(":", 1)[0] */
            const char *crest = spec + sep + 1;
            const char *colon = strchr(crest, ':');
            size_t clen = colon ? (size_t)(colon - crest) : strlen(crest);
            char *container_raw = malloc(clen + 1);
            if (container_raw) { memcpy(container_raw, crest, clen); container_raw[clen] = '\0'; }
            /* host_expanded = os.path.expanduser(spec[:sep]) — OUTSIDE the try */
            char *host_part = malloc((size_t)sep + 1);
            if (host_part) { memcpy(host_part, spec, (size_t)sep); host_part[sep] = '\0'; }
            free(spec);
            json_t *ea = json_object();
            pb2_arg_str(ea, "path", host_part);
            json_t *er = pb2_call(s, "expanduser", ea, exc);
            free(host_part);
            if (*exc) { free(container_raw); json_free(parsed); json_free(mounts); return NULL; }
            char *he = pb2_str_result(er);
            json_free(er);
            char *hen = pb2_posix_norm(he ? he : "");
            free(he);
            size_t hl = strlen(hen);
            bool host_ok = (hl > 0 && hen[0] == '/') || (hl > 1 && hen[1] == ':');
            if (!host_ok) { free(hen); free(container_raw); continue; }
            json_t *resolved = pb2__resolve_path(hen, false, false, s, exc);
            free(hen);
            if (*exc) { free(container_raw); json_free(parsed); json_free(mounts); return NULL; }
            if (resolved) {
                char *cn = pb2_posix_norm(container_raw);
                json_t *pair = json_array();
                json_append(pair, json_copy(resolved));
                json_append(pair, json_string(cn));
                free(cn);
                json_append(mounts, pair);
                json_free(resolved);
            }
            free(container_raw);
        }
    }
    json_free(parsed);
    return mounts;
}

/* PoP: pb2__profile_dirs @ gateway/platforms/base.py:_profile_dirs */
static json_t *pb2__profile_dirs(const char *hermes_root, const pb2_seams_t *s, json_t **exc) {
    char *path = pb2_posix_join(hermes_root, "profiles");
    json_t *a = json_object();
    pb2_arg_str(a, "path", path);
    free(path);
    json_t *r = pb2_call_raw(s, "iterdir", a);
    if (pb2_raise(r)) {
        if (pb2_exc_matches(pb2_raise(r), "OSError")) { json_free(r); return json_array(); }
        *exc = r; return NULL;
    }
    json_t *out = json_array();
    if (r && r->type == JSON_ARRAY) {
        for (size_t i = 0; i < r->c.count; i++) {
            const json_t *p = r->c.items[i];
            char *pd = pb2_posix_norm(p && p->type == JSON_STRING ? p->str_val : "");
            json_t *da = json_object();
            pb2_arg_str(da, "path", pd);
            json_t *dr = pb2_call_raw(s, "is_dir", da);
            if (pb2_raise(dr)) {
                if (pb2_exc_matches(pb2_raise(dr), "OSError")) {
                    json_free(dr); free(pd); json_free(r); json_free(out);
                    return json_array();   /* atomic: partial results discarded */
                }
                *exc = dr; free(pd); json_free(r); json_free(out); return NULL;
            }
            bool isd = pb2_truthy(dr);
            json_free(dr);
            if (isd) json_append(out, json_string(pd));
            free(pd);
        }
    }
    json_free(r);
    return out;
}

/* PoP: pb2__credential_home_roots @ gateway/platforms/base.py:_credential_home_roots */
static json_t *pb2__credential_home_roots(const char *hermes_root, const pb2_seams_t *s, json_t **exc) {
    json_t *hr = pb2_call(s, "get_hermes_home", json_object(), exc);
    if (*exc) return NULL;
    char *home = pb2_str_result(hr);
    json_free(hr);
    char *homen = pb2_posix_norm(home ? home : "");
    free(home);
    char *rootn = pb2_posix_norm(hermes_root ? hermes_root : "");
    json_t *prof = pb2__profile_dirs(hermes_root, s, exc);
    if (*exc) { free(homen); free(rootn); return NULL; }
    /* list(dict.fromkeys((home, root, *profiles))) — order-preserving dedupe */
    json_t *seq = json_array();
    json_append(seq, json_string(homen));
    json_append(seq, json_string(rootn));
    free(homen); free(rootn);
    if (prof && prof->type == JSON_ARRAY)
        for (size_t i = 0; i < prof->c.count; i++) json_append(seq, json_copy(prof->c.items[i]));
    json_free(prof);
    json_t *out = json_array();
    for (size_t i = 0; i < seq->c.count; i++) {
        const char *v = seq->c.items[i]->str_val;
        bool dup = false;
        for (size_t j = 0; j < out->c.count; j++)
            if (strcmp(out->c.items[j]->str_val, v) == 0) { dup = true; break; }
        if (!dup) json_append(out, json_string(v));
    }
    json_free(seq);
    return out;
}

/* PoP: pb2__existing_regular_file @ gateway/platforms/base.py:_existing_regular_file */
static json_t *pb2__existing_regular_file(const char *raw, const pb2_seams_t *s, json_t **exc) {
    json_t *a = json_object();
    pb2_arg_str(a, "path", raw);
    json_t *r = pb2_call_raw(s, "expanduser", a);
    if (pb2_raise(r)) {
        if (pb2_exc_matches(pb2_raise(r), "OSError|RuntimeError|ValueError")) {
            json_free(r); return json_bool(false);
        }
        *exc = r; return NULL;
    }
    char *ex = pb2_str_result(r);
    json_free(r);
    char *p = pb2_posix_norm(ex ? ex : "");
    free(ex);
    json_t *fa = json_object();
    pb2_arg_str(fa, "path", p);
    free(p);
    json_t *fr = pb2_call_raw(s, "is_file", fa);
    if (pb2_raise(fr)) {
        if (pb2_exc_matches(pb2_raise(fr), "OSError|RuntimeError|ValueError")) {
            json_free(fr); return json_bool(false);
        }
        *exc = fr; return NULL;
    }
    bool isf = pb2_truthy(fr);
    json_free(fr);
    return json_bool(isf);
}

/* PoP: pb2__mask_media_scan_text @ gateway/platforms/base.py:_mask_media_scan_text */
static json_t *pb2__mask_media_scan_text(const char *text, const pb2_seams_t *s, json_t **exc) {
    json_t *a1 = json_object();
    pb2_arg_str(a1, "text", text);
    json_t *r1 = pb2_call(s, "mask_protected_spans", a1, exc);
    if (*exc) return NULL;
    char *m1 = pb2_str_result(r1);
    json_free(r1);
    json_t *a2 = json_object();
    pb2_arg_str(a2, "text", m1 ? m1 : "");
    json_t *r2 = pb2_call(s, "mask_json_string_media", a2, exc);
    free(m1);
    if (*exc) return NULL;
    char *masked = pb2_str_result(r2);
    json_free(r2);
    json_t *a3 = json_object();
    pb2_arg_str(a3, "text", text);
    json_t *r3 = pb2_call(s, "terminal_sentinel_start", a3, exc);
    if (*exc) { free(masked); return NULL; }
    double start = (r3 && r3->type == JSON_NUMBER) ? r3->num_val : -1;
    json_free(r3);
    if (start >= 0) {
        char *rs = pb2_rstrip(text);
        size_t end = pb2_utf8_len(rs);
        free(rs);
        json_t *a4 = json_object();
        pb2_arg_str(a4, "text", masked ? masked : "");
        json_t *spans = json_array();
        json_t *sp = json_array();
        json_append(sp, json_number(start));
        json_append(sp, json_number((double)end));
        json_append(spans, sp);
        json_set(a4, "spans", spans);
        json_t *r4 = pb2_call(s, "blank_spans", a4, exc);
        free(masked);
        if (*exc) return NULL;
        masked = pb2_str_result(r4);
        json_free(r4);
    }
    json_t *ret = json_string(masked ? masked : "");
    free(masked);
    return ret;
}

/* PoP: pb2__session_task_is_stale @ gateway/platforms/base.py:_session_task_is_stale */
static json_t *pb2__session_task_is_stale(const json_t *self_obj, const char *session_key,
                                          const pb2_seams_t *s, json_t **exc) {
    if (!json_has(self_obj, "_session_tasks")) {
        *exc = pb2_raise_spec("AttributeError",
            "'types.SimpleNamespace' object has no attribute '_session_tasks'");
        return NULL;
    }
    const json_t *tasks = json_obj_get(self_obj, "_session_tasks");
    if (!tasks || tasks->type == JSON_NULL) {
        *exc = pb2_raise_spec("AttributeError", "'NoneType' object has no attribute 'get'");
        return NULL;
    }
    const json_t *task = json_obj_get(tasks, session_key ? session_key : "");
    const json_t *done = (task && task->type == JSON_OBJECT) ? json_obj_get(task, "done") : NULL;
    if (!pb2_truthy(done)) return json_bool(false);
    json_t *r = pb2_call(s, "task_done", json_object(), exc);
    if (*exc) return NULL;
    bool stale = pb2_truthy(r);
    json_free(r);
    return json_bool(stale);
}

/* PoP: pb2_truncate_message @ gateway/platforms/base.py:truncate_message */
static json_t *pb2_truncate_message(const char *content, double max_length, bool len_fn_present,
                                    bool chunk_fail, const pb2_seams_t *s, json_t **exc) {
    if (chunk_fail) { *exc = pb2_import_exc("gateway.platforms.base_chunking"); return NULL; }
    json_t *a = json_object();
    pb2_arg_str(a, "content", content);
    json_set(a, "max_length", json_number(max_length));
    json_set(a, "len_fn", json_bool(len_fn_present));
    return pb2_call(s, "chunk_markdown_message", a, exc);
}

/* PoP: pb2_supports_exec_approval_buttons @ gateway/platforms/base.py:supports_exec_approval_buttons */
static json_t *pb2_supports_exec_approval_buttons(bool override) {
    /* cls._send_exec_approval_prompt is not BasePlatformAdapter._send_exec_approval_prompt
     * — identity of the prompt hook; the fixture supplies the override token. */
    return json_bool(override);
}