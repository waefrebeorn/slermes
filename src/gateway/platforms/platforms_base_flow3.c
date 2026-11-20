/*
 * platforms_base_flow3.c — UNCLAIMED-remainder FLOW cores of
 * gateway/platforms/base.py (cache-dir accessor factory, media-delivery path
 * validation, MEDIA tag-span composition, auto-TTS delegation, the response
 * extraction orchestrator, and two small nested closures).
 *
 * Ports the DECISION sequences (call order, branch conditions, short-circuit
 * evaluation, early returns, exception -> fallback / suppress / re-raise paths,
 * mutation order) of the live module. All live I/O and every claimed sibling
 * enters through the universal seam below; the cores perform no I/O.
 *
 * Sequences are observable through a pb3_trace_t: the harness wires the seam to
 * a recording stub that appends one JSON event per call (arguments + scripted
 * result) in call order; tests/oracle_platforms_base_flow3.py drives the LIVE
 * AST-extracted Python with recording monkeypatch stubs fed the SAME scripts and
 * compares ordered trace + ret + propagated exception + echoed state.
 *
 * ── Models ──────────────────────────────────────────────────────────────────
 * Path:        POSIX string doubles (pb3_posix_norm: collapse repeated '/',
 *              drop '.' components, keep '..', strip trailing '/' except root;
 *              exactly two leading slashes preserved). The oracle binds
 *              ns["Path"] to a PurePosixPath subclass (RecPath) so str() keeps
 *              forward slashes on the Windows gate host. .name / .is_absolute
 *              and '/' join are PURE (no seam event); is_file / expanduser are
 *              SEAMS. Fixture paths are clean absolute POSIX.
 * regex:       MEDIA_TAG_CLEANUP_RE / MEDIA_EXTENSIONLESS_TAG_RE / the
 *              extract_local_files path_re are COMPLEX (CJK terminators,
 *              tempered-greedy tokens). Their .finditer is a SEAM returning a
 *              scripted match list [{start,end,path|raw},...]; the cores port
 *              the filtering / dedup / span decisions over those matches, not
 *              the regex engine. The oracle replaces the compiled patterns
 *              with doubles whose finditer yields MatchDoubles built from the
 *              SAME script. Match projection: {start,end,path}.
 * self:        adapter doubles carry .name (str). event.source is a dict
 *              double {chat_id,...}. _record_delivery / _opt run on explicit
 *              state / value inputs (nested closures ported as helpers).
 * sets:        _IMAGE_EXTS / history sets project to SORTED id lists on both
 *              sides (frozenset iteration is hash-randomized live).
 * _ExtractedResponse: dataclass result projects to a dict in field order
 *              {text_content, images, media_files, local_files,
 *               force_document_attachments, pre_extract}.
 *
 * ── Universal seam ──────────────────────────────────────────────────────────
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * name identifies the live call (table below); args is a BORROWED object; the
 * result is a NEW json_t the caller frees. NULL == Python None. A scripted
 * exception is {"__raise__": "<Kind>", "msg": ...}; pb3_raise() detects it. The
 * harness records {ev:name, args:<copy>, ret:<script>} per call; a scripted JSON
 * null returns a NULL pointer (NOT a json_null node). FAIL-CLOSED: a seam with
 * NO script entry returns a RuntimeError raise spec ("unscripted seam: <name>")
 * on BOTH sides; a scripted null is a valid None. pb3_call_raw takes OWNERSHIP
 * of args (frees them); pb3_call additionally parks a raise spec into *exc and
 * returns NULL.
 *
 * Script semantics (harness + oracle identical): io[name] is a single value
 * reused for every call, OR a bare array consumed one element per call (last
 * element repeats when exhausted). A constant LIST result is wrapped as
 * {"__script__": true, "ret": [...]}.
 *
 * None-valued argument keys are OMITTED on both sides (drop_none). Logger args
 * are recorded VERBATIM (format specifiers apply only at emission); exception
 * args normalize on BOTH sides to {"__exc__": Kind, "msg": str(e)}; Path logger
 * args project to str.
 *
 * Exception handlers are transcribed exactly; BaseException-only kinds
 * (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError /
 * BaseException) pass through every `except Exception`-shaped catch site. The
 * scripted-kind hierarchy below is the FULL builtin tree (py3.14).
 *
 * Import gates: in-function `from pkg.mod import name` failures are modeled by
 * per-case input flags (<gate>_import_fail). On a failed gate the import raises
 * ModuleNotFoundError("import of <pkg.mod> halted; None in sys.modules") with NO
 * seam event (the oracle sets sys.modules[full] = None); on success the stub
 * module's recorder fires as the named seam. Gates:
 *   hosted_policy_import_fail  tools.hosted_tool_policy (host_feature_enabled)
 *   media_policy_import_fail   gateway.media_policy (media_delivery_strict)
 *   media_fetch_import_fail    gateway.media_fetch (fetch_remote_media)
 *   auto_tts_import_fail       gateway.platforms.base_auto_tts (synthesize_auto_tts)
 *   docker_media_import_fail   gateway.platforms.docker_media_paths (hand_back_unreadable_media)
 *
 * Seam name -> live call (args keys; ret):
 *   get_hermes_dir            hermes_constants.get_hermes_dir {subpath,old_name}; str
 *   secure_media_cache_dir    claimed sibling _secure_media_cache_dir {path}; null
 *   unlink_files_older_than   utils.unlink_files_older_than {path,pattern,max_age_seconds}; int
 *   host_feature_enabled      tools.hosted_tool_policy {feature}; truthy
 *   normalize_media_tag_path  claimed sibling {path}; str|null
 *   expanduser                os.path.expanduser {path}; str (may raise)
 *   translate_docker          claimed sibling _translate_docker_container_media_path {path,session_key}; str|null
 *   resolve_path              claimed sibling _resolve_path {path,strict,expand}; str|null
 *   is_file                   Path.is_file {path}; bool
 *   is_project_env_basename   agent.file_safety {name}; bool
 *   allowed_roots             claimed sibling _media_delivery_allowed_roots {}; [str,...]
 *   path_is_within            claimed sibling _path_is_within {path,root}; bool
 *   media_delivery_strict     gateway.media_policy {}; truthy
 *   path_under_denied_prefix  claimed sibling {path}; bool
 *   recency_seconds           claimed sibling _media_delivery_recency_seconds {}; num
 *   file_is_recently_produced claimed sibling {path,window}; bool
 *   validate_media_delivery_path  ported sibling {path,session_key}; str|null
 *   fetch_remote_media        gateway.media_fetch {raw}; str|null
 *   existing_regular_file     claimed sibling _existing_regular_file {raw}; bool
 *   log_safe_path             claimed sibling _log_safe_path {raw}; str
 *   mask_media_scan_text      claimed sibling {text}; str
 *   real_media_tag_spans      ported sibling {masked}; [[s,e],...]
 *   terminal_sentinel_start   claimed sibling {text}; int
 *   cleanup_finditer          MEDIA_TAG_CLEANUP_RE.finditer {text}; [{start,end,path},...]
 *   extensionless_finditer    MEDIA_EXTENSIONLESS_TAG_RE.finditer {text}; [{start,end,path},...]
 *   extensionless_media_matches   ported sibling {masked}; [[match,safe,end],...]
 *   path_lacks_deliverable_extension  claimed sibling {path}; bool
 *   match_extensionless_path  claimed sibling _match_extensionless_path {masked,match}; [safe,end]|null
 *   synthesize_auto_tts       gateway.platforms.base_auto_tts {self,text,source,build_fn}; scripted
 *   media_delivery_scope_enter  self._media_delivery_scope CM __enter__ {source}; scripted
 *   media_delivery_scope_exit   self._media_delivery_scope CM __exit__ {exc}; bool (suppress)
 *   extract_media             self.extract_media {content}; [media,cleaned]
 *   filter_media_delivery_paths   self.filter_media_delivery_paths {media_files,session_key}; [[path,bool],...]
 *   extract_images            self.extract_images {response}; [images,text]
 *   strip_media_directives    module _strip_media_directives {text}; str
 *   extract_local_files       self.extract_local_files {text}; [locals,cleaned]
 *   filter_local_delivery_paths   self.filter_local_delivery_paths {files,session_key}; [str,...]
 *   hand_back_unreadable_media  gateway.platforms.docker_media_paths {paths}; null
 *   bounded_history           self._bounded_history_media_paths_for_session {session_key}; [str,...]|null
 *   footer_onto_photo_caption gateway.runtime_footer {self,extracted,image_exts}; scripted
 *   log_debug / log_info / log_warning  logger.<level> {msg,args:[...]}; null
 *
 * Claimed siblings stay SEAMED, never re-ported. Ported siblings (validate_media_
 * delivery_path, _real_media_tag_spans, _extensionless_media_matches,
 * _deliverable_tag_spans) are ALSO seamed when called by another ported flow so
 * each flow's trace stays focused on its own decisions.
 *
 * ── Fixture domains (documented exclusions) ─────────────────────────────────
 * - ASCII only: paths/text are clean absolute POSIX (no '..', no trailing '/',
 *   no drive letters, no backslashes) so string normalization == PurePosixPath
 *   parts equality. Regex matches are SCRIPTED (no CJK / emphasis exercised).
 * - float results are finite; integral floats project as JSON ints.
 * - _opt value domain: str | null | int | bool (str() projection).
 * - _record_delivery result domain: null | {success: bool} | {} (absent success).
 * - CM exit suppression is exercised; exit-raise propagates.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "libjson/json.h"

/* ── trace recorder (harness infrastructure, no Python counterpart) ── */
typedef struct pb3_trace {
    json_t *events;   /* owned JSON_ARRAY; NULL until first add */
} pb3_trace_t;

static void pb3_trace_add(pb3_trace_t *t, json_t *ev) {
    if (!t) { json_free(ev); return; }
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}

/* ── universal seam ── */
typedef struct pb3_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} pb3_seams_t;

/* ── raise spec ── */
static const char *pb3_raise(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return NULL;
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : NULL;
}
static const char *pb3_raise_msg(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return "";
    return json_get_str(r, "msg", "");
}
static json_t *pb3_raise_spec(const char *kind, const char *msg) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(kind));
    json_set(e, "msg", json_string(msg ? msg : ""));
    return e;
}
/* {"__exc__": kind, "msg": str(e)} — normalized exception VALUE (log args). */
static json_t *pb3_exc_norm(const json_t *spec) {
    json_t *e = json_object();
    json_set(e, "__exc__", json_string(pb3_raise(spec)));
    json_set(e, "msg", json_string(pb3_raise_msg(spec)));
    return e;
}

/* ── FULL builtin exception hierarchy (py3.14) ──
 * parent == NULL marks BaseException (the root). asyncio.CancelledError is a
 * BaseException subclass (py3.8+). Non-builtin scripted kinds default to
 * Exception parentage in pb3_exc_parent(). */
typedef struct { const char *name; const char *parent; } pb3_exc_ent_t;
static const pb3_exc_ent_t PB3_EXC_TREE[] = {
    {"BaseException", NULL},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"Exception", "BaseException"},
    {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"},
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
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"PythonFinalizationError", "RuntimeError"},
    {"_IncompleteInputError", "SyntaxError"},
    {"BaseExceptionGroup", "BaseException"},
    /* ExceptionGroup's first direct base is BaseExceptionGroup, but the
     * single-parent walk routes it through Exception to keep the
     * `except Exception` contract (it IS an Exception subclass). */
    {"ExceptionGroup", "Exception"},
    /* OSError aliases (errno-catch walks). */
    {"IOError", "OSError"},
    {"EnvironmentError", "OSError"},
    {"WindowsError", "OSError"},
};
static const char *pb3_exc_parent(const char *kind) {
    if (!kind) return NULL;
    for (size_t i = 0; i < sizeof(PB3_EXC_TREE) / sizeof(PB3_EXC_TREE[0]); i++)
        if (strcmp(kind, PB3_EXC_TREE[i].name) == 0) return PB3_EXC_TREE[i].parent;
    return "Exception";   /* synthetic scripted kind */
}
static bool pb3_exc_is_subclass(const char *kind, const char *ancestor) {
    if (!kind || !ancestor) return false;
    const char *cur = kind;
    for (int guard = 0; cur && guard < 64; guard++) {
        if (strcmp(cur, ancestor) == 0) return true;
        cur = pb3_exc_parent(cur);
    }
    return false;
}
static bool pb3_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0
                 || strcmp(kind, "KeyboardInterrupt") == 0
                 || strcmp(kind, "GeneratorExit") == 0
                 || strcmp(kind, "CancelledError") == 0
                 || strcmp(kind, "BaseException") == 0);
}
/* handler: a single class name, or a '|'-joined tuple ("OSError|RuntimeError|
 * ValueError"). "Exception" matches any Exception subclass (not the
 * BaseException-only trio); "BaseException" matches everything. */
static bool pb3_exc_matches(const char *kind, const char *handler) {
    if (!kind || !handler) return false;
    if (strcmp(handler, "BaseException") == 0) return true;
    if (strcmp(handler, "Exception") == 0) return pb3_exc_is_subclass(kind, "Exception");
    const char *p = handler;
    while (*p) {
        const char *bar = strchr(p, '|');
        size_t n = bar ? (size_t)(bar - p) : strlen(p);
        char comp[128];
        if (n >= sizeof(comp)) n = sizeof(comp) - 1;
        memcpy(comp, p, n); comp[n] = '\0';
        if (pb3_exc_is_subclass(kind, comp)) return true;
        if (!bar) break;
        p = bar + 1;
    }
    return false;
}

/* ── Python truthiness / None ── */
static bool pb3_truthy(const json_t *v) {
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
static bool pb3_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }

/* ── dynamic string buffer ── */
typedef struct { char *buf; size_t len, cap; } pb3_sb_t;
static void pb3_sb_init(pb3_sb_t *sb) { sb->buf = NULL; sb->len = 0; sb->cap = 0; }
static void pb3_sb_grow(pb3_sb_t *sb, size_t need) {
    if (sb->len + need + 1 <= sb->cap) return;
    size_t nc = sb->cap ? sb->cap : 64;
    while (nc < sb->len + need + 1) nc *= 2;
    char *nb = realloc(sb->buf, nc);
    if (!nb) return;
    sb->buf = nb; sb->cap = nc;
}
static void pb3_sb_addn(pb3_sb_t *sb, const char *s, size_t n) {
    if (!s || !n) return;
    pb3_sb_grow(sb, n);
    if (!sb->buf) return;
    memcpy(sb->buf + sb->len, s, n);
    sb->len += n;
    sb->buf[sb->len] = '\0';
}
static void pb3_sb_add(pb3_sb_t *sb, const char *s) { if (s) pb3_sb_addn(sb, s, strlen(s)); }
static void pb3_sb_addch(pb3_sb_t *sb, char c) { pb3_sb_addn(sb, &c, 1); }
static char *pb3_sb_take(pb3_sb_t *sb) {
    char *r = sb->buf ? sb->buf : strdup("");
    sb->buf = NULL; sb->len = 0; sb->cap = 0;
    return r;
}
static char *pb3_xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}
static bool pb3_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
static bool pb3_contains(const char *s, const char *sub) {
    if (!s || !sub) return false;
    return strstr(s, sub) != NULL;
}

/* ── UTF-8 + Unicode White_Space (str.strip / rstrip no-arg set) ── */
static size_t pb3_utf8_next(const char *s, size_t len, size_t i, unsigned *cp) {
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
static bool pb3_is_ws_cp(unsigned cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F))
        return true;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200A) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000)
        return true;
    return false;
}
static char *pb3_strip(const char *s) {
    if (!s) return pb3_xstrdup("");
    size_t len = strlen(s);
    size_t a = 0, b = len;
    while (a < b) {
        unsigned cp; size_t nx = pb3_utf8_next(s, len, a, &cp);
        if (!pb3_is_ws_cp(cp)) break;
        a = nx;
    }
    while (b > a) {
        size_t st = b - 1;
        while (st > a && ((unsigned char)s[st] & 0xC0) == 0x80) st--;
        unsigned cp; pb3_utf8_next(s, len, st, &cp);
        if (!pb3_is_ws_cp(cp)) break;
        b = st;
    }
    char *d = malloc(b - a + 1);
    if (d) { memcpy(d, s + a, b - a); d[b - a] = '\0'; }
    return d;
}
static char *pb3_rstrip(const char *s) {
    if (!s) return pb3_xstrdup("");
    size_t len = strlen(s), b = len;
    while (b > 0) {
        size_t st = b - 1;
        while (st > 0 && ((unsigned char)s[st] & 0xC0) == 0x80) st--;
        unsigned cp; pb3_utf8_next(s, len, st, &cp);
        if (!pb3_is_ws_cp(cp)) break;
        b = st;
    }
    char *d = malloc(b + 1);
    if (d) { memcpy(d, s, b); d[b] = '\0'; }
    return d;
}
/* len(str) — codepoint count. */
static size_t pb3_utf8_len(const char *s) {
    if (!s) return 0;
    size_t len = strlen(s), i = 0, n = 0;
    while (i < len) { unsigned cp; i = pb3_utf8_next(s, len, i, &cp); n++; }
    return n;
}

/* ── PurePosixPath str normalization + join + name ── */
static char *pb3_posix_norm(const char *s) {
    if (!s) return pb3_xstrdup("");
    size_t len = strlen(s);
    size_t lead = 0;
    while (lead < len && s[lead] == '/') lead++;
    bool abs = lead > 0;
    const char *root = (lead == 2) ? "//" : "/";
    pb3_sb_t sb; pb3_sb_init(&sb);
    if (abs) pb3_sb_add(&sb, root);
    size_t i = lead;
    bool first = true;
    while (i < len) {
        while (i < len && s[i] == '/') i++;
        size_t st = i;
        while (i < len && s[i] != '/') i++;
        if (i == st) break;
        size_t n = i - st;
        if (n == 1 && s[st] == '.') continue;
        if (!first) pb3_sb_addch(&sb, '/');
        pb3_sb_addn(&sb, s + st, n);
        first = false;
    }
    return pb3_sb_take(&sb);
}
static char *pb3_posix_name(const char *p) {
    char *n = pb3_posix_norm(p ? p : "");
    size_t l = strlen(n);
    if (l == 0 || (l == 1 && n[0] == '/')) { free(n); return pb3_xstrdup(""); }
    char *slash = strrchr(n, '/');
    char *r = pb3_xstrdup(slash ? slash + 1 : n);
    free(n);
    return r;
}
/* PurePosixPath.is_absolute() — leading '/' (POSIX fixture domain). */
static bool pb3_posix_is_absolute(const char *p) {
    return p && p[0] == '/';
}

/* ── seam call helpers ── */
static json_t *pb3_call_raw(const pb3_seams_t *s, const char *name, json_t *args) {
    json_t *a = args ? args : json_object();
    json_t *r = s->call(s->ctx, name, a);
    json_free(a);
    return r;
}
static json_t *pb3_call(const pb3_seams_t *s, const char *name, json_t *args, json_t **exc) {
    json_t *r = pb3_call_raw(s, name, args);
    if (pb3_raise(r)) { *exc = r; return NULL; }
    return r;
}
static void pb3_arg_str(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void pb3_arg_node(json_t *a, const char *k, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
/* logger.<level>(msg, *args) — args recorded verbatim; takes ownership of
 * args_array (NULL == empty). Returns false when the seam raised. */
static bool pb3_log(const pb3_seams_t *s, json_t **exc, const char *level,
                    const char *msg, json_t *args_array) {
    char name[32];
    snprintf(name, sizeof(name), "log_%s", level);
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_array ? args_array : json_array());
    json_t *r = pb3_call(s, name, a, exc);
    json_free(r);
    return *exc == NULL;
}
/* str seam result reader: strdup'd string or NULL (None/non-str). */
static char *pb3_str_result(const json_t *r) {
    if (r && r->type == JSON_STRING) return pb3_xstrdup(r->str_val);
    return NULL;
}
/* Import-gate spec (no seam event on a failed import). */
static json_t *pb3_import_exc(const char *modname) {
    char msg[256];
    snprintf(msg, sizeof(msg), "import of %s halted; None in sys.modules", modname);
    return pb3_raise_spec("ModuleNotFoundError", msg);
}
/* str(value) projection for the _opt closure (str|null|int|bool domain). */
static char *pb3_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return pb3_xstrdup("None");
    if (v->type == JSON_STRING) return pb3_xstrdup(v->str_val);
    if (v->type == JSON_BOOL) return pb3_xstrdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e16) snprintf(buf, sizeof(buf), "%.0f", d);
        else snprintf(buf, sizeof(buf), "%g", d);
        return pb3_xstrdup(buf);
    }
    return pb3_xstrdup("");
}

/* ══════════════════════════════════════════════════════════════════════════
 * GROUP A — cache-dir accessor factory (_cache_dir_accessors + get_dir/cleanup)
 * ══════════════════════════════════════════════════════════════════════════ */

/* The factory body only renames the two closures and returns the pair; no I/O.
 * Observable output: the renamed __name__ pair. */
/* PoP: pb3__cache_dir_accessors @ gateway/platforms/base.py:_cache_dir_accessors */
static json_t *pb3__cache_dir_accessors(const char *kind) {
    json_t *r = json_object();
    char buf[128];
    snprintf(buf, sizeof(buf), "get_%s_cache_dir", kind ? kind : "");
    json_set(r, "get_dir_name", json_string(buf));
    snprintf(buf, sizeof(buf), "cleanup_%s_cache", kind ? kind : "");
    json_set(r, "cleanup_name", json_string(buf));
    return r;
}

/* get_dir closure body:
 *   d = get_hermes_dir(new_subpath, old_name)
 *   current = globals().get(constant_name)        (input constant_value)
 *   default = _CACHE_DIR_IMPORT_DEFAULTS.get(constant_name)  (input import_defaults)
 *   if current is not None and default is not None and current != default:
 *       d = Path(current)
 *   _secure_media_cache_dir(d)
 *   return d
 */
/* PoP: pb3_cache_dir_get_dir @ gateway/platforms/base.py:get_dir */
static json_t *pb3_cache_dir_get_dir(const char *new_subpath, const char *old_name,
                                     const char *constant_name, const json_t *constant_value,
                                     const json_t *import_defaults,
                                     const pb3_seams_t *s, json_t **exc) {
    json_t *a = json_object();
    pb3_arg_str(a, "subpath", new_subpath);
    pb3_arg_str(a, "old_name", old_name);
    json_t *r = pb3_call(s, "get_hermes_dir", a, exc);
    if (*exc) return NULL;
    char *raw_d = pb3_str_result(r);
    json_free(r);
    char *d = pb3_posix_norm(raw_d ? raw_d : "");
    free(raw_d);
    /* current / default identity + inequality gate */
    const json_t *def = (import_defaults && constant_name)
        ? json_obj_get(import_defaults, constant_name) : NULL;
    if (!pb3_is_none(constant_value) && !pb3_is_none(def)) {
        char *cur = pb3_py_str(constant_value);
        char *dfs = pb3_py_str(def);
        char *cn = pb3_posix_norm(cur);
        char *dn = pb3_posix_norm(dfs);
        if (strcmp(cn, dn) != 0) {
            free(d);
            d = pb3_posix_norm(cur);
        }
        free(cur); free(dfs); free(cn); free(dn);
    }
    /* _secure_media_cache_dir(d) — claimed sibling, may raise (propagates) */
    json_t *sa = json_object();
    pb3_arg_str(sa, "path", d);
    json_t *sr = pb3_call(s, "secure_media_cache_dir", sa, exc);
    json_free(sr);
    if (*exc) { free(d); return NULL; }
    json_t *out = json_string(d);
    free(d);
    return out;
}

/* cleanup closure body:
 *   return unlink_files_older_than(get_dir(), "*", max_age_hours * 3600)
 */
/* PoP: pb3_cache_dir_cleanup @ gateway/platforms/base.py:cleanup */
static json_t *pb3_cache_dir_cleanup(const char *new_subpath, const char *old_name,
                                     const char *constant_name, const json_t *constant_value,
                                     const json_t *import_defaults, double max_age_hours,
                                     const pb3_seams_t *s, json_t **exc) {
    json_t *d = pb3_cache_dir_get_dir(new_subpath, old_name, constant_name,
                                      constant_value, import_defaults, s, exc);
    if (*exc) return NULL;
    char *ds = pb3_str_result(d);
    json_free(d);
    json_t *a = json_object();
    pb3_arg_str(a, "path", ds ? ds : "");
    pb3_arg_str(a, "pattern", "*");
    json_set(a, "max_age_seconds", json_number(max_age_hours * 3600.0));
    free(ds);
    json_t *r = pb3_call(s, "unlink_files_older_than", a, exc);
    if (*exc) return NULL;
    return r;   /* int count (or scripted) */
}

/* ══════════════════════════════════════════════════════════════════════════
 * GROUP B — media-delivery path validation
 * ══════════════════════════════════════════════════════════════════════════ */

/* PoP: pb3_validate_media_delivery_path @ gateway/platforms/base.py:validate_media_delivery_path */
static json_t *pb3_validate_media_delivery_path(const char *path, const char *session_key,
                                                bool hosted_policy_fail, bool media_policy_fail,
                                                const pb3_seams_t *s, json_t **exc) {
    /* from tools.hosted_tool_policy import host_feature_enabled (no try) */
    if (hosted_policy_fail) { *exc = pb3_import_exc("tools.hosted_tool_policy"); return NULL; }
    json_t *a = json_object();
    pb3_arg_str(a, "feature", "media_delivery");
    json_t *r = pb3_call(s, "host_feature_enabled", a, exc);
    if (*exc) return NULL;
    bool enabled = pb3_truthy(r);
    json_free(r);
    if (!enabled) return NULL;   /* short-circuit: normalize NOT called */
    /* candidate := _normalize_media_tag_path(path) */
    a = json_object();
    pb3_arg_str(a, "path", path);
    r = pb3_call(s, "normalize_media_tag_path", a, exc);
    if (*exc) return NULL;
    char *candidate = pb3_str_result(r);
    json_free(r);
    if (!candidate || candidate[0] == '\0') { free(candidate); return NULL; }
    /* try: expanded = Path(os.path.expanduser(candidate))
     * except (OSError, RuntimeError, ValueError): return None */
    a = json_object();
    pb3_arg_str(a, "path", candidate);
    r = pb3_call_raw(s, "expanduser", a);
    if (pb3_raise(r)) {
        const char *kind = pb3_raise(r);
        if (pb3_exc_matches(kind, "OSError|RuntimeError|ValueError")) {
            json_free(r); free(candidate); return NULL;
        }
        *exc = r; free(candidate); return NULL;
    }
    char *exp_s = pb3_str_result(r);
    json_free(r);
    free(candidate);
    char *expanded = pb3_posix_norm(exp_s ? exp_s : "");
    free(exp_s);
    if (!pb3_posix_is_absolute(expanded)) { free(expanded); return NULL; }
    /* resolved = _translate_docker_container_media_path(expanded, session_key=...)
     *            or _resolve_path(expanded, strict=True) */
    a = json_object();
    pb3_arg_str(a, "path", expanded);
    pb3_arg_str(a, "session_key", session_key);
    r = pb3_call(s, "translate_docker", a, exc);
    if (*exc) { free(expanded); return NULL; }
    char *resolved = pb3_str_result(r);
    json_free(r);
    if (!resolved || resolved[0] == '\0') {
        free(resolved);
        a = json_object();
        pb3_arg_str(a, "path", expanded);
        json_set(a, "strict", json_bool(true));
        json_set(a, "expand", json_bool(false));
        r = pb3_call(s, "resolve_path", a, exc);
        if (*exc) { free(expanded); return NULL; }
        resolved = pb3_str_result(r);
        json_free(r);
    }
    free(expanded);
    /* if resolved is None or not resolved.is_file(): return None */
    if (!resolved) return NULL;
    a = json_object();
    pb3_arg_str(a, "path", resolved);
    r = pb3_call(s, "is_file", a, exc);
    if (*exc) { free(resolved); return NULL; }
    bool isf = pb3_truthy(r);
    json_free(r);
    if (!isf) { free(resolved); return NULL; }
    /* if is_project_env_basename(resolved.name): return None */
    char *nm = pb3_posix_name(resolved);
    a = json_object();
    pb3_arg_str(a, "name", nm);
    free(nm);
    r = pb3_call(s, "is_project_env_basename", a, exc);
    if (*exc) { free(resolved); return NULL; }
    bool envb = pb3_truthy(r);
    json_free(r);
    if (envb) { free(resolved); return NULL; }
    /* for root in _media_delivery_allowed_roots():
     *     resolved_root = _resolve_path(root, expand=True)
     *     if resolved_root is not None and _path_is_within(resolved, resolved_root):
     *         return str(resolved) */
    r = pb3_call(s, "allowed_roots", json_object(), exc);
    if (*exc) { free(resolved); return NULL; }
    json_t *roots = r;   /* owned array (or NULL) */
    if (roots && roots->type == JSON_ARRAY) {
        for (size_t i = 0; i < roots->c.count; i++) {
            const json_t *root = roots->c.items[i];
            char *root_s = pb3_str_result(root);
            a = json_object();
            pb3_arg_str(a, "path", root_s ? root_s : "");
            json_set(a, "strict", json_bool(false));
            json_set(a, "expand", json_bool(true));
            free(root_s);
            json_t *rr = pb3_call(s, "resolve_path", a, exc);
            if (*exc) { json_free(roots); free(resolved); return NULL; }
            char *rroot = pb3_str_result(rr);
            json_free(rr);
            if (rroot) {   /* resolved_root is not None */
                a = json_object();
                pb3_arg_str(a, "path", resolved);
                pb3_arg_str(a, "root", rroot);
                json_t *wr = pb3_call(s, "path_is_within", a, exc);
                if (*exc) { free(rroot); json_free(roots); free(resolved); return NULL; }
                bool within = pb3_truthy(wr);
                json_free(wr);
                free(rroot);
                if (within) {
                    json_free(roots);
                    json_t *out = json_string(resolved);
                    free(resolved);
                    return out;
                }
            }
        }
    }
    json_free(roots);
    /* from gateway.media_policy import media_delivery_strict (no try) */
    if (media_policy_fail) {
        *exc = pb3_import_exc("gateway.media_policy"); free(resolved); return NULL;
    }
    r = pb3_call(s, "media_delivery_strict", json_object(), exc);
    if (*exc) { free(resolved); return NULL; }
    bool strict = pb3_truthy(r);
    json_free(r);
    if (!strict) {
        a = json_object();
        pb3_arg_str(a, "path", resolved);
        r = pb3_call(s, "path_under_denied_prefix", a, exc);
        if (*exc) { free(resolved); return NULL; }
        bool denied = pb3_truthy(r);
        json_free(r);
        if (denied) { free(resolved); return NULL; }
        json_t *out = json_string(resolved);
        free(resolved);
        return out;
    }
    /* window = _media_delivery_recency_seconds() */
    r = pb3_call(s, "recency_seconds", json_object(), exc);
    if (*exc) { free(resolved); return NULL; }
    double window = (r && r->type == JSON_NUMBER) ? r->num_val : 0.0;
    json_free(r);
    /* if window > 0 and not _path_under_denied_prefix(resolved)
     *        and _file_is_recently_produced(resolved, window): return str(resolved) */
    if (window > 0.0) {
        a = json_object();
        pb3_arg_str(a, "path", resolved);
        r = pb3_call(s, "path_under_denied_prefix", a, exc);
        if (*exc) { free(resolved); return NULL; }
        bool denied = pb3_truthy(r);
        json_free(r);
        if (!denied) {
            a = json_object();
            pb3_arg_str(a, "path", resolved);
            json_set(a, "window", json_number(window));
            r = pb3_call(s, "file_is_recently_produced", a, exc);
            if (*exc) { free(resolved); return NULL; }
            bool recent = pb3_truthy(r);
            json_free(r);
            if (recent) {
                json_t *out = json_string(resolved);
                free(resolved);
                return out;
            }
        }
    }
    free(resolved);
    return NULL;
}

/* PoP: pb3__validated_delivery_path @ gateway/platforms/base.py:_validated_delivery_path */
static json_t *pb3__validated_delivery_path(const char *raw_path, const char *session_key,
                                            const char *label, json_t *dropped,
                                            bool media_fetch_fail,
                                            const pb3_seams_t *s, json_t **exc) {
    char *raw = pb3_xstrdup(raw_path ? raw_path : "");
    /* safe_path = validate_media_delivery_path(raw, session_key=session_key) */
    json_t *a = json_object();
    pb3_arg_str(a, "path", raw);
    pb3_arg_str(a, "session_key", session_key);
    json_t *r = pb3_call(s, "validate_media_delivery_path", a, exc);
    if (*exc) { free(raw); return NULL; }
    char *safe = pb3_str_result(r);
    json_free(r);
    /* if not safe_path: from gateway.media_fetch import fetch_remote_media;
     *                    safe_path = fetch_remote_media(raw) */
    if (!safe || safe[0] == '\0') {
        free(safe); safe = NULL;
        if (media_fetch_fail) {
            *exc = pb3_import_exc("gateway.media_fetch"); free(raw); return NULL;
        }
        a = json_object();
        pb3_arg_str(a, "raw", raw);
        r = pb3_call(s, "fetch_remote_media", a, exc);
        if (*exc) { free(raw); return NULL; }
        safe = pb3_str_result(r);
        json_free(r);
    }
    /* if not safe_path: reason + logger.warning + dropped.append */
    if (!safe || safe[0] == '\0') {
        a = json_object();
        pb3_arg_str(a, "raw", raw);
        r = pb3_call(s, "existing_regular_file", a, exc);
        if (*exc) { free(safe); free(raw); return NULL; }
        bool exists = pb3_truthy(r);
        json_free(r);
        const char *reason = exists ? "denied by the delivery policy" : "not found on this host";
        /* _log_safe_path(raw) evaluated as a logger arg BEFORE log_warning fires */
        a = json_object();
        pb3_arg_str(a, "raw", raw);
        r = pb3_call(s, "log_safe_path", a, exc);
        if (*exc) { free(safe); free(raw); return NULL; }
        char *safe_log = pb3_str_result(r);
        json_free(r);
        json_t *args = json_array();
        json_append(args, json_string(label ? label : ""));
        json_append(args, json_string(reason));
        json_append(args, json_string(safe_log ? safe_log : ""));
        free(safe_log);
        if (!pb3_log(s, exc, "warning", "Skipping %s (%s): %s", args)) {
            free(safe); free(raw); return NULL;
        }
        if (dropped && dropped->type == JSON_ARRAY) {
            json_t *ent = json_object();
            json_set(ent, "path", json_string(raw));
            json_set(ent, "reason", json_string(reason));
            json_append(dropped, ent);
        }
        free(safe);
        free(raw);
        return NULL;
    }
    free(raw);
    json_t *out = json_string(safe);   /* safe non-empty */
    free(safe);
    return out;
}

/* ══════════════════════════════════════════════════════════════════════════
 * GROUP C — MEDIA tag-span composition (regex finditer seamed)
 * ══════════════════════════════════════════════════════════════════════════ */

/* A scripted match node {start,end,path}. Projection helpers. */
static json_t *pb3_match_proj(const json_t *m) {
    json_t *o = json_object();
    const json_t *st = m ? json_obj_get(m, "start") : NULL;
    const json_t *en = m ? json_obj_get(m, "end") : NULL;
    const json_t *pa = m ? json_obj_get(m, "path") : NULL;
    json_set(o, "start", json_number((st && st->type == JSON_NUMBER) ? st->num_val : 0));
    json_set(o, "end", json_number((en && en->type == JSON_NUMBER) ? en->num_val : 0));
    json_set(o, "path", json_string((pa && pa->type == JSON_STRING) ? pa->str_val : ""));
    return o;
}
static double pb3_match_start(const json_t *m) {
    const json_t *st = m ? json_obj_get(m, "start") : NULL;
    return (st && st->type == JSON_NUMBER) ? st->num_val : 0;
}
static const char *pb3_match_path(const json_t *m) {
    const json_t *pa = m ? json_obj_get(m, "path") : NULL;
    return (pa && pa->type == JSON_STRING) ? pa->str_val : "";
}
/* [start,end] span node from a match. */
static json_t *pb3_span_of(const json_t *m) {
    json_t *sp = json_array();
    json_append(sp, json_number(pb3_match_start(m)));
    const json_t *en = m ? json_obj_get(m, "end") : NULL;
    json_append(sp, json_number((en && en->type == JSON_NUMBER) ? en->num_val : 0));
    return sp;
}

/* _extensionless_media_matches(masked) — generator; ported as a list of
 * [match_proj, safe_path, end] triples in yield order.
 *   known = [m.span() for m in CLEANUP.finditer(masked)
 *            if "MEDIA:" not in m.group("path")
 *            and _existing_regular_file(_normalize_media_tag_path(m.group("path")))]
 *   for match in EXTENSIONLESS.finditer(masked):
 *       path = _normalize_media_tag_path(match.group("path"))
 *       if path and _path_lacks_deliverable_extension(path)
 *              and not any(s <= match.start() < e for s,e in known):
 *           resolved = _match_extensionless_path(masked, match)
 *           if resolved is not None: yield match, resolved[0], resolved[1]
 */
/* PoP: pb3__extensionless_media_matches @ gateway/platforms/base.py:_extensionless_media_matches */
static json_t *pb3__extensionless_media_matches(const char *masked, const pb3_seams_t *s, json_t **exc) {
    json_t *out = json_array();
    /* known spans */
    json_t *known = json_array();   /* array of [s,e] */
    json_t *a = json_object();
    pb3_arg_str(a, "text", masked);
    json_t *r = pb3_call(s, "cleanup_finditer", a, exc);
    if (*exc) { json_free(known); json_free(out); return NULL; }
    if (r && r->type == JSON_ARRAY) {
        for (size_t i = 0; i < r->c.count; i++) {
            const json_t *m = r->c.items[i];
            const char *mp = pb3_match_path(m);
            if (pb3_contains(mp, "MEDIA:")) continue;   /* short-circuit: no seams */
            /* _normalize_media_tag_path(path) */
            a = json_object();
            pb3_arg_str(a, "path", mp);
            json_t *nr = pb3_call(s, "normalize_media_tag_path", a, exc);
            if (*exc) { json_free(r); json_free(known); json_free(out); return NULL; }
            char *norm = pb3_str_result(nr);
            json_free(nr);
            /* _existing_regular_file(norm) — arg may be None (omitted) */
            a = json_object();
            pb3_arg_str(a, "raw", norm);   /* NULL -> omitted */
            json_t *er = pb3_call(s, "existing_regular_file", a, exc);
            if (*exc) { free(norm); json_free(r); json_free(known); json_free(out); return NULL; }
            bool exists = pb3_truthy(er);
            json_free(er);
            free(norm);
            if (exists) json_append(known, pb3_span_of(m));
        }
    }
    json_free(r);
    /* extensionless loop */
    a = json_object();
    pb3_arg_str(a, "text", masked);
    r = pb3_call(s, "extensionless_finditer", a, exc);
    if (*exc) { json_free(known); json_free(out); return NULL; }
    if (r && r->type == JSON_ARRAY) {
        for (size_t i = 0; i < r->c.count; i++) {
            const json_t *m = r->c.items[i];
            const char *mp = pb3_match_path(m);
            a = json_object();
            pb3_arg_str(a, "path", mp);
            json_t *nr = pb3_call(s, "normalize_media_tag_path", a, exc);
            if (*exc) { json_free(r); json_free(known); json_free(out); return NULL; }
            char *path = pb3_str_result(nr);
            json_free(nr);
            if (!path || path[0] == '\0') { free(path); continue; }   /* path falsy */
            a = json_object();
            pb3_arg_str(a, "path", path);
            json_t *lr = pb3_call(s, "path_lacks_deliverable_extension", a, exc);
            if (*exc) { free(path); json_free(r); json_free(known); json_free(out); return NULL; }
            bool lacks = pb3_truthy(lr);
            json_free(lr);
            if (!lacks) { free(path); continue; }
            /* not any(s <= match.start() < e for s,e in known) — pure */
            double ms = pb3_match_start(m);
            bool inside = false;
            for (size_t k = 0; k < known->c.count; k++) {
                const json_t *sp = known->c.items[k];
                if (sp->type == JSON_ARRAY && sp->c.count >= 2) {
                    double sk = sp->c.items[0]->num_val;
                    double ek = sp->c.items[1]->num_val;
                    if (sk <= ms && ms < ek) { inside = true; break; }
                }
            }
            free(path);
            if (inside) continue;
            /* resolved = _match_extensionless_path(masked, match) */
            a = json_object();
            pb3_arg_str(a, "masked", masked);
            json_set(a, "match", pb3_match_proj(m));
            json_t *rr = pb3_call(s, "match_extensionless_path", a, exc);
            if (*exc) { json_free(r); json_free(known); json_free(out); return NULL; }
            if (rr && rr->type == JSON_ARRAY && rr->c.count >= 2) {   /* resolved is not None */
                json_t *trip = json_array();
                json_append(trip, pb3_match_proj(m));
                json_append(trip, json_copy(rr->c.items[0]));
                json_append(trip, json_copy(rr->c.items[1]));
                json_append(out, trip);
            }
            json_free(rr);
        }
    }
    json_free(r);
    json_free(known);
    return out;
}

/* _real_media_tag_spans(masked):
 *   spans = [m.span() for m in CLEANUP.finditer(masked)]
 *   spans.extend((match.start(), end) for match,_,end in _extensionless_media_matches(masked))
 *   return spans
 */
/* PoP: pb3__real_media_tag_spans @ gateway/platforms/base.py:_real_media_tag_spans */
static json_t *pb3__real_media_tag_spans(const char *masked, const pb3_seams_t *s, json_t **exc) {
    json_t *spans = json_array();
    json_t *a = json_object();
    pb3_arg_str(a, "text", masked);
    json_t *r = pb3_call(s, "cleanup_finditer", a, exc);
    if (*exc) { json_free(spans); return NULL; }
    if (r && r->type == JSON_ARRAY)
        for (size_t i = 0; i < r->c.count; i++)
            json_append(spans, pb3_span_of(r->c.items[i]));
    json_free(r);
    /* _extensionless_media_matches(masked) — ported sibling, seamed */
    a = json_object();
    pb3_arg_str(a, "masked", masked);
    r = pb3_call(s, "extensionless_media_matches", a, exc);
    if (*exc) { json_free(spans); return NULL; }
    if (r && r->type == JSON_ARRAY) {
        for (size_t i = 0; i < r->c.count; i++) {
            const json_t *trip = r->c.items[i];
            if (trip->type == JSON_ARRAY && trip->c.count >= 3) {
                json_t *sp = json_array();
                json_append(sp, json_number(pb3_match_start(trip->c.items[0])));
                json_append(sp, json_copy(trip->c.items[2]));
                json_append(spans, sp);
            }
        }
    }
    json_free(r);
    return spans;
}

/* _deliverable_tag_spans(text):
 *   spans = _real_media_tag_spans(_mask_media_scan_text(text))
 *   start = _terminal_sentinel_start(text)
 *   if spans and start >= 0: spans.append((start, len(text.rstrip())))
 *   return spans
 */
/* PoP: pb3__deliverable_tag_spans @ gateway/platforms/base.py:_deliverable_tag_spans */
static json_t *pb3__deliverable_tag_spans(const char *text, const pb3_seams_t *s, json_t **exc) {
    /* _mask_media_scan_text(text) — arg seam fires first */
    json_t *a = json_object();
    pb3_arg_str(a, "text", text);
    json_t *r = pb3_call(s, "mask_media_scan_text", a, exc);
    if (*exc) return NULL;
    char *masked = pb3_str_result(r);
    json_free(r);
    /* _real_media_tag_spans(masked) — ported sibling, seamed */
    a = json_object();
    pb3_arg_str(a, "masked", masked ? masked : "");
    r = pb3_call(s, "real_media_tag_spans", a, exc);
    if (*exc) { free(masked); return NULL; }
    json_t *spans = (r && r->type == JSON_ARRAY) ? r : json_array();
    if (r && r->type != JSON_ARRAY) json_free(r);
    /* _terminal_sentinel_start(text) */
    a = json_object();
    pb3_arg_str(a, "text", text);
    r = pb3_call(s, "terminal_sentinel_start", a, exc);
    if (*exc) { free(masked); json_free(spans); return NULL; }
    double start = (r && r->type == JSON_NUMBER) ? r->num_val : -1;
    json_free(r);
    if (pb3_truthy(spans) && start >= 0) {
        char *rs = pb3_rstrip(text);
        size_t ln = pb3_utf8_len(rs);
        free(rs);
        json_t *sp = json_array();
        json_append(sp, json_number(start));
        json_append(sp, json_number((double)ln));
        json_append(spans, sp);
    }
    free(masked);
    return spans;
}

/* ══════════════════════════════════════════════════════════════════════════
 * GROUP D — small closures / delegation
 * ══════════════════════════════════════════════════════════════════════════ */

/* _synthesize_auto_tts(self, text_content, source=None):
 *   from gateway.platforms.base_auto_tts import synthesize_auto_tts
 *   return await synthesize_auto_tts(self, text_content, source, build_auto_tts_output_path)
 */
/* PoP: pb3__synthesize_auto_tts @ gateway/platforms/base.py:_synthesize_auto_tts */
static json_t *pb3__synthesize_auto_tts(const char *name, const char *text_content,
                                        const json_t *source, bool auto_tts_fail,
                                        const pb3_seams_t *s, json_t **exc) {
    if (auto_tts_fail) {
        *exc = pb3_import_exc("gateway.platforms.base_auto_tts");
        return NULL;
    }
    json_t *a = json_object();
    json_t *selfm = json_object();
    pb3_arg_str(selfm, "name", name);
    json_set(a, "self", selfm);
    pb3_arg_str(a, "text", text_content);
    pb3_arg_node(a, "source", source);   /* null omitted */
    pb3_arg_str(a, "build_fn", "build_auto_tts_output_path");
    json_t *r = pb3_call(s, "synthesize_auto_tts", a, exc);
    if (*exc) return NULL;
    return r;   /* scripted (tuple projects to list) */
}

/* _record_delivery(result) — nested closure mutating delivery_attempted /
 * delivery_succeeded. State is an explicit {delivery_attempted, delivery_succeeded}
 * object mutated in place; ret is None.
 *   if result is not None:
 *       delivery_attempted = True
 *       delivery_succeeded = delivery_succeeded or bool(getattr(result, "success", False))
 */
/* PoP: pb3__record_delivery @ gateway/platforms/base.py:_record_delivery */
static json_t *pb3__record_delivery(json_t *state, const json_t *result) {
    if (!pb3_is_none(result)) {
        json_set(state, "delivery_attempted", json_bool(true));
        bool prev = json_get_bool(state, "delivery_succeeded", false);
        bool succ = json_get_bool(result, "success", false);   /* getattr default False */
        json_set(state, "delivery_succeeded", json_bool(prev || succ));
    }
    return NULL;   /* closure returns None */
}

/* _opt(value) — nested closure: return str(value) if value else None. */
/* PoP: pb3__opt @ gateway/platforms/base.py:_opt */
static json_t *pb3__opt(const json_t *value) {
    if (!pb3_truthy(value)) return NULL;
    char *s = pb3_py_str(value);
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* ══════════════════════════════════════════════════════════════════════════
 * GROUP E — response extraction orchestrator (_extract_response_content)
 * ══════════════════════════════════════════════════════════════════════════ */

/* sorted(_IMAGE_EXTS) — frozenset {.jpg,.jpeg,.png,.webp,.gif} */
static json_t *pb3_image_exts(void) {
    static const char *exts[] = { ".gif", ".jpeg", ".jpg", ".png", ".webp" };
    json_t *a = json_array();
    for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++)
        json_append(a, json_string(exts[i]));
    return a;
}
/* [p for p,_ in media_files] + local_files */
static json_t *pb3_media_paths_plus(const json_t *media_files, const json_t *local_files) {
    json_t *a = json_array();
    if (media_files && media_files->type == JSON_ARRAY)
        for (size_t i = 0; i < media_files->c.count; i++) {
            const json_t *pair = media_files->c.items[i];
            if (pair->type == JSON_ARRAY && pair->c.count >= 1)
                json_append(a, json_copy(pair->c.items[0]));
        }
    if (local_files && local_files->type == JSON_ARRAY)
        for (size_t i = 0; i < local_files->c.count; i++)
            json_append(a, json_copy(local_files->c.items[i]));
    return a;
}
static bool pb3_arr_has(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *v = arr->c.items[i];
        if (v->type == JSON_STRING && strcmp(v->str_val, s) == 0) return true;
    }
    return false;
}

/* PoP: pb3__extract_response_content @ gateway/platforms/base.py:_extract_response_content */
static json_t *pb3__extract_response_content(const char *response_in, const json_t *source,
                                             const char *session_key, bool is_ephemeral,
                                             const char *name, bool docker_media_fail,
                                             const pb3_seams_t *s, json_t **exc) {
    char *response = pb3_xstrdup(response_in ? response_in : "");
    char *pre_extract = pb3_xstrdup(response);
    bool force_document = pb3_contains(response, "[[as_document]]");
    json_t *media_files = json_array();
    json_t *images = json_array();
    json_t *local_files = json_array();
    char *text_content = pb3_xstrdup("");
    json_t *ret = NULL;

    /* with self._media_delivery_scope(event.source): __enter__ */
    json_t *a = json_object();
    pb3_arg_node(a, "source", source);
    json_t *r = pb3_call(s, "media_delivery_scope_enter", a, exc);
    json_free(r);
    if (*exc) goto fail;   /* enter raised: body never runs, no __exit__ */

    /* media_files, response = self.extract_media(response) */
    a = json_object();
    pb3_arg_str(a, "content", response);
    r = pb3_call(s, "extract_media", a, exc);
    if (*exc) goto cm_exit;
    if (r && r->type == JSON_ARRAY && r->c.count >= 2) {
        json_free(media_files);
        media_files = json_copy(r->c.items[0]);
        char *nr = pb3_str_result(r->c.items[1]);
        free(response); response = nr ? nr : pb3_xstrdup("");
    }
    json_free(r);
    /* media_files = self.filter_media_delivery_paths(media_files, session_key=...) */
    a = json_object();
    pb3_arg_node(a, "media_files", media_files);
    pb3_arg_str(a, "session_key", session_key);
    r = pb3_call(s, "filter_media_delivery_paths", a, exc);
    if (*exc) goto cm_exit;
    if (r && r->type == JSON_ARRAY) { json_free(media_files); media_files = r; r = NULL; }
    json_free(r);
    /* images, text_content = self.extract_images(response) */
    a = json_object();
    pb3_arg_str(a, "response", response);
    r = pb3_call(s, "extract_images", a, exc);
    if (*exc) goto cm_exit;
    if (r && r->type == JSON_ARRAY && r->c.count >= 2) {
        json_free(images);
        images = json_copy(r->c.items[0]);
        char *nt = pb3_str_result(r->c.items[1]);
        free(text_content); text_content = nt ? nt : pb3_xstrdup("");
    }
    json_free(r);
    /* text_content = _strip_media_directives(text_content).strip() */
    a = json_object();
    pb3_arg_str(a, "text", text_content);
    r = pb3_call(s, "strip_media_directives", a, exc);
    if (*exc) goto cm_exit;
    {
        char *sd = pb3_str_result(r);
        char *st = pb3_strip(sd ? sd : "");
        free(sd); free(text_content); text_content = st;
    }
    json_free(r);
    /* if images: logger.info(...) */
    if (pb3_truthy(images)) {
        json_t *args = json_array();
        json_append(args, json_string(name ? name : ""));
        json_append(args, json_number((double)images->c.count));
        json_append(args, json_number((double)pb3_utf8_len(response)));
        if (!pb3_log(s, exc, "info",
                     "[%s] extract_images found %d image(s) in response (%d chars)", args))
            goto cm_exit;
    }
    /* local_files = []; if not is_ephemeral_response: ... */
    if (!is_ephemeral) {
        a = json_object();
        pb3_arg_str(a, "text", text_content);
        r = pb3_call(s, "extract_local_files", a, exc);
        if (*exc) goto cm_exit;
        if (r && r->type == JSON_ARRAY && r->c.count >= 2) {
            json_free(local_files);
            local_files = json_copy(r->c.items[0]);
            char *nt = pb3_str_result(r->c.items[1]);
            free(text_content); text_content = nt ? nt : pb3_xstrdup("");
        }
        json_free(r);
        a = json_object();
        pb3_arg_node(a, "files", local_files);
        pb3_arg_str(a, "session_key", session_key);
        r = pb3_call(s, "filter_local_delivery_paths", a, exc);
        if (*exc) goto cm_exit;
        if (r && r->type == JSON_ARRAY) { json_free(local_files); local_files = r; r = NULL; }
        json_free(r);
    }
    /* from gateway.platforms.docker_media_paths import hand_back_unreadable_media */
    if (docker_media_fail) {
        *exc = pb3_import_exc("gateway.platforms.docker_media_paths");
        goto cm_exit;
    }
    a = json_object();
    json_set(a, "paths", pb3_media_paths_plus(media_files, local_files));
    r = pb3_call(s, "hand_back_unreadable_media", a, exc);
    json_free(r);
    if (*exc) goto cm_exit;

cm_exit:
    {
        const char *bkind = *exc ? pb3_raise(*exc) : NULL;
        json_t *ea = json_object();
        if (bkind) json_set(ea, "exc", json_string(bkind));
        json_t *er = pb3_call_raw(s, "media_delivery_scope_exit", ea);
        if (pb3_raise(er)) {
            if (*exc) json_free(*exc);
            *exc = er;
            goto fail;
        }
        bool suppress = pb3_truthy(er);
        json_free(er);
        if (*exc) {
            if (suppress) { json_free(*exc); *exc = NULL; }
            else goto fail;
        }
    }

    /* history = await self._bounded_history_media_paths_for_session(session_key)
     *           if local_files else None */
    json_t *history = NULL;
    if (pb3_truthy(local_files)) {
        a = json_object();
        pb3_arg_str(a, "session_key", session_key);
        history = pb3_call(s, "bounded_history", a, exc);
        if (*exc) goto fail;
    }
    /* if history: suppressed = [p in local_files if p in history]; ... */
    if (pb3_truthy(history)) {
        json_t *suppressed = json_array();
        for (size_t i = 0; i < local_files->c.count; i++) {
            const json_t *p = local_files->c.items[i];
            if (p->type == JSON_STRING && pb3_arr_has(history, p->str_val))
                json_append(suppressed, json_copy(p));
        }
        if (pb3_truthy(suppressed)) {
            json_t *args = json_array();
            json_append(args, json_string(name ? name : ""));
            json_append(args, json_number((double)suppressed->c.count));
            json_append(args, json_copy(suppressed));
            if (!pb3_log(s, exc, "info",
                         "[%s] Suppressing %d bare local file path(s) already delivered in "
                         "this session: %s", args)) {
                json_free(suppressed); json_free(history); goto fail;
            }
        }
        json_free(suppressed);
        json_t *kept = json_array();
        for (size_t i = 0; i < local_files->c.count; i++) {
            const json_t *p = local_files->c.items[i];
            if (!(p->type == JSON_STRING && pb3_arr_has(history, p->str_val)))
                json_append(kept, json_copy(p));
        }
        json_free(local_files);
        local_files = kept;
    }
    json_free(history);
    /* if local_files: logger.info(...) */
    if (pb3_truthy(local_files)) {
        json_t *args = json_array();
        json_append(args, json_string(name ? name : ""));
        json_append(args, json_number((double)local_files->c.count));
        if (!pb3_log(s, exc, "info",
                     "[%s] extract_local_files found %d file(s) in response", args))
            goto fail;
    }
    /* if not (text_content or images or local_files or media_files): recover */
    if (!( (text_content && text_content[0]) || pb3_truthy(images)
           || pb3_truthy(local_files) || pb3_truthy(media_files) )) {
        a = json_object();
        pb3_arg_str(a, "text", response);
        r = pb3_call(s, "strip_media_directives", a, exc);
        if (*exc) goto fail;
        char *sd = pb3_str_result(r);
        json_free(r);
        char *recovered = pb3_strip(sd ? sd : "");
        free(sd);
        if (recovered && recovered[0]) {
            json_t *args = json_array();
            json_append(args, json_string(name ? name : ""));
            json_append(args, json_number((double)pb3_utf8_len(pre_extract)));
            const json_t *cid = source ? json_obj_get(source, "chat_id") : NULL;
            json_append(args, json_string((cid && cid->type == JSON_STRING) ? cid->str_val : ""));
            if (!pb3_log(s, exc, "warning",
                         "[%s] response_delivery_recovered: extract pipeline reduced a "
                         "non-empty response (%d chars) to empty with no attachment; "
                         "delivering recovered original to %s", args)) {
                free(recovered); goto fail;
            }
            free(text_content);
            text_content = recovered;
            recovered = NULL;
        }
        free(recovered);
    }
    /* return footer_onto_photo_caption(self, _ExtractedResponse(...), _IMAGE_EXTS) */
    {
        json_t *extracted = json_object();
        json_set(extracted, "text_content", json_string(text_content));
        json_set(extracted, "images", json_copy(images));
        json_set(extracted, "media_files", json_copy(media_files));
        json_set(extracted, "local_files", json_copy(local_files));
        json_set(extracted, "force_document_attachments", json_bool(force_document));
        json_set(extracted, "pre_extract", json_string(pre_extract));
        a = json_object();
        json_t *selfm = json_object();
        pb3_arg_str(selfm, "name", name);
        json_set(a, "self", selfm);
        json_set(a, "extracted", extracted);
        json_set(a, "image_exts", pb3_image_exts());
        ret = pb3_call(s, "footer_onto_photo_caption", a, exc);
        if (*exc) { ret = NULL; goto fail; }
    }

fail:
    free(response); free(pre_extract); free(text_content);
    json_free(media_files); json_free(images); json_free(local_files);
    if (*exc) { json_free(ret); return NULL; }
    return ret;
}