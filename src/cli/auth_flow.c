/*
 * auth_flow.c — FLOW port of the UNCLAIMED auth surface of
 * hermes_cli/auth.py (LIVE authority). Self-contained: libjson only. All
 * functions static au_*; the harness (#include "cli/auth_flow.c") wires the
 * universal seam to scripted results and records the ordered trace.
 *
 * Covered flows (PoP per function):
 *   registry/config leaves: _api_key_provider, _empty_auth_store,
 *     _store_section, _provider_state_in, _slot_selects,
 *     _model_level_key_env, _config_selects_provider,
 *     _explicit_env_credentials_present, _keyless_provider_has_explicit_config,
 *     _active_provider_is, _plugin_aliases, _config_model_provider,
 *     _credential_profile, _env_key_auto_detected, _scoped_key_env_reader,
 *     _openrouter_auto_detected, _logged_in_oauth_active_provider,
 *     _refuse_env_adoption_if_config_corrupt
 *   secret/env gates: _env_secret, _pool_entry_is_explicit,
 *     _explicit_pool_entry_present
 *   lock cluster: _is_lock_contention, _lock_holder_hint, _kernel_lock,
 *     _stamp_lock_holder_pid, _clear_stamped_lock_holder_pid
 *   token/TTL markers: _tls_state_from_verify, _utc_now_z,
 *     _last_auth_error_marker, _quarantine_flat_oauth_state,
 *     _is_terminal_refresh_error, _codex_pool_rate_limited_status
 *   persistence/status: _resolved_key, _save_private_json,
 *     _save_active_provider_state, suppressed_credential_sources,
 *     _get_aws_sdk_auth_status, _external_process_spec
 *
 * SEAM DOCTRINE: every live effect enters through ONE universal seam
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * returning a NEW json_t* (NULL == Python None). A raising live call is
 * scripted {"__raise__": "<ExcClass>", "msg": ...}; the cores propagate it
 * exactly where the live try/except catches it. except Exception does NOT
 * catch KeyboardInterrupt/SystemExit/GeneratorExit/BaseException — those pass
 * through to the harness ret envelope (BaseException passthrough). A MISSING
 * seam key fail-closes: the harness delivers {"__raise__":"RuntimeError",
 * "msg":"unscripted seam: <name>"}; a present JSON null is a valid None.
 * Claimed siblings (has_usable_secret, looks_like_openrouter_key,
 * read_credential_pool, _load_auth_store, get_auth_status,
 * _save_provider_state, _save_auth_store, _auth_store_lock, _auth_file_path)
 * and foreign collaborators are SEAMED; in-function imports fire an
 * "import_<name>" seam on module attribute access, mirroring the oracle's
 * stub modules. Claimed mutation siblings here (_save_provider_state /
 * _save_auth_store) are inert recorders on BOTH sides: no out-state is
 * echoed for their flows, so their store mutations are unobservable.
 *
 * Seam name table (name -> live call, args):
 *   has_usable_secret            has_usable_secret(value) -> bool
 *   looks_like_openrouter_key    looks_like_openrouter_key(value) -> bool
 *   read_credential_pool         read_credential_pool(provider_id) -> list
 *   load_auth_store              _load_auth_store() -> dict
 *   get_auth_status              get_auth_status(provider_id) -> dict
 *   save_provider_state          _save_provider_state(auth_store,provider_id,state)
 *   save_auth_store              _save_auth_store(auth_store[,target_path]) -> path
 *   auth_store_lock              _auth_store_lock(timeout_seconds) CM factory
 *   auth_store_lock_enter        CM __enter__
 *   auth_store_lock_exit         CM __exit__ (raise overrides body raise)
 *   auth_file_path               _auth_file_path() -> path str
 *   get_config_path              get_config_path() -> path str (bare global)
 *   canonical_registry_rows      _canonical_registry_rows() -> [[pid,pconfig]]
 *   codex_pool_rate_limit_status _codex_pool_rate_limit_status() -> dict|null
 *   provider_env_base_url        provider_env_base_url(pconfig) -> str
 *   secure_parent_dir            secure_parent_dir(target)
 *   atomic_json_write            atomic_json_write(target,data,mode,fsync_dir,**kw)
 *   datetime_now                 datetime.now(UTC).isoformat() -> str
 *   path_resolve                 Path(path).resolve(strict=False) -> str
 *   path_read_text               Path(path).read_text(encoding,errors) -> str
 *   os_getpid                    os.getpid() -> int
 *   os_kill                      os.kill(pid, sig) (posix branch only)
 *   os_getenv                    os.getenv(name[,default]) -> str|null
 *   file_seek                    lock_file.seek(offset)
 *   file_fileno                  lock_file.fileno() -> int
 *   file_truncate                lock_file.truncate(size)
 *   file_write                   lock_file.write(data)
 *   file_flush                   lock_file.flush()
 *   fcntl_flock                  fcntl.flock(fd, flags) (fcntl backend)
 *   msvcrt_locking               msvcrt.locking(fd, mode, nbytes) (msvcrt backend)
 *   shlex_split                  shlex.split(raw) -> [str]
 *   import_load_config / load_config
 *   import_get_env_value_prefer_dotenv / get_env_value_prefer_dotenv {name}
 *   import_get_provider / get_provider {provider_id}
 *   import_has_explicit_vertex_config / has_explicit_vertex_config
 *   import_get_active_config_parse_failure / get_active_config_parse_failure
 *   import_list_providers / list_providers -> [{name,aliases,ambient_credentials}]
 *   import_scoped_key_env        (agent.auxiliary_client._scoped_key_env)
 *   scoped_key_env               _scoped_key_env(name) -> str
 *   import_load_pool / load_pool {provider_id} -> pool record|null
 *   has_credentials              pool.has_credentials() -> bool
 *   import_free_inference_active / free_inference_active
 *   import_has_guest / has_guest
 *   import_has_named_custom_provider / has_named_custom_provider {provider}
 *   import_config_base_url_trustworthy_for_bare_custom /
 *     config_base_url_trustworthy_for_bare_custom {base_url,provider}
 *   import_get_provider_profile / get_provider_profile {provider_id}
 *   import_has_aws_credentials / has_aws_credentials
 *   import_external_process_command / external_process_command {profile}
 *   import_resolve_external_process_command /
 *     resolve_external_process_command {command}
 *   import_mkdir_under_hermes_home / mkdir_under_hermes_home {path}
 *
 * Callable markers: _scoped_key_env_reader returns a function; it projects to
 * {"__callable__":"scoped_key_env"} (the imported reader) or
 * {"__callable__":"os_getenv"} (the ImportError fallback lambda). Callers
 * (_openrouter_auto_detected, _env_key_auto_detected) take the marker as an
 * input and fire the matching seam per key read; the os_getenv marker applies
 * the live `or ""` falsy collapse.
 *
 * Platform doubles (fixture-driven, mirroring the oracle): os_name input
 * ("nt" default; "posix" exercises the os.kill branch of _lock_holder_hint),
 * lock_backend input ("msvcrt" default on Windows, "fcntl", or "none" — both
 * modules None, so _kernel_lock's else branch raises the live NoneType
 * AttributeError on msvcrt.locking).
 *
 * Fixture domains (documented exclusions):
 * - ASCII strings; POSIX path strings. Path inputs are normalized at the
 *   call boundary exactly like PurePosixPath (drop '.'/empty components,
 *   keep '..', exactly two leading slashes preserved); parent uses the same
 *   component algebra.
 * - JSON numbers have no int/float identity in libjson: integral-float
 *   literals (5.0) are excluded where live distinguishes type names
 *   (AttributeError messages) or str() renders ".0"; use integral ints or
 *   non-integral floats (5.5).
 * - has_usable_secret / looks_like_openrouter_key scripts are bools.
 * - Truthiness-gated seam records (get_provider, registry pconfig entries)
 *   are null or NON-EMPTY dicts carrying the fields the flow reads
 *   (auth_type, api_key_env_vars); an empty {} record is outside the domain
 *   (live SimpleNamespace is truthy, libjson {} is falsy).
 * - load_pool scripts: null (-> live NoneType AttributeError on
 *   .has_credentials, swallowed by the caller) or a record (object presence,
 *   {} allowed — no truthiness gate on it).
 * - list_providers rows carry name/aliases/ambient_credentials;
 *   canonical_registry_rows pairs carry auth_type/api_key_env_vars;
 *   alias/env-var/args lists are JSON arrays or null.
 * - int() tokens (_lock_holder_hint): [+-]?digits, |pid| < 2^53, no
 *   underscores; str.split()/strip() whitespace is the ASCII White_Space set
 *   (incl \x1c-\x1f for split/strip; int() never sees them post-split).
 * - shlex.split is SEAMED (no quote grammar ported); os.getenv scripts are
 *   strings; datetime_now scripts are ISO strings (null exercised where the
 *   live double returns None into .replace / the marker dict).
 * - logger is silent (no trace events), matching the sibling flow ports.
 * - AuthError doubles: {"kind":"AuthError","message","provider","code",
 *   "relogin_required"}; the oracle materializes the REAL extracted
 *   AuthError class (isinstance identity), C reads the fields.
 *
 * Ownership: seam results and returned json_t / char pointers are malloc'd,
 * caller frees. Inputs are borrowed. au_call borrows args (caller frees).
 * A propagated raise dict is returned upward as-is; a caught raise dict is
 * freed at the catch site. au_store_section returns a COPY of the section
 * (the in-place mutation travels in the harness out-state echo).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include "json.h"

/* ── universal seam + trace ── */

typedef struct au_trace { json_t *events; } au_trace_t;

typedef struct au_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} au_seams_t;

static void au_trace_add(au_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void au_trace_free(au_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *au_call(const au_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static json_t *au_call0(const au_seams_t *s, const char *name) {
    json_t *a = json_object();
    json_t *r = au_call(s, name, a);
    json_free(a);
    return r;
}

/* ── JSON value helpers ── */

static bool au_jtruth(const json_t *v) {
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
static const char *au_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool au_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static char *au_xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}

typedef struct { char *p; size_t len, cap; } au_buf;
static void au_binit(au_buf *b) { b->p = NULL; b->len = 0; b->cap = 0; }
static void au_bfree(au_buf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }
static void au_bput(au_buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->len + n + 1) nc *= 2;
        char *np = realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}
static void au_bputs(au_buf *b, const char *s) { au_bput(b, s, strlen(s)); }
static void au_bputc(au_buf *b, char c) { au_bput(b, &c, 1); }
static char *au_btake(au_buf *b) {
    if (!b->p) return strdup("");
    char *r = b->p; b->p = NULL; b->len = b->cap = 0; return r;
}

/* ── exception model (FULL builtin parent hierarchy) ── */

static const char *au_raise_class(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool au_is_raise(const json_t *r) { return au_raise_class(r) != NULL; }

static json_t *au_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

/* Parent lookup mirroring the py3.14 builtin hierarchy. Unknown custom kinds
 * default to Exception (the oracle builds type(name,(Exception,)) for them);
 * the BaseException trio stays OUTSIDE Exception so except Exception passes
 * them through. */
static bool au_in_names(const char *k, const char *const *names) {
    for (size_t i = 0; names[i]; i++) if (!strcmp(k, names[i])) return true;
    return false;
}
static const char *au_exc_parent(const char *k) {
    if (!strcmp(k, "BaseException")) return NULL;
    if (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt") ||
        !strcmp(k, "GeneratorExit")) return "BaseException";
    if (!strcmp(k, "Exception")) return "BaseException";
    static const char *const EXC_CHILDREN[] = {
        "StopIteration", "StopAsyncIteration", "ArithmeticError", "AssertionError",
        "AttributeError", "BufferError", "EOFError", "ImportError", "LookupError",
        "MemoryError", "NameError", "OSError", "ReferenceError", "RuntimeError",
        "SyntaxError", "SystemError", "TypeError", "ValueError", "Warning", NULL
    };
    if (au_in_names(k, EXC_CHILDREN)) return "Exception";
    static const char *const ARITH_CHILDREN[] = {
        "FloatingPointError", "OverflowError", "ZeroDivisionError", NULL
    };
    if (au_in_names(k, ARITH_CHILDREN)) return "ArithmeticError";
    static const char *const IMPORT_CHILDREN[] = { "ModuleNotFoundError", "ZipImportError", NULL };
    if (au_in_names(k, IMPORT_CHILDREN)) return "ImportError";
    static const char *const LOOKUP_CHILDREN[] = { "IndexError", "KeyError", NULL };
    if (au_in_names(k, LOOKUP_CHILDREN)) return "LookupError";
    static const char *const OSERROR_CHILDREN[] = {
        "BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", NULL
    };
    if (au_in_names(k, OSERROR_CHILDREN)) return "OSError";
    static const char *const CONN_CHILDREN[] = {
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", NULL
    };
    if (au_in_names(k, CONN_CHILDREN)) return "ConnectionError";
    static const char *const RUNTIME_CHILDREN[] = { "NotImplementedError", "RecursionError", NULL };
    if (au_in_names(k, RUNTIME_CHILDREN)) return "RuntimeError";
    if (!strcmp(k, "AuthError")) return "RuntimeError";
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    static const char *const UNICODE_CHILDREN[] = {
        "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError", NULL
    };
    if (au_in_names(k, UNICODE_CHILDREN)) return "UnicodeError";
    if (!strcmp(k, "JSONDecodeError")) return "ValueError";
    static const char *const WARNING_CHILDREN[] = {
        "UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", NULL
    };
    if (au_in_names(k, WARNING_CHILDREN)) return "Warning";
    static const char *const NAME_CHILDREN[] = { "UnboundLocalError", NULL };
    if (au_in_names(k, NAME_CHILDREN)) return "NameError";
    /* py3.11+ groups / py3.13+ finalization / py3.14 interactive */
    if (!strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "BaseExceptionGroup";
    if (!strcmp(k, "PythonFinalizationError")) return "RuntimeError";
    if (!strcmp(k, "_IncompleteInputError")) return "SyntaxError";
    return "Exception";  /* unknown custom kind: oracle bases it on Exception */
}
/* caught(r, cls): cls appears anywhere in r's parent chain */
static bool au_caught(const json_t *r, const char *cls) {
    const char *k = au_raise_class(r);
    /* ExceptionGroup multiply-inherits (BaseExceptionGroup, Exception) */
    if (k && !strcmp(k, "ExceptionGroup") && !strcmp(cls, "Exception")) return true;
    while (k) {
        if (!strcmp(k, cls)) return true;
        k = au_exc_parent(k);
    }
    return false;
}
static bool au_is_exception(const json_t *r) { return au_caught(r, "Exception"); }

/* Python type name of a JSON value (integral numbers read as int) */
static const char *au_typename(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val) && isfinite(v->num_val)) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}
static json_t *au_attr_raise(const char *tname, const char *attr) {
    char *msg = au_xsf("'%s' object has no attribute '%s'", tname, attr);
    json_t *e = au_raise_spec("AttributeError", msg);
    free(msg);
    return e;
}
static json_t *au_not_iterable_raise(const json_t *v) {
    char *msg = au_xsf("'%s' object is not iterable", au_typename(v));
    json_t *e = au_raise_spec("TypeError", msg);
    free(msg);
    return e;
}

/* ── args builders ── */

static void au_aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void au_aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void au_aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void au_aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ── Python float repr / str / repr ── */

static char *au_float_repr(double v) {
    if (v == 0.0) return strdup(signbit(v) ? "-0.0" : "0.0");
    if (!isfinite(v)) return strdup(v > 0 ? "inf" : (isnan(v) ? "nan" : "-inf"));
    char tmp[64];
    int prec;
    for (prec = 0; prec <= 16; prec++) {
        snprintf(tmp, sizeof(tmp), "%.*e", prec, v);
        if (strtod(tmp, NULL) == v) break;
    }
    if (prec > 16) { prec = 16; snprintf(tmp, sizeof(tmp), "%.*e", prec, v); }
    const char *p = tmp;
    bool neg = false;
    if (*p == '-') { neg = true; p++; }
    char digits[32];
    size_t nd = 0;
    digits[nd++] = *p++;
    if (*p == '.') { p++; while (*p && *p != 'e' && *p != 'E') digits[nd++] = *p++; }
    digits[nd] = '\0';
    int exp = 0;
    if (*p == 'e' || *p == 'E') exp = atoi(p + 1);
    while (nd > 1 && digits[nd - 1] == '0') digits[--nd] = '\0';
    au_buf b; au_binit(&b);
    if (neg) au_bputc(&b, '-');
    if (exp >= -4 && exp < 16) {
        if (exp >= 0) {
            size_t point = (size_t)exp + 1;
            if (nd <= point) {
                au_bput(&b, digits, nd);
                for (size_t i = nd; i < point; i++) au_bputc(&b, '0');
                au_bputs(&b, ".0");
            } else {
                au_bput(&b, digits, point);
                au_bputc(&b, '.');
                au_bputs(&b, digits + point);
            }
        } else {
            au_bputs(&b, "0.");
            for (int i = 0; i < -exp - 1; i++) au_bputc(&b, '0');
            au_bputs(&b, digits);
        }
    } else {
        au_bputc(&b, digits[0]);
        if (nd > 1) { au_bputc(&b, '.'); au_bputs(&b, digits + 1); }
        char ex[16];
        snprintf(ex, sizeof(ex), "e%s%02d", exp < 0 ? "-" : "+", exp < 0 ? -exp : exp);
        au_bputs(&b, ex);
    }
    return au_btake(&b);
}

static char *au_repr_string(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    au_buf b; au_binit(&b);
    au_bputc(&b, q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { au_bputc(&b, '\\'); au_bputc(&b, (char)c); }
        else if (c == '\n') au_bputs(&b, "\\n");
        else if (c == '\r') au_bputs(&b, "\\r");
        else if (c == '\t') au_bputs(&b, "\\t");
        else if (c < 0x20 || c == 0x7f) { char x[8]; snprintf(x, sizeof(x), "\\x%02x", c); au_bputs(&b, x); }
        else au_bputc(&b, (char)c);
    }
    au_bputc(&b, q);
    return au_btake(&b);
}

static char *au_pyrepr(const json_t *v);
static char *au_pystr(const json_t *v) {
    if (au_is_null(v)) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 1e16) {
                if (d == 0.0) return strdup("0");
                return au_xsf("%.0f", d);
            }
            return au_float_repr(d);
        }
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default: return au_pyrepr(v);
    }
}
static char *au_pyrepr(const json_t *v) {
    if (au_is_null(v)) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 1e16) {
                if (d == 0.0) return strdup("0");
                return au_xsf("%.0f", d);
            }
            return au_float_repr(d);
        }
        case JSON_STRING: return au_repr_string(v->str_val ? v->str_val : "");
        case JSON_ARRAY: {
            au_buf b; au_binit(&b);
            au_bputc(&b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) au_bputs(&b, ", ");
                char *r = au_pyrepr(v->c.items[i]);
                au_bputs(&b, r); free(r);
            }
            au_bputc(&b, ']');
            return au_btake(&b);
        }
        case JSON_OBJECT: {
            au_buf b; au_binit(&b);
            au_bputc(&b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) au_bputs(&b, ", ");
                char *k = au_repr_string(v->c.keys[i]);
                char *r = au_pyrepr(v->c.items[i]);
                au_bputs(&b, k); au_bputs(&b, ": "); au_bputs(&b, r);
                free(k); free(r);
            }
            au_bputc(&b, '}');
            return au_btake(&b);
        }
        default: return strdup("None");
    }
}

/* ── ASCII string helpers (str.strip/lower/split/replace/startswith) ── */

static bool au_is_ws(unsigned char c) {
    return c == ' ' || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}
static char *au_strip(const char *s) {
    while (*s && au_is_ws((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && au_is_ws((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - s);
    char *r = malloc(n + 1);
    memcpy(r, s, n); r[n] = '\0';
    return r;
}
static char *au_lower(const char *s) {
    char *r = strdup(s);
    for (char *p = r; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return r;
}
static bool au_startswith(const char *s, const char *pre) {
    return strncmp(s, pre, strlen(pre)) == 0;
}
static char *au_replace_all(const char *s, const char *from, const char *to) {
    size_t fl = strlen(from), tl = strlen(to);
    au_buf b; au_binit(&b);
    if (fl == 0) { au_bputs(&b, s); return au_btake(&b); }
    const char *p = s;
    while (*p) {
        if (strncmp(p, from, fl) == 0) { au_bputs(&b, to); p += fl; }
        else { au_bputc(&b, *p); p++; }
    }
    return au_btake(&b);
}
/* str.split() (no args): runs of White_Space, no empty tokens */
static json_t *au_split_ws(const char *s) {
    json_t *out = json_array();
    const char *p = s;
    while (*p) {
        while (*p && au_is_ws((unsigned char)*p)) p++;
        if (!*p) break;
        const char *st = p;
        while (*p && !au_is_ws((unsigned char)*p)) p++;
        char *tok = malloc((size_t)(p - st) + 1);
        memcpy(tok, st, (size_t)(p - st)); tok[p - st] = '\0';
        json_append(out, json_string(tok));
        free(tok);
    }
    return out;
}
/* int(token): [+-]?digits (fixture domain: no underscores, |v| < 2^53) */
static bool au_int_token(const char *s, double *out) {
    if (!s || !*s) return false;
    const char *p = s;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (!*p) return false;
    double v = 0.0;
    for (; *p; p++) {
        if (*p < '0' || *p > '9') return false;
        v = v * 10.0 + (*p - '0');
        if (v >= 9007199254740992.0) return false;  /* outside fixture domain */
    }
    *out = neg ? -v : v;
    return true;
}
/* PurePosixPath(s) stringification: drop empty + '.' components, keep '..',
 * 1 leading slash -> /, exactly 2 -> //, 3+ -> /, no components -> "." */
static char *au_path_norm(const char *s) {
    size_t lead = 0;
    while (s[lead] == '/') lead++;
    const char *root = lead == 2 ? "//" : (lead >= 1 ? "/" : "");
    struct { const char *p; size_t n; } parts[256];
    size_t np = 0;
    const char *p = s + lead;
    while (*p && np < 256) {
        const char *st = p;
        while (*p && *p != '/') p++;
        size_t n = (size_t)(p - st);
        if (n > 0 && !(n == 1 && st[0] == '.')) { parts[np].p = st; parts[np].n = n; np++; }
        if (*p == '/') p++;
    }
    if (np == 0) return strdup(root[0] ? root : ".");
    au_buf b; au_binit(&b);
    au_bputs(&b, root);
    for (size_t i = 0; i < np; i++) {
        if (i) au_bputc(&b, '/');
        au_bput(&b, parts[i].p, parts[i].n);
    }
    return au_btake(&b);
}

/* PurePosixPath(s).parent: component algebra — drop empty + '.' parts, keep
 * '..', exactly two leading slashes preserved (3+ collapse to one). */
static char *au_path_parent(const char *s) {
    size_t lead = 0;
    while (s[lead] == '/') lead++;
    const char *root = lead == 2 ? "//" : (lead >= 1 ? "/" : "");
    struct { const char *p; size_t n; } parts[256];
    size_t np = 0;
    const char *p = s + lead;
    while (*p && np < 256) {
        const char *st = p;
        while (*p && *p != '/') p++;
        size_t n = (size_t)(p - st);
        if (n > 0 && !(n == 1 && st[0] == '.')) { parts[np].p = st; parts[np].n = n; np++; }
        if (*p == '/') p++;
    }
    if (np == 0) return strdup(root[0] ? root : ".");
    np--;  /* drop the last component */
    if (np == 0) return strdup(root[0] ? root : ".");
    au_buf b; au_binit(&b);
    au_bputs(&b, root);
    for (size_t i = 0; i < np; i++) {
        if (i) au_bputc(&b, '/');
        au_bput(&b, parts[i].p, parts[i].n);
    }
    return au_btake(&b);
}

/* ── module constants (AST-seeded on the oracle side) ── */

/* path-normalizing inner bodies (defined below their PoP wrappers) */
static json_t *au_lock_holder_hint_n(const au_seams_t *s, const char *path, const char *os_name);
static json_t *au_resolved_key_n(const au_seams_t *s, const char *path);
static json_t *au_save_private_json_n(const au_seams_t *s, const char *target,
                                      const json_t *data, bool fsync_dir,
                                      const json_t *dump_kwargs);

#define AU_AUTH_STORE_VERSION 1
#define AU_AUTH_LOCK_TIMEOUT_SECONDS 15.0
#define AU_CODEX_RATE_LIMITED_CODE "codex_rate_limited"

static const char *const AU_FLAT_OAUTH_TOKEN_KEYS[] = {
    "access_token", "refresh_token", "expires_at", "expires_in", "obtained_at", NULL
};
static const char *const AU_EXPLICIT_POOL_SOURCES[] = {
    "device_code", "loopback_pkce", "hermes_pkce", "manual", NULL
};
static const char *const AU_IMPLICIT_ENV_VARS[] = { "CLAUDE_CODE_OAUTH_TOKEN", NULL };
static const char *const AU_VERTEX_PROVIDER_IDS[] = {
    "vertex", "google-vertex", "vertex-ai", "gcp-vertex", "vertexai", NULL
};
static const char *const AU_NO_AUTO_DETECT_PROVIDERS[] = { "lmstudio", NULL };
/* errno.EACCES=13, errno.EAGAIN=11, errno.EDEADLK=36 (probe-verified, py3.14 win) */
static const double AU_LOCK_CONTENTION_ERRNOS[] = { 13.0, 11.0, 36.0 };

/* _PROVIDER_ALIASES (hermes_cli/auth.py) */
static const char *const AU_PROVIDER_ALIASES[][2] = {
    {"glm", "zai"}, {"z-ai", "zai"}, {"z.ai", "zai"}, {"zhipu", "zai"},
    {"google", "gemini"}, {"google-gemini", "gemini"}, {"google-ai-studio", "gemini"},
    {"x-ai", "xai"}, {"x.ai", "xai"}, {"grok", "xai"},
    {"xai-oauth", "xai-oauth"}, {"x-ai-oauth", "xai-oauth"},
    {"grok-oauth", "xai-oauth"}, {"xai-grok-oauth", "xai-oauth"},
    {"kimi", "kimi-coding"}, {"kimi-for-coding", "kimi-coding"}, {"moonshot", "kimi-coding"},
    {"kimi-cn", "kimi-coding-cn"}, {"moonshot-cn", "kimi-coding-cn"},
    {"step", "stepfun"}, {"stepfun-coding-plan", "stepfun"},
    {"arcee-ai", "arcee"}, {"arceeai", "arcee"},
    {"gmi-cloud", "gmi"}, {"gmicloud", "gmi"},
    {"actual-computer", "actual"}, {"actualcomputer", "actual"}, {"aci", "actual"},
    {"minimax-china", "minimax-cn"}, {"minimax_cn", "minimax-cn"},
    {"minimax-portal", "minimax-oauth"}, {"minimax-global", "minimax-oauth"},
    {"minimax_oauth", "minimax-oauth"},
    {"alibaba_coding", "alibaba-coding-plan"}, {"alibaba-coding", "alibaba-coding-plan"},
    {"alibaba_coding_plan", "alibaba-coding-plan"},
    {"claude", "anthropic"}, {"claude-code", "anthropic"},
    {"aigateway", "ai-gateway"}, {"vercel", "ai-gateway"}, {"vercel-ai-gateway", "ai-gateway"},
    {"opencode", "opencode-zen"}, {"zen", "opencode-zen"},
    {"qwen-portal", "qwen-oauth"}, {"qwen-cli", "qwen-oauth"}, {"qwen-oauth", "qwen-oauth"},
    {"hf", "huggingface"}, {"hugging-face", "huggingface"}, {"huggingface-hub", "huggingface"},
    {"mimo", "xiaomi"}, {"xiaomi-mimo", "xiaomi"},
    {"tencent", "tencent-tokenhub"}, {"tokenhub", "tencent-tokenhub"},
    {"tencent-cloud", "tencent-tokenhub"}, {"tencentmaas", "tencent-tokenhub"},
    {"tokenplan", "tencent-tokenplan"}, {"tencent-lkeap", "tencent-tokenplan"},
    {"aws", "bedrock"}, {"aws-bedrock", "bedrock"}, {"amazon-bedrock", "bedrock"},
    {"amazon", "bedrock"},
    {"go", "opencode-go"}, {"opencode-go-sub", "opencode-go"},
    {"kilo", "kilocode"}, {"kilo-code", "kilocode"}, {"kilo-gateway", "kilocode"},
    {"lmstudio", "lmstudio"}, {"lm-studio", "lmstudio"}, {"lm_studio", "lmstudio"},
    {"chatgpt", "openai-codex"}, {"chatgpt-codex", "openai-codex"},
    {"local", "custom"},
    {"ollama", "custom"}, {"ollama_cloud", "ollama-cloud"},
    {"vllm", "custom"}, {"llamacpp", "custom"},
    {"llama.cpp", "custom"}, {"llama-cpp", "custom"},
    {NULL, NULL}
};

/* OAUTH_PROVIDER_FLOWS terminal_refresh_codes (frozensets, per provider) */
static const char *const AU_NOUS_DEAD_CODES[] = {
    "invalid_grant", "invalid_token", "refresh_token_reused",
    "nous_auth_missing", "nous_auth_missing_access_token", "nous_auth_missing_refresh_token", NULL
};
static const char *const AU_CODEX_DEAD_CODES[] = {
    "invalid_grant", "invalid_token", "refresh_token_reused",
    "codex_refresh_failed", "codex_auth_missing_refresh_token", NULL
};
static const char *const AU_XAI_DEAD_CODES[] = {
    "xai_refresh_failed", "xai_auth_missing_refresh_token", NULL
};
static const char *const AU_NO_DEAD_CODES[] = { NULL };
typedef struct { const char *pid; const char *const *codes; } au_oauth_flow_t;
static const au_oauth_flow_t AU_OAUTH_FLOWS[] = {
    {"nous", AU_NOUS_DEAD_CODES},
    {"openai-codex", AU_CODEX_DEAD_CODES},
    {"xai-oauth", AU_XAI_DEAD_CODES},
    {"qwen-oauth", AU_NO_DEAD_CODES},
    {"minimax-oauth", AU_NO_DEAD_CODES},
    {NULL, NULL}
};

/* ── shared seam helpers ── */

/* in-function `from mod import name`: fires import_<name>; result ignored
 * unless it raises (the oracle stub module raises through seam()). */
static json_t *au_import(const au_seams_t *s, const char *name) {
    char *ev = au_xsf("import_%s", name);
    json_t *r = au_call0(s, ev);
    free(ev);
    return r;
}

/* callable-marker key reader: {"__callable__":"scoped_key_env"|"os_getenv"} */
static json_t *au_keyenv_call(const au_seams_t *s, const json_t *keyenv, const char *name) {
    const char *kind = json_get_str(keyenv, "__callable__", "scoped_key_env");
    json_t *a = json_object();
    au_aset_s(a, "name", name);
    if (!strcmp(kind, "os_getenv")) {
        json_t *r = au_call(s, "os_getenv", a);
        json_free(a);
        if (au_is_raise(r)) return r;
        if (au_jtruth(r)) return r;
        json_free(r);
        return json_string("");  /* live `os.getenv(name) or ""` */
    }
    json_t *r = au_call(s, "scoped_key_env", a);
    json_free(a);
    return r;
}
static json_t *au_callable_marker(const char *kind) {
    json_t *m = json_object();
    json_set(m, "__callable__", json_string(kind));
    return m;
}

/* ══ registry / config leaves ══ */

/* PoP: au_api_key_provider @ hermes_cli/auth.py:_api_key_provider */
static json_t *au_api_key_provider(const char *id, const char *name,
                                   const char *inference_base_url,
                                   const json_t *api_key_env_vars,
                                   const char *base_url_env_var,
                                   const char *auth_type) {
    json_t *out = json_object();
    au_aset_s(out, "id", id);
    au_aset_s(out, "name", name);
    au_aset_s(out, "auth_type", auth_type);
    au_aset_s(out, "portal_base_url", "");
    au_aset_s(out, "inference_base_url", inference_base_url);
    au_aset_s(out, "client_id", "");
    au_aset_s(out, "scope", "");
    json_set(out, "extra", json_object());
    json_set(out, "api_key_env_vars",
             (api_key_env_vars && api_key_env_vars->type == JSON_ARRAY)
                 ? json_copy(api_key_env_vars) : json_array());
    au_aset_s(out, "base_url_env_var", base_url_env_var);
    return out;
}

/* PoP: au_empty_auth_store @ hermes_cli/auth.py:_empty_auth_store */
static json_t *au_empty_auth_store(void) {
    json_t *out = json_object();
    json_set(out, "version", json_number(AU_AUTH_STORE_VERSION));
    json_set(out, "providers", json_object());
    return out;
}

/* PoP: au_store_section @ hermes_cli/auth.py:_store_section */
static json_t *au_store_section(json_t *auth_store, const char *key) {
    const json_t *section = json_obj_get(auth_store, key);
    if (section && section->type == JSON_OBJECT) return json_copy(section);
    json_t *fresh = json_object();
    json_set(auth_store, key, json_copy(fresh));
    return fresh;
}

/* PoP: au_provider_state_in @ hermes_cli/auth.py:_provider_state_in */
static json_t *au_provider_state_in(const json_t *store, const char *provider_id) {
    const json_t *providers = au_jtruth(store) ? json_obj_get(store, "providers") : NULL;
    const json_t *state = (providers && providers->type == JSON_OBJECT)
                              ? json_obj_get(providers, provider_id) : NULL;
    if (state && state->type == JSON_OBJECT) return json_copy(state);
    return json_null();
}

/* PoP: au_slot_selects @ hermes_cli/auth.py:_slot_selects */
static json_t *au_slot_selects(const json_t *slot, const char *normalized) {
    if (!slot || slot->type != JSON_OBJECT) return json_bool(false);
    const json_t *p = json_obj_get(slot, "provider");
    if (!au_jtruth(p)) return json_bool(!strcmp("", normalized));  /* (x or "") */
    if (p->type != JSON_STRING) return au_attr_raise(au_typename(p), "strip");
    char *low = au_lower(p->str_val);
    char *st = au_strip(low);
    free(low);
    bool eq = !strcmp(st, normalized);
    free(st);
    return json_bool(eq);
}

/* PoP: au_model_level_key_env @ hermes_cli/auth.py:_model_level_key_env */
static json_t *au_model_level_key_env(const au_seams_t *s, const char *provider_id) {
    json_t *ir = au_import(s, "load_config");
    if (au_is_raise(ir)) {
        if (au_is_exception(ir)) { json_free(ir); return json_string(""); }
        return ir;
    }
    json_free(ir);
    json_t *lc = au_call0(s, "load_config");
    if (au_is_raise(lc)) {
        if (au_is_exception(lc)) { json_free(lc); return json_string(""); }
        return lc;
    }
    const json_t *model = NULL;
    if (au_jtruth(lc)) {
        if (lc->type != JSON_OBJECT) {
            json_t *e = au_attr_raise(au_typename(lc), "get");
            json_free(lc);
            if (au_is_exception(e)) { json_free(e); return json_string(""); }
            return e;
        }
        model = json_obj_get(lc, "model");
    }
    if (!model || model->type != JSON_OBJECT) { json_free(lc); return json_string(""); }
    const json_t *prov = json_obj_get(model, "provider");
    char *ps = au_pystr(au_jtruth(prov) ? prov : NULL);
    char *pl = au_lower(ps);
    char *pst = au_strip(pl);
    free(ps); free(pl);
    bool match = !strcmp(pst, provider_id);
    free(pst);
    if (!match) { json_free(lc); return json_string(""); }
    const json_t *ke = json_obj_get(model, "key_env");
    const json_t *ae = json_obj_get(model, "api_key_env");
    const json_t *eff = au_jtruth(ke) ? ke : (au_jtruth(ae) ? ae : NULL);
    /* str(key_env or api_key_env or "") — the chain ends in "", never None */
    char *ks = eff ? au_pystr(eff) : strdup("");
    char *kst = au_strip(ks);
    free(ks);
    json_free(lc);
    json_t *res = json_string(kst);
    free(kst);
    return res;
}

/* PoP: au_active_provider_is @ hermes_cli/auth.py:_active_provider_is */
static json_t *au_active_provider_is(const au_seams_t *s, const char *normalized) {
    json_t *st = au_call0(s, "load_auth_store");
    if (au_is_raise(st)) return st;
    if (!st || st->type != JSON_OBJECT) {
        json_t *e = au_attr_raise(au_typename(st), "get");
        json_free(st);
        return e;
    }
    const json_t *ap = json_obj_get(st, "active_provider");
    if (!au_jtruth(ap)) { json_free(st); return json_bool(false); }
    if (ap->type != JSON_STRING) {
        json_t *e = au_attr_raise(au_typename(ap), "strip");
        json_free(st);
        return e;
    }
    char *low = au_lower(ap->str_val);
    char *stt = au_strip(low);
    free(low);
    bool res = stt[0] != '\0' && !strcmp(stt, normalized);
    free(stt);
    json_free(st);
    return json_bool(res);
}

/* _moa_block_matches closure (pure; may raise via _slot_selects) */
static json_t *au_moa_block_matches(const json_t *block, const char *normalized) {
    if (!block || block->type != JSON_OBJECT) return json_bool(false);
    const json_t *rm = json_obj_get(block, "reference_models");
    const json_t *seq = au_jtruth(rm) ? rm : NULL;  /* `or []` */
    if (seq) {
        if (seq->type != JSON_ARRAY) return au_not_iterable_raise(seq);
        for (size_t i = 0; i < seq->c.count; i++) {
            json_t *t = au_slot_selects(seq->c.items[i], normalized);
            if (au_is_raise(t)) return t;
            bool hit = au_jtruth(t);
            json_free(t);
            if (hit) return json_bool(true);
        }
    }
    return au_slot_selects(json_obj_get(block, "aggregator"), normalized);
}

/* PoP: au_config_selects_provider @ hermes_cli/auth.py:_config_selects_provider */
static json_t *au_config_selects_provider(const au_seams_t *s, const char *normalized) {
    json_t *ir = au_import(s, "load_config");
    if (au_is_raise(ir)) return ir;
    json_free(ir);
    json_t *cfg = au_call0(s, "load_config");
    if (au_is_raise(cfg)) return cfg;
    if (!cfg || cfg->type != JSON_OBJECT) {
        json_t *e = au_attr_raise(au_typename(cfg), "get");
        json_free(cfg);
        return e;
    }
    json_t *t = au_slot_selects(json_obj_get(cfg, "model"), normalized);
    if (au_is_raise(t)) { json_free(cfg); return t; }
    if (au_jtruth(t)) { json_free(t); json_free(cfg); return json_bool(true); }
    json_free(t);
    const json_t *aux = json_obj_get(cfg, "auxiliary");
    if (aux && aux->type == JSON_OBJECT) {
        for (size_t i = 0; i < aux->c.count; i++) {
            t = au_slot_selects(aux->c.items[i], normalized);
            if (au_is_raise(t)) { json_free(cfg); return t; }
            bool hit = au_jtruth(t);
            json_free(t);
            if (hit) { json_free(cfg); return json_bool(true); }
        }
    }
    const json_t *moa = json_obj_get(cfg, "moa");
    if (!moa || moa->type != JSON_OBJECT) { json_free(cfg); return json_bool(false); }
    t = au_moa_block_matches(moa, normalized);
    if (au_is_raise(t)) { json_free(cfg); return t; }
    if (au_jtruth(t)) { json_free(t); json_free(cfg); return json_bool(true); }
    json_free(t);
    const json_t *presets = json_obj_get(moa, "presets");
    if (presets && presets->type == JSON_OBJECT) {
        for (size_t i = 0; i < presets->c.count; i++) {
            t = au_moa_block_matches(presets->c.items[i], normalized);
            if (au_is_raise(t)) { json_free(cfg); return t; }
            bool hit = au_jtruth(t);
            json_free(t);
            if (hit) { json_free(cfg); return json_bool(true); }
        }
    }
    json_free(cfg);
    return json_bool(false);
}

/* PoP: au_env_secret @ hermes_cli/auth.py:_env_secret */
static json_t *au_env_secret(const au_seams_t *s, const char *name) {
    json_t *ir = au_import(s, "get_env_value_prefer_dotenv");
    if (au_is_raise(ir)) return ir;
    json_free(ir);
    json_t *a = json_object();
    au_aset_s(a, "name", name);
    json_t *r = au_call(s, "get_env_value_prefer_dotenv", a);
    json_free(a);
    if (au_is_raise(r)) return r;
    json_t *v = au_jtruth(r) ? json_copy(r) : json_string("");  /* `or ""` */
    json_free(r);
    json_t *a2 = json_object();
    json_set(a2, "value", v);
    json_t *hs = au_call(s, "has_usable_secret", a2);
    json_free(a2);
    return hs;
}

/* PoP: au_explicit_env_credentials_present @ hermes_cli/auth.py:_explicit_env_credentials_present */
static json_t *au_explicit_env_credentials_present(const au_seams_t *s,
                                                   const json_t *registry,
                                                   const char *normalized) {
    const json_t *pc = json_obj_get(registry, normalized);
    json_t *owned = NULL;
    if (!pc) {
        json_t *ir = au_import(s, "get_provider");
        if (au_is_raise(ir)) return ir;
        json_free(ir);
        json_t *a = json_object();
        au_aset_s(a, "provider_id", normalized);
        json_t *gp = au_call(s, "get_provider", a);
        json_free(a);
        if (au_is_raise(gp)) return gp;
        if (!au_jtruth(gp)) { json_free(gp); return json_bool(false); }
        owned = gp;
        pc = gp;
    }
    const char *at = json_get_str(pc, "auth_type", "");
    json_t *res = NULL;
    if (!strcmp(at, "api_key")) {
        const json_t *vars = json_obj_get(pc, "api_key_env_vars");
        bool hit = false;
        if (vars && vars->type == JSON_ARRAY) {
            for (size_t i = 0; i < vars->c.count && !hit; i++) {
                const char *v = au_jstr(vars->c.items[i]);
                if (!v) continue;  /* fixture domain: env var names are str */
                if (au_in_names(v, AU_IMPLICIT_ENV_VARS)) continue;
                json_t *r = au_env_secret(s, v);
                if (au_is_raise(r)) { res = r; break; }
                hit = au_jtruth(r);
                json_free(r);
            }
        }
        if (!res) res = json_bool(hit);
    } else if (!strcmp(at, "aws_sdk")) {
        /* live: A or (B and C) — operand semantics */
        json_t *ra = au_env_secret(s, "AWS_BEARER_TOKEN_BEDROCK");
        if (au_is_raise(ra)) res = ra;
        else if (au_jtruth(ra)) res = ra;
        else {
            json_free(ra);
            json_t *rb = au_env_secret(s, "AWS_ACCESS_KEY_ID");
            if (au_is_raise(rb)) res = rb;
            else if (!au_jtruth(rb)) res = rb;
            else {
                json_free(rb);
                res = au_env_secret(s, "AWS_SECRET_ACCESS_KEY");
            }
        }
    } else {
        res = json_bool(false);
    }
    if (owned) json_free(owned);
    return res;
}

/* PoP: au_keyless_provider_has_explicit_config @ hermes_cli/auth.py:_keyless_provider_has_explicit_config */
static json_t *au_keyless_provider_has_explicit_config(const au_seams_t *s,
                                                       const char *normalized) {
    if (au_in_names(normalized, AU_VERTEX_PROVIDER_IDS)) {
        json_t *ir = au_import(s, "has_explicit_vertex_config");
        if (au_is_raise(ir)) return ir;
        json_free(ir);
        json_t *r = au_call0(s, "has_explicit_vertex_config");
        if (au_is_raise(r)) return r;
        bool t = au_jtruth(r);
        json_free(r);
        return json_bool(t);
    }
    if (!strcmp(normalized, "bedrock")) {
        json_t *ir = au_import(s, "load_config");
        if (au_is_raise(ir)) return ir;
        json_free(ir);
        json_t *lc = au_call0(s, "load_config");
        if (au_is_raise(lc)) return lc;
        if (!lc || lc->type != JSON_OBJECT) {
            json_t *e = au_attr_raise(au_typename(lc), "get");
            json_free(lc);
            return e;
        }
        const json_t *bc = json_obj_get(lc, "bedrock");
        if (!bc || bc->type != JSON_OBJECT) { json_free(lc); return json_bool(false); }
        const json_t *reg = json_obj_get(bc, "region");
        char *rs = au_pystr(au_jtruth(reg) ? reg : NULL);
        char *rst = au_strip(rs);
        free(rs);
        bool t = rst[0] != '\0';
        free(rst);
        json_free(lc);
        return json_bool(t);
    }
    return json_bool(false);
}

/* PoP: au_refuse_env_adoption_if_config_corrupt @ hermes_cli/auth.py:_refuse_env_adoption_if_config_corrupt */
static json_t *au_refuse_env_adoption_if_config_corrupt(const au_seams_t *s) {
    json_t *ir = au_import(s, "get_active_config_parse_failure");
    if (au_is_raise(ir)) {
        if (au_is_exception(ir)) { json_free(ir); return NULL; }
        return ir;
    }
    json_free(ir);
    json_t *err = au_call0(s, "get_active_config_parse_failure");
    if (au_is_raise(err)) {
        if (au_is_exception(err)) { json_free(err); return NULL; }
        return err;
    }
    if (!au_jtruth(err)) { json_free(err); return NULL; }
    json_t *path = au_call0(s, "get_config_path");
    if (au_is_raise(path)) {
        json_free(err);
        if (au_is_exception(path)) { json_free(path); return NULL; }
        return path;
    }
    char *es = au_pystr(err);
    char *ps = au_pystr(path);
    json_free(err);
    json_free(path);
    char *msg = au_xsf(
        "config.yaml at %s is corrupt (%s) \xe2\x80\x94 refusing to auto-select "
        "an inference provider from environment keys. Fix the YAML (a backup "
        "was saved next to it) or run hermes setup.",
        ps, es);
    free(es); free(ps);
    json_t *e = au_raise_spec("AuthError", msg);
    free(msg);
    return e;
}

/* PoP: au_plugin_aliases @ hermes_cli/auth.py:_plugin_aliases */
static json_t *au_plugin_aliases(const au_seams_t *s) {
    json_t *out = json_object();
    for (size_t i = 0; AU_PROVIDER_ALIASES[i][0]; i++)
        json_set(out, AU_PROVIDER_ALIASES[i][0], json_string(AU_PROVIDER_ALIASES[i][1]));
    json_t *ir = au_import(s, "list_providers");
    if (au_is_raise(ir)) {
        if (au_is_exception(ir)) { json_free(ir); return out; }
        json_free(out);
        return ir;
    }
    json_free(ir);
    json_t *lp = au_call0(s, "list_providers");
    if (au_is_raise(lp)) {
        if (au_is_exception(lp)) { json_free(lp); return out; }
        json_free(out);
        return lp;
    }
    if (!lp || lp->type != JSON_ARRAY) {
        /* live: `for _pp in _lp()` TypeError — caught by except Exception */
        json_free(lp);
        return out;
    }
    for (size_t i = 0; i < lp->c.count; i++) {
        const json_t *row = lp->c.items[i];
        if (!row || row->type != JSON_OBJECT) { json_free(lp); return out; }  /* SimpleNamespace(**d) TypeError */
        const json_t *aliases = json_obj_get(row, "aliases");
        const json_t *name = json_obj_get(row, "name");
        if (!aliases || aliases->type != JSON_ARRAY || !name) { json_free(lp); return out; }
        for (size_t j = 0; j < aliases->c.count; j++) {
            const char *al = au_jstr(aliases->c.items[j]);
            if (!al) { json_free(lp); return out; }  /* fixture domain: aliases are str */
            if (!json_has(out, al)) json_set(out, al, json_copy(name));
        }
    }
    json_free(lp);
    return out;
}

/* PoP: au_scoped_key_env_reader @ hermes_cli/auth.py:_scoped_key_env_reader */
static json_t *au_scoped_key_env_reader(const au_seams_t *s) {
    json_t *ir = au_import(s, "scoped_key_env");
    if (au_is_raise(ir)) {
        if (au_caught(ir, "ImportError")) {
            json_free(ir);
            return au_callable_marker("os_getenv");  /* fallback lambda */
        }
        return ir;  /* only ImportError is caught */
    }
    json_free(ir);
    return au_callable_marker("scoped_key_env");
}

/* PoP: au_openrouter_auto_detected @ hermes_cli/auth.py:_openrouter_auto_detected */
static json_t *au_openrouter_auto_detected(const au_seams_t *s, const json_t *keyenv) {
    json_t *k1 = au_keyenv_call(s, keyenv, "OPENROUTER_API_KEY");
    if (au_is_raise(k1)) return k1;
    json_t *a = json_object();
    json_set(a, "value", k1);
    json_t *h1 = au_call(s, "has_usable_secret", a);
    json_free(a);
    if (au_is_raise(h1)) return h1;
    if (au_jtruth(h1)) { json_free(h1); return json_bool(true); }
    json_free(h1);
    json_t *k2 = au_keyenv_call(s, keyenv, "OPENAI_API_KEY");
    if (au_is_raise(k2)) return k2;
    a = json_object();
    au_aset_j(a, "value", k2);
    json_t *h2 = au_call(s, "has_usable_secret", a);
    json_free(a);
    if (au_is_raise(h2)) { json_free(k2); return h2; }
    bool legacy = false;
    if (au_jtruth(h2)) {
        a = json_object();
        au_aset_j(a, "value", k2);
        json_t *lk = au_call(s, "looks_like_openrouter_key", a);
        json_free(a);
        if (au_is_raise(lk)) { json_free(h2); json_free(k2); return lk; }
        legacy = au_jtruth(lk);
        json_free(lk);
    }
    json_free(h2);
    json_free(k2);
    if (legacy) return json_bool(true);
    /* try: load_pool("openrouter").has_credentials() — except Exception: False */
    json_t *ir = au_import(s, "load_pool");
    if (au_is_raise(ir)) {
        if (au_is_exception(ir)) { json_free(ir); return json_bool(false); }
        return ir;
    }
    json_free(ir);
    a = json_object();
    au_aset_s(a, "provider_id", "openrouter");
    json_t *lp = au_call(s, "load_pool", a);
    json_free(a);
    if (au_is_raise(lp)) {
        if (au_is_exception(lp)) { json_free(lp); return json_bool(false); }
        return lp;
    }
    if (au_is_null(lp)) {
        /* None.has_credentials -> AttributeError, swallowed live */
        json_free(lp);
        return json_bool(false);
    }
    json_free(lp);
    json_t *hc = au_call0(s, "has_credentials");
    if (au_is_raise(hc)) {
        if (au_is_exception(hc)) { json_free(hc); return json_bool(false); }
        return hc;
    }
    bool t = au_jtruth(hc);
    json_free(hc);
    return json_bool(t);
}

/* PoP: au_logged_in_oauth_active_provider @ hermes_cli/auth.py:_logged_in_oauth_active_provider */
static json_t *au_logged_in_oauth_active_provider(const au_seams_t *s,
                                                  const json_t *registry,
                                                  bool skip_free_tier) {
    /* whole body is under try/except Exception -> None; BaseException passes */
    json_t *st = au_call0(s, "load_auth_store");
    if (au_is_raise(st)) {
        if (au_is_exception(st)) { json_free(st); return NULL; }
        return st;
    }
    if (!st || st->type != JSON_OBJECT) {
        json_free(st);
        return NULL;  /* AttributeError on .get, swallowed */
    }
    const json_t *maybe = json_obj_get(st, "active_provider");
    const char *ms = au_jstr(maybe);
    if (ms && !strcmp(ms, "nous")) {
        json_t *ir1 = au_import(s, "free_inference_active");
        if (au_is_raise(ir1)) {
            json_free(st);
            if (au_is_exception(ir1)) { json_free(ir1); return NULL; }
            return ir1;
        }
        json_free(ir1);
        json_t *ir2 = au_import(s, "has_guest");
        if (au_is_raise(ir2)) {
            json_free(st);
            if (au_is_exception(ir2)) { json_free(ir2); return NULL; }
            return ir2;
        }
        json_free(ir2);
        json_t *hg = au_call0(s, "has_guest");
        if (au_is_raise(hg)) {
            json_free(st);
            if (au_is_exception(hg)) { json_free(hg); return NULL; }
            return hg;
        }
        bool guest = au_jtruth(hg);
        json_free(hg);
        if (guest) {
            if (skip_free_tier) { json_free(st); return NULL; }
            json_t *fi = au_call0(s, "free_inference_active");
            if (au_is_raise(fi)) {
                json_free(st);
                if (au_is_exception(fi)) { json_free(fi); return NULL; }
                return fi;
            }
            bool active = au_jtruth(fi);
            json_free(fi);
            if (!active) { json_free(st); return NULL; }
        }
    }
    json_t *res = NULL;
    if (au_jtruth(maybe) && ms && json_has(registry, ms)) {
        json_t *a = json_object();
        au_aset_j(a, "provider_id", maybe);
        json_t *gs = au_call(s, "get_auth_status", a);
        json_free(a);
        if (au_is_raise(gs)) {
            json_free(st);
            if (au_is_exception(gs)) { json_free(gs); return NULL; }
            return gs;
        }
        if (gs && gs->type == JSON_OBJECT) {
            const json_t *li = json_obj_get(gs, "logged_in");
            if (au_jtruth(li)) res = json_copy(maybe);
        }
        /* non-dict status: AttributeError on .get, swallowed */
        json_free(gs);
    }
    json_free(st);
    return res;
}

/* PoP: au_config_model_provider @ hermes_cli/auth.py:_config_model_provider */
static json_t *au_config_model_provider(const au_seams_t *s, const json_t *registry) {
    json_t *fail = json_array();
    json_append(fail, json_null());
    json_append(fail, json_null());
    json_t *ir = au_import(s, "load_config");
    if (au_is_raise(ir)) {
        if (au_is_exception(ir)) { json_free(ir); return fail; }
        json_free(fail);
        return ir;
    }
    json_free(ir);
    json_t *lc = au_call0(s, "load_config");
    if (au_is_raise(lc)) {
        if (au_is_exception(lc)) { json_free(lc); return fail; }
        json_free(fail);
        return lc;
    }
    json_t *empty = NULL;
    const json_t *cfg = lc;
    if (!au_jtruth(lc)) { empty = json_object(); cfg = empty; }  /* `or {}` */
    const json_t *model = NULL;
    if (cfg->type != JSON_OBJECT) {
        /* AttributeError on .get, swallowed -> (None, None) */
        json_free(lc);
        if (empty) json_free(empty);
        return fail;
    }
    model = json_obj_get(cfg, "model");
    const json_t *prov = (model && model->type == JSON_OBJECT)
                             ? json_obj_get(model, "provider") : NULL;
    char *pstr;
    if (prov && prov->type == JSON_STRING) {
        char *low = au_lower(prov->str_val);
        pstr = au_strip(low);
        free(low);
    } else {
        pstr = strdup("");
    }
    json_t *aliases = au_plugin_aliases(s);
    if (au_is_raise(aliases)) {
        free(pstr);
        json_free(lc);
        if (empty) json_free(empty);
        if (au_is_exception(aliases)) { json_free(aliases); return fail; }
        json_free(fail);
        return aliases;
    }
    const json_t *ap = json_obj_get(aliases, pstr);
    const char *eff = (ap && ap->type == JSON_STRING) ? ap->str_val : pstr;
    json_t *res = NULL;
    if (!strcmp(eff, "custom") || au_startswith(eff, "custom:")) {
        res = json_array();
        json_append(res, model ? json_copy(model) : json_null());
        json_append(res, json_string("custom"));
    } else if (!strcmp(eff, "openrouter") || json_has(registry, eff)) {
        res = json_array();
        json_append(res, model ? json_copy(model) : json_null());
        json_append(res, json_string(eff));
    } else {
        bool done = false;
        if (eff[0]) {
            json_t *ir2 = au_import(s, "has_named_custom_provider");
            if (au_is_raise(ir2)) {
                if (au_is_exception(ir2)) { json_free(ir2); goto swallow; }
                res = ir2; goto cleanup;
            }
            json_free(ir2);
            json_t *a = json_object();
            au_aset_s(a, "provider", eff);
            json_t *hn = au_call(s, "has_named_custom_provider", a);
            json_free(a);
            if (au_is_raise(hn)) {
                if (au_is_exception(hn)) { json_free(hn); goto swallow; }
                res = hn; goto cleanup;
            }
            if (au_jtruth(hn)) {
                json_free(hn);
                res = json_array();
                json_append(res, model ? json_copy(model) : json_null());
                json_append(res, json_string("custom"));
                done = true;
            } else {
                json_free(hn);
            }
        }
        if (!done) {
            char *burl = strdup("");
            if (model && model->type == JSON_OBJECT) {
                const json_t *bu = json_obj_get(model, "base_url");
                /* str(base_url or "") — falsy collapses to "", never "None" */
                char *bs = au_jtruth(bu) ? au_pystr(bu) : strdup("");
                free(burl);
                burl = au_strip(bs);
                free(bs);
            }
            if (burl[0]) {
                json_t *ir3 = au_import(s, "config_base_url_trustworthy_for_bare_custom");
                if (au_is_raise(ir3)) {
                    free(burl);
                    if (au_is_exception(ir3)) { json_free(ir3); goto swallow; }
                    res = ir3; goto cleanup;
                }
                json_free(ir3);
                json_t *a = json_object();
                au_aset_s(a, "base_url", burl);
                au_aset_s(a, "provider", eff);
                json_t *tr = au_call(s, "config_base_url_trustworthy_for_bare_custom", a);
                json_free(a);
                if (au_is_raise(tr)) {
                    free(burl);
                    if (au_is_exception(tr)) { json_free(tr); goto swallow; }
                    res = tr; goto cleanup;
                }
                if (au_jtruth(tr)) {
                    json_free(tr);
                    res = json_array();
                    json_append(res, model ? json_copy(model) : json_null());
                    json_append(res, json_string("custom"));
                } else {
                    json_free(tr);
                }
            }
            free(burl);
        }
    }
out:
    if (!res) {
        res = json_array();
        json_append(res, model ? json_copy(model) : json_null());
        json_append(res, json_null());
    }
cleanup:
    free(pstr);
    json_free(aliases);
    json_free(lc);
    if (empty) json_free(empty);
    json_free(fail);
    return res;
swallow:
    /* live `except Exception: return None, None` — NOT (model_cfg, None) */
    free(pstr);
    json_free(aliases);
    json_free(lc);
    if (empty) json_free(empty);
    return fail;
}

/* PoP: au_credential_profile @ hermes_cli/auth.py:_credential_profile */
static json_t *au_credential_profile(const au_seams_t *s, const char *provider_id) {
    json_t *ir = au_import(s, "get_provider_profile");
    if (au_is_raise(ir)) return ir;
    json_free(ir);
    json_t *a = json_object();
    au_aset_s(a, "provider_id", provider_id);
    json_t *r = au_call(s, "get_provider_profile", a);
    json_free(a);
    return r;
}

/* PoP: au_env_key_auto_detected @ hermes_cli/auth.py:_env_key_auto_detected */
static json_t *au_env_key_auto_detected(const au_seams_t *s, const json_t *keyenv,
                                        const char *oauth_active) {
    json_t *ir = au_import(s, "list_providers");
    if (au_is_raise(ir)) return ir;
    json_free(ir);
    json_t *lp = au_call0(s, "list_providers");
    if (au_is_raise(lp)) return lp;
    if (!lp || lp->type != JSON_ARRAY) {
        json_t *e = au_not_iterable_raise(lp);
        json_free(lp);
        return e;
    }
    /* ambient = {p.name for p in list_providers() if p.ambient_credentials} */
    json_t *ambient = json_array();
    for (size_t i = 0; i < lp->c.count; i++) {
        const json_t *row = lp->c.items[i];
        if (!row || row->type != JSON_OBJECT) {
            json_free(ambient); json_free(lp);
            return au_raise_spec("TypeError", "argument after ** must be a mapping, not str");
        }
        const json_t *ac = json_obj_get(row, "ambient_credentials");
        if (!au_jtruth(ac)) continue;
        const json_t *nm = json_obj_get(row, "name");
        if (!nm || nm->type != JSON_STRING) {
            json_free(ambient); json_free(lp);
            return au_attr_raise("types.SimpleNamespace", "name");
        }
        json_append(ambient, json_string(nm->str_val));
    }
    json_free(lp);
    json_t *rows = au_call0(s, "canonical_registry_rows");
    if (au_is_raise(rows)) { json_free(ambient); return rows; }
    if (!rows || rows->type != JSON_ARRAY) {
        json_t *e = au_not_iterable_raise(rows);
        json_free(rows); json_free(ambient);
        return e;
    }
    json_t *res = NULL;
    for (size_t i = 0; i < rows->c.count && !res; i++) {
        const json_t *pair = rows->c.items[i];
        if (!pair || pair->type != JSON_ARRAY || pair->c.count != 2) continue;
        const char *pid = au_jstr(json_get(pair, 0));
        const json_t *pc = json_get(pair, 1);
        if (!pid || !pc) continue;  /* fixture domain: [str, dict] pairs */
        const char *at = json_get_str(pc, "auth_type", "");
        if (strcmp(at, "api_key") != 0) continue;
        if (au_in_names(pid, AU_NO_AUTO_DETECT_PROVIDERS)) continue;
        bool in_ambient = false;
        for (size_t j = 0; j < ambient->c.count; j++)
            if (!strcmp(pid, au_jstr(ambient->c.items[j]))) { in_ambient = true; break; }
        if (in_ambient) continue;
        const json_t *vars = json_obj_get(pc, "api_key_env_vars");
        if (!vars || vars->type != JSON_ARRAY) continue;
        for (size_t j = 0; j < vars->c.count && !res; j++) {
            const char *v = au_jstr(vars->c.items[j]);
            if (!v) continue;
            json_t *k = au_keyenv_call(s, keyenv, v);
            if (au_is_raise(k)) { res = k; break; }
            json_t *a = json_object();
            json_set(a, "value", k);
            json_t *hs = au_call(s, "has_usable_secret", a);
            json_free(a);
            if (au_is_raise(hs)) { res = hs; break; }
            if (au_jtruth(hs)) res = json_string(pid);  /* logger.warning silent */
            json_free(hs);
        }
    }
    json_free(rows);
    json_free(ambient);
    return res;
}

/* ══ secret / env gates ══ */

/* PoP: au_pool_entry_is_explicit @ hermes_cli/auth.py:_pool_entry_is_explicit */
static json_t *au_pool_entry_is_explicit(const au_seams_t *s, const json_t *entry) {
    if (!entry || entry->type != JSON_OBJECT) return json_bool(false);
    const json_t *src = json_obj_get(entry, "source");
    char *ss = au_pystr(au_jtruth(src) ? src : NULL);
    char *sl = au_lower(ss);
    char *st = au_strip(sl);
    free(ss); free(sl);
    if (au_startswith(st, "env:")) {
        free(st);
        const char *raw = json_get_str(entry, "source", "");
        const char *colon = strchr(raw, ':');
        char *env = au_strip(colon ? colon + 1 : "");
        if (!env[0]) { free(env); return json_bool(false); }
        json_t *r = au_env_secret(s, env);
        free(env);
        if (au_is_raise(r)) return r;
        bool t = au_jtruth(r);
        json_free(r);
        return json_bool(t);
    }
    bool has = st[0] != '\0';
    bool ok = has && (au_in_names(st, AU_EXPLICIT_POOL_SOURCES) || au_startswith(st, "manual:"));
    free(st);
    return json_bool(ok);
}

/* PoP: au_explicit_pool_entry_present @ hermes_cli/auth.py:_explicit_pool_entry_present */
static json_t *au_explicit_pool_entry_present(const au_seams_t *s, const char *normalized) {
    json_t *a = json_object();
    au_aset_s(a, "provider_id", normalized);
    json_t *r = au_call(s, "read_credential_pool", a);
    json_free(a);
    if (au_is_raise(r)) return r;
    if (!r || r->type != JSON_ARRAY) {
        json_t *e = au_not_iterable_raise(r);
        json_free(r);
        return e;
    }
    for (size_t i = 0; i < r->c.count; i++) {
        json_t *t = au_pool_entry_is_explicit(s, r->c.items[i]);
        if (au_is_raise(t)) { json_free(r); return t; }
        bool hit = au_jtruth(t);
        json_free(t);
        if (hit) { json_free(r); return json_bool(true); }
    }
    json_free(r);
    return json_bool(false);
}

/* ══ lock cluster ══ */

/* PoP: au_is_lock_contention @ hermes_cli/auth.py:_is_lock_contention */
static json_t *au_is_lock_contention(const json_t *exc) {
    const char *kind = json_get_str(exc, "kind", "");
    if (!strcmp(kind, "BlockingIOError")) return json_bool(true);
    const json_t *e = json_obj_get(exc, "errno");
    if (!e) return au_attr_raise(kind[0] ? kind : "object", "errno");
    if (e->type != JSON_NUMBER) return json_bool(false);
    for (size_t i = 0; i < sizeof(AU_LOCK_CONTENTION_ERRNOS) / sizeof(double); i++)
        if (e->num_val == AU_LOCK_CONTENTION_ERRNOS[i]) return json_bool(true);
    return json_bool(false);
}

/* PoP: au_lock_holder_hint @ hermes_cli/auth.py:_lock_holder_hint */
static json_t *au_lock_holder_hint(const au_seams_t *s, const char *path,
                                   const char *os_name) {
    /* live materializes Path(lock_path) — normalize at the call boundary */
    char *np = au_path_norm(path);
    json_t *r = au_lock_holder_hint_n(s, np, os_name);
    free(np);
    return r;
}
static json_t *au_lock_holder_hint_n(const au_seams_t *s, const char *path,
                                     const char *os_name) {
    json_t *a = json_object();
    au_aset_s(a, "path", path);
    au_aset_s(a, "encoding", "utf-8-sig");
    au_aset_s(a, "errors", "replace");
    json_t *r = au_call(s, "path_read_text", a);
    json_free(a);
    if (au_is_raise(r)) {
        if (au_caught(r, "OSError") || au_caught(r, "ValueError") || au_caught(r, "IndexError")) {
            json_free(r);
            return json_string("");
        }
        return r;
    }
    if (!r || r->type != JSON_STRING) {
        json_t *e = au_attr_raise(au_typename(r), "split");
        json_free(r);
        return e;  /* AttributeError is NOT in the caught tuple */
    }
    json_t *tokens = au_split_ws(r->str_val);
    json_free(r);
    if (tokens->c.count == 0) { json_free(tokens); return json_string(""); }  /* IndexError */
    double pid;
    if (!au_int_token(au_jstr(tokens->c.items[0]), &pid)) {
        json_free(tokens);
        return json_string("");  /* ValueError */
    }
    json_free(tokens);
    if (pid <= 0) return json_string("");
    json_t *gp = au_call0(s, "os_getpid");
    if (au_is_raise(gp)) return gp;
    double mypid = (gp && gp->type == JSON_NUMBER) ? gp->num_val : 0;
    json_free(gp);
    if (pid == mypid) return json_string("");
    if (!strcmp(os_name, "posix")) {
        a = json_object();
        au_aset_n(a, "pid", pid);
        au_aset_n(a, "sig", 0);
        json_t *k = au_call(s, "os_kill", a);
        json_free(a);
        if (au_is_raise(k)) {
            if (au_caught(k, "ProcessLookupError")) { json_free(k); return json_string(""); }
            if (au_caught(k, "OSError")) { json_free(k); /* live holder */ }
            else return k;
        } else {
            json_free(k);
        }
    }
    char *msg = au_xsf("another hermes process (pid %.0f) probably still holds it", pid);
    json_t *res = json_string(msg);
    free(msg);
    return res;
}

/* PoP: au_kernel_lock @ hermes_cli/auth.py:_kernel_lock */
static json_t *au_kernel_lock(const au_seams_t *s, const char *backend, bool acquire) {
    if (!strcmp(backend, "fcntl")) {
        json_t *fd = au_call0(s, "file_fileno");
        if (au_is_raise(fd)) return fd;
        json_t *a = json_object();
        au_aset_j(a, "fd", fd);
        /* LOCK_EX|LOCK_NB = 2|4 = 6; LOCK_UN = 8 */
        au_aset_n(a, "flags", acquire ? 6 : 8);
        json_free(fd);
        json_t *r = au_call(s, "fcntl_flock", a);
        json_free(a);
        if (au_is_raise(r)) return r;
        json_free(r);
        return NULL;
    }
    json_t *a = json_object();
    au_aset_n(a, "offset", 0);
    json_t *sk = au_call(s, "file_seek", a);
    json_free(a);
    if (au_is_raise(sk)) return sk;
    json_free(sk);
    if (!strcmp(backend, "none")) {
        /* msvcrt.locking attribute resolves BEFORE lock_file.fileno() evaluates */
        return au_attr_raise("NoneType", "locking");
    }
    json_t *fd = au_call0(s, "file_fileno");
    if (au_is_raise(fd)) return fd;
    a = json_object();
    au_aset_j(a, "fd", fd);
    /* LK_NBLCK = 2; LK_UNLCK = 0 */
    au_aset_n(a, "mode", acquire ? 2 : 0);
    au_aset_n(a, "nbytes", 1);
    json_free(fd);
    json_t *r = au_call(s, "msvcrt_locking", a);
    json_free(a);
    if (au_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: au_stamp_lock_holder_pid @ hermes_cli/auth.py:_stamp_lock_holder_pid */
static json_t *au_stamp_lock_holder_pid(const au_seams_t *s) {
    json_t *a = json_object();
    au_aset_n(a, "size", 0);
    json_t *t = au_call(s, "file_truncate", a);
    json_free(a);
    if (au_is_raise(t)) {
        if (au_caught(t, "OSError")) { json_free(t); return NULL; }
        return t;
    }
    json_free(t);
    json_t *gp = au_call0(s, "os_getpid");
    if (au_is_raise(gp)) {
        if (au_caught(gp, "OSError")) { json_free(gp); return NULL; }
        return gp;
    }
    double pid = (gp && gp->type == JSON_NUMBER) ? gp->num_val : 0;
    json_free(gp);
    char *data = au_xsf("%.0f\n", pid);
    a = json_object();
    au_aset_s(a, "data", data);
    free(data);
    json_t *w = au_call(s, "file_write", a);
    json_free(a);
    if (au_is_raise(w)) {
        if (au_caught(w, "OSError")) { json_free(w); return NULL; }
        return w;
    }
    json_free(w);
    json_t *f = au_call0(s, "file_flush");
    if (au_is_raise(f)) {
        if (au_caught(f, "OSError")) { json_free(f); return NULL; }
        return f;
    }
    json_free(f);
    return NULL;
}

/* PoP: au_clear_stamped_lock_holder_pid @ hermes_cli/auth.py:_clear_stamped_lock_holder_pid */
static json_t *au_clear_stamped_lock_holder_pid(const au_seams_t *s, const char *backend) {
    json_t *a = json_object();
    au_aset_n(a, "size", 0);
    json_t *t = au_call(s, "file_truncate", a);
    json_free(a);
    if (au_is_raise(t)) {
        if (au_caught(t, "OSError")) { json_free(t); return NULL; }
        return t;
    }
    json_free(t);
    if (!strcmp(backend, "msvcrt")) {
        a = json_object();
        au_aset_s(a, "data", " ");
        json_t *w = au_call(s, "file_write", a);
        json_free(a);
        if (au_is_raise(w)) {
            if (au_caught(w, "OSError")) { json_free(w); return NULL; }
            return w;
        }
        json_free(w);
    }
    json_t *f = au_call0(s, "file_flush");
    if (au_is_raise(f)) {
        if (au_caught(f, "OSError")) { json_free(f); return NULL; }
        return f;
    }
    json_free(f);
    return NULL;
}

/* ══ token / TTL markers ══ */

/* PoP: au_tls_state_from_verify @ hermes_cli/auth.py:_tls_state_from_verify */
static json_t *au_tls_state_from_verify(const json_t *verify) {
    json_t *out = json_object();
    /* `verify is False` — identity: JSON_BOOL false only (0/1/null excluded) */
    json_set(out, "insecure",
             json_bool(verify && verify->type == JSON_BOOL && !verify->bool_val));
    json_set(out, "ca_bundle",
             (verify && verify->type == JSON_STRING) ? json_copy(verify) : json_null());
    return out;
}

/* PoP: au_utc_now_z @ hermes_cli/auth.py:_utc_now_z */
static json_t *au_utc_now_z(const au_seams_t *s) {
    json_t *r = au_call0(s, "datetime_now");
    if (au_is_raise(r)) return r;
    if (!r || r->type != JSON_STRING) {
        json_t *e = au_attr_raise(au_typename(r), "replace");
        json_free(r);
        return e;
    }
    char *z = au_replace_all(r->str_val, "+00:00", "Z");
    json_free(r);
    json_t *res = json_string(z);
    free(z);
    return res;
}

/* PoP: au_last_auth_error_marker @ hermes_cli/auth.py:_last_auth_error_marker */
static json_t *au_last_auth_error_marker(const au_seams_t *s, const char *provider,
                                         const json_t *error, const char *reason,
                                         const json_t *default_code) {
    json_t *out = json_object();
    au_aset_s(out, "provider", provider);
    au_aset_s(out, "message", json_get_str(error, "message", ""));  /* str(error) */
    au_aset_s(out, "reason", reason);
    json_set(out, "relogin_required", json_bool(true));
    const json_t *ecode = json_obj_get(error, "code");
    json_t *code;
    if (au_is_null(default_code)) {
        code = ecode ? json_copy(ecode) : json_null();
    } else {
        code = au_jtruth(ecode) ? json_copy(ecode) : json_copy(default_code);
    }
    json_set(out, "code", code);
    json_t *at = au_call0(s, "datetime_now");
    if (au_is_raise(at)) { json_free(out); return at; }
    json_set(out, "at", at ? at : json_null());
    return out;
}

/* PoP: au_quarantine_flat_oauth_state @ hermes_cli/auth.py:_quarantine_flat_oauth_state */
static json_t *au_quarantine_flat_oauth_state(const au_seams_t *s, json_t *state,
                                              const char *provider, const json_t *error) {
    for (size_t i = 0; AU_FLAT_OAUTH_TOKEN_KEYS[i]; i++)
        json_obj_del(state, AU_FLAT_OAUTH_TOKEN_KEYS[i]);
    json_t *dc = json_string("refresh_failed");
    json_t *marker = au_last_auth_error_marker(s, provider, error,
                                               "runtime_refresh_failure", dc);
    json_free(dc);
    if (au_is_raise(marker)) return marker;  /* pops already applied */
    json_set(state, "last_auth_error", marker);
    return NULL;
}

/* PoP: au_is_terminal_refresh_error @ hermes_cli/auth.py:_is_terminal_refresh_error */
static json_t *au_is_terminal_refresh_error(const json_t *exc, const char *provider) {
    const au_oauth_flow_t *flow = NULL;
    for (size_t i = 0; AU_OAUTH_FLOWS[i].pid; i++)
        if (!strcmp(provider, AU_OAUTH_FLOWS[i].pid)) { flow = &AU_OAUTH_FLOWS[i]; break; }
    if (!flow) {
        char *rp = au_repr_string(provider);
        json_t *e = au_raise_spec("KeyError", rp);  /* str(KeyError(k)) == repr(k) */
        free(rp);
        return e;
    }
    const char *kind = json_get_str(exc, "kind", "");
    if (strcmp(kind, "AuthError") != 0) return json_bool(false);
    const char *ep = json_get_str(exc, "provider", "");
    if (strcmp(ep, flow->pid) != 0) return json_bool(false);
    const json_t *code = json_obj_get(exc, "code");
    const char *cs = au_jstr(code);
    if (!cs || !au_in_names(cs, flow->codes)) return json_bool(false);
    return json_bool(au_jtruth(json_obj_get(exc, "relogin_required")));
}

/* PoP: au_codex_pool_rate_limited_status @ hermes_cli/auth.py:_codex_pool_rate_limited_status */
static json_t *au_codex_pool_rate_limited_status(const au_seams_t *s) {
    json_t *rl = au_call0(s, "codex_pool_rate_limit_status");
    if (au_is_raise(rl)) return rl;
    if (!au_jtruth(rl)) { json_free(rl); return NULL; }
    json_t *out = json_object();
    json_set(out, "logged_in", json_bool(true));
    json_t *ap = au_call0(s, "auth_file_path");
    if (au_is_raise(ap)) { json_free(out); json_free(rl); return ap; }
    char *ps = au_pystr(ap);
    json_free(ap);
    json_set(out, "auth_store", json_string(ps));
    free(ps);
    au_aset_j(out, "last_refresh", json_obj_get(rl, "last_refresh"));
    au_aset_s(out, "auth_mode", "chatgpt");
    const json_t *label = json_obj_get(rl, "label");
    /* f"pool:{label or 'unknown'}" — falsy collapses to 'unknown' BEFORE str() */
    char *ls = au_jtruth(label) ? au_pystr(label) : strdup("unknown");
    char *src = au_xsf("pool:%s", ls);
    free(ls);
    json_set(out, "source", json_string(src));
    free(src);
    json_set(out, "rate_limited", json_bool(true));
    au_aset_s(out, "error_code", AU_CODEX_RATE_LIMITED_CODE);
    const json_t *msg = json_obj_get(rl, "message");
    json_set(out, "error", au_jtruth(msg) ? json_copy(msg)
        : json_string("Codex provider quota exhausted; retry after the usage limit resets."));
    au_aset_j(out, "reset_at", json_obj_get(rl, "reset_at"));
    json_free(rl);
    return out;
}

/* ══ persistence / status ══ */

/* PoP: au_resolved_key @ hermes_cli/auth.py:_resolved_key */
static json_t *au_resolved_key(const au_seams_t *s, const char *path) {
    char *np = au_path_norm(path);
    json_t *r = au_resolved_key_n(s, np);
    free(np);
    return r;
}
static json_t *au_resolved_key_n(const au_seams_t *s, const char *path) {
    json_t *a = json_object();
    au_aset_s(a, "path", path);
    au_aset_b(a, "strict", false);
    json_t *r = au_call(s, "path_resolve", a);
    json_free(a);
    if (au_is_raise(r)) {
        if (au_is_exception(r)) { json_free(r); return json_string(path); }
        return r;
    }
    char *rs = au_pystr(r);
    json_free(r);
    json_t *res = json_string(rs);
    free(rs);
    return res;
}

/* PoP: au_save_private_json @ hermes_cli/auth.py:_save_private_json */
static json_t *au_save_private_json(const au_seams_t *s, const char *target,
                                    const json_t *data, bool fsync_dir,
                                    const json_t *dump_kwargs) {
    char *nt = au_path_norm(target);
    json_t *r = au_save_private_json_n(s, nt, data, fsync_dir, dump_kwargs);
    free(nt);
    return r;
}
static json_t *au_save_private_json_n(const au_seams_t *s, const char *target,
                                      const json_t *data, bool fsync_dir,
                                      const json_t *dump_kwargs) {
    json_t *ir = au_import(s, "mkdir_under_hermes_home");
    if (au_is_raise(ir)) return ir;
    json_free(ir);
    char *parent = au_path_parent(target);
    json_t *a = json_object();
    au_aset_s(a, "path", parent);
    free(parent);
    json_t *r = au_call(s, "mkdir_under_hermes_home", a);
    json_free(a);
    if (au_is_raise(r)) return r;
    json_free(r);
    a = json_object();
    au_aset_s(a, "path", target);
    r = au_call(s, "secure_parent_dir", a);
    json_free(a);
    if (au_is_raise(r)) return r;
    json_free(r);
    a = json_object();
    au_aset_s(a, "path", target);
    au_aset_j(a, "data", data);
    au_aset_n(a, "mode", 384);  /* 0o600 */
    au_aset_b(a, "fsync_dir", fsync_dir);
    if (dump_kwargs && dump_kwargs->type == JSON_OBJECT) {
        for (size_t i = 0; i < dump_kwargs->c.count; i++)
            json_set(a, dump_kwargs->c.keys[i], json_copy(dump_kwargs->c.items[i]));
    }
    r = au_call(s, "atomic_json_write", a);
    json_free(a);
    if (au_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: au_save_active_provider_state @ hermes_cli/auth.py:_save_active_provider_state */
static json_t *au_save_active_provider_state(const au_seams_t *s, const char *provider_id,
                                             const json_t *state) {
    /* CM factory: _auth_store_lock() (default timeout AUTH_LOCK_TIMEOUT_SECONDS) */
    json_t *a = json_object();
    au_aset_n(a, "timeout_seconds", AU_AUTH_LOCK_TIMEOUT_SECONDS);
    json_t *f = au_call(s, "auth_store_lock", a);
    json_free(a);
    if (au_is_raise(f)) return f;
    json_free(f);
    json_t *en = au_call0(s, "auth_store_lock_enter");
    if (au_is_raise(en)) return en;  /* enter failed: no body, no exit */
    json_free(en);
    /* body: capture value/raise, then run __exit__ (exit raise overrides) */
    json_t *body_ret = NULL;
    json_t *body_raise = NULL;
    json_t *st = au_call0(s, "load_auth_store");
    if (au_is_raise(st)) {
        body_raise = st;
    } else {
        a = json_object();
        au_aset_j(a, "auth_store", st);
        au_aset_s(a, "provider_id", provider_id);
        au_aset_j(a, "state", state);
        json_t *spr = au_call(s, "save_provider_state", a);
        json_free(a);
        if (au_is_raise(spr)) {
            body_raise = spr;
        } else {
            json_free(spr);
            a = json_object();
            au_aset_j(a, "auth_store", st);
            json_t *sar = au_call(s, "save_auth_store", a);
            json_free(a);
            if (au_is_raise(sar)) body_raise = sar;
            else body_ret = sar;
        }
        json_free(st);
    }
    json_t *ex = au_call0(s, "auth_store_lock_exit");
    if (au_is_raise(ex)) {
        if (body_ret) json_free(body_ret);
        if (body_raise) json_free(body_raise);
        return ex;
    }
    json_free(ex);
    if (body_raise) {
        if (body_ret) json_free(body_ret);
        return body_raise;
    }
    return body_ret ? body_ret : json_null();
}

/* PoP: au_suppressed_credential_sources @ hermes_cli/auth.py:suppressed_credential_sources */
static json_t *au_suppressed_credential_sources(const au_seams_t *s, const char *provider_id) {
    json_t *st = au_call0(s, "load_auth_store");
    if (au_is_raise(st)) {
        if (au_caught(st, "OSError")) { json_free(st); return json_array(); }
        return st;  /* only OSError is caught */
    }
    if (!st || st->type != JSON_OBJECT) {
        json_t *e = au_attr_raise(au_typename(st), "get");
        json_free(st);
        return e;  /* AttributeError propagates (not OSError) */
    }
    const json_t *section = json_obj_get(st, "suppressed_sources");
    const json_t *listed = (section && section->type == JSON_OBJECT)
                               ? json_obj_get(section, provider_id) : NULL;
    json_free(st);
    json_t *strs = json_array();
    if (listed && listed->type == JSON_ARRAY) {
        for (size_t i = 0; i < listed->c.count; i++) {
            char *v = au_pystr(listed->c.items[i]);
            json_append(strs, json_string(v));
            free(v);
        }
    } else if (listed && listed->type == JSON_OBJECT) {
        for (size_t i = 0; i < listed->c.count; i++)
            json_append(strs, json_string(listed->c.keys[i]));
    }
    /* frozenset -> sorted list (set-valued output normalization, both sides) */
    size_t n = strs->c.count;
    for (size_t i = 1; i < n; i++) {
        for (size_t j = i; j > 0; j--) {
            const char *a = au_jstr(strs->c.items[j - 1]);
            const char *b = au_jstr(strs->c.items[j]);
            if (a && b && strcmp(a, b) > 0) {
                json_t *tmp = strs->c.items[j - 1];
                strs->c.items[j - 1] = strs->c.items[j];
                strs->c.items[j] = tmp;
            } else break;
        }
    }
    json_t *out = json_array();
    const char *prev = NULL;
    for (size_t i = 0; i < n; i++) {
        const char *v = au_jstr(strs->c.items[i]);
        if (prev && v && !strcmp(prev, v)) continue;
        json_append(out, json_string(v ? v : ""));
        prev = v;
    }
    json_free(strs);
    return out;
}

/* PoP: au_get_aws_sdk_auth_status @ hermes_cli/auth.py:_get_aws_sdk_auth_status */
static json_t *au_get_aws_sdk_auth_status(const au_seams_t *s, const char *target) {
    json_t *ir = au_import(s, "has_aws_credentials");
    if (au_is_raise(ir)) {
        if (au_caught(ir, "ImportError")) {
            json_free(ir);
            json_t *out = json_object();
            json_set(out, "logged_in", json_bool(false));
            au_aset_s(out, "provider", target);
            au_aset_s(out, "error", "boto3 not installed");
            return out;
        }
        return ir;
    }
    json_free(ir);
    json_t *r = au_call0(s, "has_aws_credentials");
    if (au_is_raise(r)) return r;
    json_t *out = json_object();
    json_set(out, "logged_in", r ? r : json_null());
    au_aset_s(out, "provider", target);
    return out;
}

/* PoP: au_external_process_spec @ hermes_cli/auth.py:_external_process_spec */
static json_t *au_external_process_spec(const au_seams_t *s, const json_t *pconfig) {
    json_t *a = json_object();
    au_aset_j(a, "pconfig", pconfig);
    json_t *pe = au_call(s, "provider_env_base_url", a);
    json_free(a);
    if (au_is_raise(pe)) return pe;
    json_t *base;
    if (au_jtruth(pe)) {
        base = pe;
    } else {
        json_free(pe);
        const json_t *ibu = json_obj_get(pconfig, "inference_base_url");
        base = ibu ? json_copy(ibu) : json_null();
    }
    /* try: profile = get_provider_profile(pconfig.id) — except Exception: None */
    json_t *profile = NULL;
    json_t *ir = au_import(s, "get_provider_profile");
    if (au_is_raise(ir)) {
        if (!au_is_exception(ir)) { json_free(base); return ir; }
        json_free(ir);
    } else {
        json_free(ir);
        a = json_object();
        au_aset_s(a, "provider_id", json_get_str(pconfig, "id", ""));
        json_t *pr = au_call(s, "get_provider_profile", a);
        json_free(a);
        if (au_is_raise(pr)) {
            if (!au_is_exception(pr)) { json_free(base); return pr; }
            json_free(pr);
        } else {
            profile = pr;
        }
    }
    /* tuple(getattr(profile, "process_command_env_vars", ()) or ()) */
    const json_t *cev = json_obj_get(profile, "process_command_env_vars");
    json_t *command_env_vars = (cev && cev->type == JSON_ARRAY && cev->c.count > 0)
                                   ? json_copy(cev) : json_array();
    const json_t *ae = json_obj_get(profile, "process_args_env_var");
    /* str(getattr(profile, "process_args_env_var", "") or "") — absent/falsy -> "" */
    char *args_env_var = au_jtruth(ae) ? au_pystr(ae) : strdup("");
    json_t *ir1 = au_import(s, "external_process_command");
    if (au_is_raise(ir1)) {
        free(args_env_var); json_free(command_env_vars); json_free(base);
        if (profile) json_free(profile);
        return ir1;
    }
    json_free(ir1);
    json_t *ir2 = au_import(s, "resolve_external_process_command");
    if (au_is_raise(ir2)) {
        free(args_env_var); json_free(command_env_vars); json_free(base);
        if (profile) json_free(profile);
        return ir2;
    }
    json_free(ir2);
    a = json_object();
    au_aset_j(a, "profile", profile);
    json_t *cmd = au_call(s, "external_process_command", a);
    json_free(a);
    if (au_is_raise(cmd)) {
        free(args_env_var); json_free(command_env_vars); json_free(base);
        if (profile) json_free(profile);
        return cmd;
    }
    char *raw_args = strdup("");
    if (args_env_var[0]) {
        a = json_object();
        au_aset_s(a, "name", args_env_var);
        au_aset_s(a, "default", "");
        json_t *gv = au_call(s, "os_getenv", a);
        json_free(a);
        if (au_is_raise(gv)) {
            free(raw_args); free(args_env_var); json_free(command_env_vars);
            json_free(base); json_free(cmd);
            if (profile) json_free(profile);
            return gv;
        }
        if (!gv || gv->type != JSON_STRING) {
            json_t *e = au_attr_raise(au_typename(gv), "strip");
            json_free(gv);
            free(raw_args); free(args_env_var); json_free(command_env_vars);
            json_free(base); json_free(cmd);
            if (profile) json_free(profile);
            return e;
        }
        free(raw_args);
        raw_args = au_strip(gv->str_val);
        json_free(gv);
    }
    free(args_env_var);
    json_t *args;
    if (raw_args[0]) {
        a = json_object();
        au_aset_s(a, "raw", raw_args);
        args = au_call(s, "shlex_split", a);
        json_free(a);
        if (au_is_raise(args)) {
            free(raw_args); json_free(command_env_vars); json_free(base); json_free(cmd);
            if (profile) json_free(profile);
            return args;
        }
    } else {
        const json_t *pa = json_obj_get(profile, "process_args");
        args = (pa && pa->type == JSON_ARRAY && pa->c.count > 0) ? json_copy(pa) : json_array();
    }
    free(raw_args);
    a = json_object();
    au_aset_j(a, "command", cmd);
    json_t *rc = au_call(s, "resolve_external_process_command", a);
    json_free(a);
    if (au_is_raise(rc)) {
        json_free(args); json_free(command_env_vars); json_free(base); json_free(cmd);
        if (profile) json_free(profile);
        return rc;
    }
    json_t *out = json_array();
    json_append(out, cmd);
    json_append(out, args);
    json_append(out, base);
    json_append(out, rc);
    json_append(out, command_env_vars);
    if (profile) json_free(profile);
    return out;
}