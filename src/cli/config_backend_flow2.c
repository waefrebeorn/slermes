/*
 * config_backend_flow2.c — hermes_cli/config_backend.py UNCLAIMED remainder (C11 port).
 * LIVE authority: C:/Users/eman5/hermes-agent/hermes_cli/config_backend.py
 *
 * Sibling of config_backend_flow.c (42 funcs) and config_migration_flow.c (10 funcs).
 * Every function claimed by those two PoP sets is SEAMED here, never re-ported.
 * This file ports the defs NO existing PoP claims (grep PoP first):
 *
 *   FLOW cores
 *     _refuse_redacted_sets            the keyed-write redaction guard: resolves the
 *                                      hermes_cli.config_credential_values import,
 *                                      defines the read_old closure, calls the imported
 *                                      refuse_redacted_config_sets(sets, read_old).
 *     _refuse_redacted_sets.read_old   the nested guard-read closure:
 *                                      keeping_read_base(lambda: read_config_doc(cp)) or {}
 *                                      with `except FileNotFoundError: return {}`.
 *   constructors (projection builders; setup_flow2 ctor precedent)
 *     ConfigWriteError.__init__        super().__init__(message); self.code = code
 *     ConfigLockedError.__init__       message or f"{path} is locked by the {locked_by} level",
 *                                      code="config_key_locked"; self.path / self.locked_by
 *     ConfigValueError.__init__        super().__init__(message, code=code)
 *     _ExplicitFile.__init__           self._path = path
 *     UserLayer.__init__               frozen dataclass ctor (doc, version required;
 *                                      locks default_factory=dict, provenance default "")
 *     Changes.__init__                 frozen dataclass ctor (document=None, set={},
 *                                      unset=(), extra_content_on_create=None)
 *   ConfigBackend Protocol surface (19 methods whose live body is docstring + `...`,
 *   i.e. they return None regardless of arguments; the concrete FileBackend impls and
 *   the module-level delegators are CLAIMED elsewhere — these abstract defs are not):
 *     read_user_doc read_user_doc_readonly version cache_version exists probe_readable
 *     write_changes locked supports_file_tooling honors_managed_config boot
 *     protected_env_names deployment_env_names bootstrap_env_names preserve_read_base
 *     doctor_warnings apply_in_memory describe destination
 *
 * Self-contained, libjson-only. Every live effect (the imported collaborator and the
 * CLAIMED siblings keeping_read_base / read_config_doc) enters through ONE universal
 * seam; the cores perform NO I/O:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON object
 * (the seam records a copy and must not free it); the result is a NEW json_t* the
 * caller frees. NULL == Python None (never a JSON_NULL node);
 * {"__raise__": cls, "msg": msg} == a raised exception. The harness records
 * {"ev": name, "args": args, "ret": result} per call in order; the oracle's recording
 * stubs emit the identical shape driving LIVE Python with the same scripts.
 * BaseException kinds (SystemExit / KeyboardInterrupt / GeneratorExit and the
 * ConfigBackendUnavailable SystemExit subclass) PASS THROUGH every catch site; only
 * the SPECIFIC named except clause (FileNotFoundError in read_old) swallows its own
 * class family. With no seam installed the dispatcher is fail-closed (RuntimeError
 * raise spec); the harness script_call is ALSO fail-closed: a MISSING script returns
 * a RuntimeError raise spec (a scripted null is a valid None result).
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == None; a {"__raise__": ...} object == a propagated exception; the ctor
 * ports return a projection OBJECT of the constructed instance's state.
 *
 * Seam name table (name -> live call; args key order is part of the trace):
 *   refuse_redacted_config_sets  refuse_redacted_config_sets(sets, read_old)
 *                                args{sets, read_old:"<callback>"} ret null|raise
 *                                (the read_old callable projects to the fixed marker;
 *                                the seamed collaborator never invokes it — its own
 *                                guard-read behaviour is covered by the read_old port)
 *   keeping_read_base            keeping_read_base(read)   args{config_path} ret doc|raise
 *                                (CLAIMED sibling; the read callback projects to its
 *                                captured config_path; the stub does not call it)
 *   read_config_doc              read_config_doc(config_path) args{config_path} ret doc|raise
 *                                (CLAIMED sibling; unreachable in this domain because
 *                                the keeping_read_base stub never invokes the callback)
 *
 * Representation contract (fixture domain, identical on both sides):
 *  - ASCII text; POSIX lexical paths (backslash->slash, trailing slashes stripped).
 *  - integral JSON numbers read as int (libjson has no int/float identity).
 *  - the in-function `from hermes_cli.config_credential_values import
 *    refuse_redacted_config_sets` resolves silently (sys.modules stub on the live
 *    side); import FAILURE is outside the fixture domain.
 *  - ctor fixture kwargs are restricted to the declared fields (no binding-error
 *    cases: unknown kwarg / missing required are outside the domain); a PRESENT null
 *    passes through verbatim (dataclasses do not coerce), an ABSENT field takes its
 *    default. Exception projections: {message, code, args} (+ path, locked_by for
 *    ConfigLockedError); message is str(e), args is list(e.args).
 *  - _ExplicitFile projects to the backend-desc shape {"explicit": path, "name":"file"}
 *    (same projection config_backend_flow uses for the instance).
 *  - the Protocol methods ignore self and every argument and return None (NULL).
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
#include "json.h"

/* ── JSON / Python-value helpers ── */
static bool cb2_jtruth(const json_t *v) {
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
static bool cb2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool cb2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static char *cb2_xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}

/* ── universal seam ── */
typedef struct cb2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} cb2_seams_t;

static json_t *cb2_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static bool cb2_is_raise(const json_t *r) {
    return cb2_is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *cb2_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
/* Fail-closed dispatcher: no seam installed -> RuntimeError raise spec.
 * Ownership: cb2_call OWNS args (frees it after the call); the seam BORROWS args
 * (records a deep copy, must not free it). Results are NEW (caller frees). */
static json_t *cb2_call(const cb2_seams_t *s, const char *name, json_t *args) {
    if (!s || !s->call) {
        json_free(args);
        return cb2_raise("RuntimeError", "no seam installed");
    }
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}
static json_t *cb2_args0(void) { return json_object(); }
static void cb2_set_str(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
/* set a key to a copy of v, or an explicit JSON null when v is NULL/JSON_NULL.
 * (json_set(obj,key,NULL) is a no-op in libjson — never pass NULL.) */
static void cb2_set_node(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, (v && v->type != JSON_NULL) ? json_copy(v) : json_null());
}

/* ── FULL builtin exception hierarchy (child -> parent) for isinstance-style
 *    catch folding. The only catch site in this port is `except FileNotFoundError`
 *    (read_old); the full table is carried so a scripted subclass name folds into
 *    its ancestor exactly as live isinstance does. ExceptionGroup's second base
 *    (Exception) is modelled by its primary BaseExceptionGroup parent; it is
 *    outside the fixture domain. ── */
typedef struct cb2_exc_pair { const char *child; const char *parent; } cb2_exc_pair_t;
static const cb2_exc_pair_t CB2_EXC_TREE[] = {
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
    /* module-defined exception family (config_backend.py) */
    {"ConfigBackendUnavailable", "SystemExit"},
    {"ConfigWriteError", "RuntimeError"},
    {"ConfigLockedError", "ConfigWriteError"},
    {"ConfigValueError", "ConfigWriteError"},
};
static const char *cb2_exc_parent(const char *c) {
    for (size_t i = 0; i < sizeof(CB2_EXC_TREE) / sizeof(CB2_EXC_TREE[0]); i++)
        if (!strcmp(CB2_EXC_TREE[i].child, c)) return CB2_EXC_TREE[i].parent;
    return NULL;
}
/* isinstance(c, ancestor): walk child -> parent up to BaseException. */
static bool cb2_cls_is(const char *c, const char *ancestor) {
    if (!c || !*c) return false;
    while (c) {
        if (!strcmp(c, ancestor)) return true;
        c = cb2_exc_parent(c);
    }
    return false;
}

/* ── POSIX lexical path normalization (mirror pathlib / PathDouble) ── */
static char *cb2_path_posix(const char *p) {
    if (!p) return strdup("");
    char *s = strdup(p);
    for (char *q = s; *q; q++) if (*q == '\\') *q = '/';
    size_t n = strlen(s);
    while (n > 1 && s[n - 1] == '/') s[--n] = '\0';
    return s;
}

/* ═══════════════════════ FLOW cores ═══════════════════════ */

/* PoP: cb2_refuse_redacted_sets @ hermes_cli/config_backend.py:_refuse_redacted_sets */
json_t *cb2_refuse_redacted_sets(const cb2_seams_t *s, const char *config_path,
                                 const json_t *sets) {
    /* from hermes_cli.config_credential_values import refuse_redacted_config_sets
     * — resolves silently (sys.modules stub); import failure is outside the domain.
     * config_path is captured by the read_old closure only; the seamed collaborator
     * receives (sets, read_old) and never invokes the callback in this domain. */
    (void)config_path;
    json_t *args = json_object();
    cb2_set_node(args, "sets", sets);
    cb2_set_str(args, "read_old", "<callback>");
    json_t *r = cb2_call(s, "refuse_redacted_config_sets", args);
    if (cb2_is_raise(r)) return r;
    json_free(r);
    return NULL;  /* _refuse_redacted_sets returns None */
}

/* PoP: cb2_read_old @ hermes_cli/config_backend.py:_refuse_redacted_sets.read_old */
json_t *cb2_read_old(const cb2_seams_t *s, const char *config_path) {
    /* try: return keeping_read_base(lambda: read_config_doc(config_path)) or {}
     * except FileNotFoundError: return {}
     * keeping_read_base is CLAIMED -> seam; the read callback projects to its
     * captured config_path. `or` returns the OPERAND (truthy seam result verbatim). */
    json_t *args = json_object();
    cb2_set_str(args, "config_path", config_path);
    json_t *r = cb2_call(s, "keeping_read_base", args);
    if (cb2_is_raise(r)) {
        if (cb2_cls_is(cb2_raise_class(r), "FileNotFoundError")) {
            json_free(r);
            return json_object();  /* except FileNotFoundError: return {} */
        }
        return r;  /* BaseException / other OSError / anything else passes through */
    }
    if (cb2_jtruth(r)) return r;
    json_free(r);
    return json_object();  /* falsy -> {} */
}

/* ═══════════════════════ exception constructors ═══════════════════════ */

/* PoP: cb2_config_write_error_init @ hermes_cli/config_backend.py:ConfigWriteError.__init__ */
json_t *cb2_config_write_error_init(const char *message, const json_t *code) {
    /* super().__init__(message); self.code = code  (default "config_write_failed").
     * Projection {message, code, args}; args == (message,). A PRESENT-null code
     * passes through (self.code = None); an ABSENT code takes the default. */
    const char *msg = message ? message : "";
    json_t *o = json_object();
    cb2_set_str(o, "message", msg);
    json_set(o, "code", code ? json_copy(code) : json_string("config_write_failed"));
    json_t *args = json_array();
    json_append(args, json_string(msg));
    json_set(o, "args", args);
    return o;
}

/* PoP: cb2_config_locked_error_init @ hermes_cli/config_backend.py:ConfigLockedError.__init__ */
json_t *cb2_config_locked_error_init(const char *path, const char *locked_by,
                                     const json_t *message) {
    /* super().__init__(message or f"{path} is locked by the {locked_by} level",
     *                  code="config_key_locked")
     * self.path = path; self.locked_by = locked_by
     * The super() chain runs REAL through cb2_config_write_error_init. A truthy
     * message is used verbatim; a falsy one (None / "") takes the f-string default. */
    const char *p = path ? path : "";
    const char *by = locked_by ? locked_by : "";
    char *alloc = NULL;
    const char *msg;
    if (message && message->type != JSON_NULL && cb2_jtruth(message) && cb2_is_str(message)) {
        msg = message->str_val ? message->str_val : "";
    } else {
        alloc = cb2_xsf("%s is locked by the %s level", p, by);
        msg = alloc;
    }
    json_t *code = json_string("config_key_locked");
    json_t *o = cb2_config_write_error_init(msg, code);  /* REAL composition */
    json_free(code);
    free(alloc);
    cb2_set_str(o, "path", p);
    cb2_set_str(o, "locked_by", by);
    return o;  /* {message, code, args, path, locked_by} */
}

/* PoP: cb2_config_value_error_init @ hermes_cli/config_backend.py:ConfigValueError.__init__ */
json_t *cb2_config_value_error_init(const char *message, const json_t *code) {
    /* super().__init__(message, code=code)  (default "config_value_invalid") —
     * delegates REAL to ConfigWriteError.__init__ with the defaulted code. */
    json_t *c = code ? json_copy(code) : json_string("config_value_invalid");
    json_t *o = cb2_config_write_error_init(message, c);
    json_free(c);
    return o;
}

/* ═══════════════════════ dataclass / instance constructors ═══════════════════════ */

/* PoP: cb2_explicit_file_init @ hermes_cli/config_backend.py:_ExplicitFile.__init__ */
json_t *cb2_explicit_file_init(const char *path) {
    /* self._path = path (a Path). Projection = backend-desc shape (name inherited
     * from FileBackend). PathDouble / cb2_path_posix normalize identically. */
    char *p = cb2_path_posix(path);
    json_t *o = json_object();
    cb2_set_str(o, "explicit", p);
    cb2_set_str(o, "name", "file");
    free(p);
    return o;
}

/* PoP: cb2_user_layer_init @ hermes_cli/config_backend.py:UserLayer.__init__ */
json_t *cb2_user_layer_init(const json_t *fields) {
    /* frozen dataclass ctor: doc, version required (domain always supplies them);
     * locks default_factory=dict, provenance default "". PRESENT values pass through
     * verbatim (no coercion); only an ABSENT field takes its default. */
    json_t *o = json_object();
    cb2_set_node(o, "doc", json_obj_get(fields, "doc"));
    cb2_set_node(o, "version", json_obj_get(fields, "version"));
    const json_t *locks = json_obj_get(fields, "locks");
    json_set(o, "locks", locks ? json_copy(locks) : json_object());
    const json_t *prov = json_obj_get(fields, "provenance");
    json_set(o, "provenance", prov ? json_copy(prov) : json_string(""));
    return o;
}

/* PoP: cb2_changes_init @ hermes_cli/config_backend.py:Changes.__init__ */
json_t *cb2_changes_init(const json_t *fields) {
    /* frozen dataclass ctor: document=None, set=default_factory dict, unset=(),
     * extra_content_on_create=None. PRESENT values verbatim; ABSENT -> default. */
    json_t *o = json_object();
    cb2_set_node(o, "document", json_obj_get(fields, "document"));
    const json_t *set = json_obj_get(fields, "set");
    json_set(o, "set", set ? json_copy(set) : json_object());
    const json_t *unset = json_obj_get(fields, "unset");
    json_set(o, "unset", unset ? json_copy(unset) : json_array());
    cb2_set_node(o, "extra_content_on_create", json_obj_get(fields, "extra_content_on_create"));
    return o;
}

/* ═══════════════════════ ConfigBackend Protocol surface ═══════════════════════
 * Each live method body is a docstring + `...` — it reads neither self nor its
 * arguments and returns None. The faithful C port returns NULL. The concrete
 * FileBackend implementations and the module-level delegators are CLAIMED by
 * config_backend_flow.c / config_migration_flow.c; these abstract defs are not. */

/* PoP: cb2_proto_read_user_doc @ hermes_cli/config_backend.py:ConfigBackend.read_user_doc */
json_t *cb2_proto_read_user_doc(void) { return NULL; }

/* PoP: cb2_proto_read_user_doc_readonly @ hermes_cli/config_backend.py:ConfigBackend.read_user_doc_readonly */
json_t *cb2_proto_read_user_doc_readonly(void) { return NULL; }

/* PoP: cb2_proto_version @ hermes_cli/config_backend.py:ConfigBackend.version */
json_t *cb2_proto_version(void) { return NULL; }

/* PoP: cb2_proto_cache_version @ hermes_cli/config_backend.py:ConfigBackend.cache_version */
json_t *cb2_proto_cache_version(void) { return NULL; }

/* PoP: cb2_proto_exists @ hermes_cli/config_backend.py:ConfigBackend.exists */
json_t *cb2_proto_exists(void) { return NULL; }

/* PoP: cb2_proto_probe_readable @ hermes_cli/config_backend.py:ConfigBackend.probe_readable */
json_t *cb2_proto_probe_readable(void) { return NULL; }

/* PoP: cb2_proto_write_changes @ hermes_cli/config_backend.py:ConfigBackend.write_changes */
json_t *cb2_proto_write_changes(void) { return NULL; }

/* PoP: cb2_proto_locked @ hermes_cli/config_backend.py:ConfigBackend.locked */
json_t *cb2_proto_locked(void) { return NULL; }

/* PoP: cb2_proto_supports_file_tooling @ hermes_cli/config_backend.py:ConfigBackend.supports_file_tooling */
json_t *cb2_proto_supports_file_tooling(void) { return NULL; }

/* PoP: cb2_proto_honors_managed_config @ hermes_cli/config_backend.py:ConfigBackend.honors_managed_config */
json_t *cb2_proto_honors_managed_config(void) { return NULL; }

/* PoP: cb2_proto_boot @ hermes_cli/config_backend.py:ConfigBackend.boot */
json_t *cb2_proto_boot(void) { return NULL; }

/* PoP: cb2_proto_protected_env_names @ hermes_cli/config_backend.py:ConfigBackend.protected_env_names */
json_t *cb2_proto_protected_env_names(void) { return NULL; }

/* PoP: cb2_proto_deployment_env_names @ hermes_cli/config_backend.py:ConfigBackend.deployment_env_names */
json_t *cb2_proto_deployment_env_names(void) { return NULL; }

/* PoP: cb2_proto_bootstrap_env_names @ hermes_cli/config_backend.py:ConfigBackend.bootstrap_env_names */
json_t *cb2_proto_bootstrap_env_names(void) { return NULL; }

/* PoP: cb2_proto_preserve_read_base @ hermes_cli/config_backend.py:ConfigBackend.preserve_read_base */
json_t *cb2_proto_preserve_read_base(void) { return NULL; }

/* PoP: cb2_proto_doctor_warnings @ hermes_cli/config_backend.py:ConfigBackend.doctor_warnings */
json_t *cb2_proto_doctor_warnings(void) { return NULL; }

/* PoP: cb2_proto_apply_in_memory @ hermes_cli/config_backend.py:ConfigBackend.apply_in_memory */
json_t *cb2_proto_apply_in_memory(void) { return NULL; }

/* PoP: cb2_proto_describe @ hermes_cli/config_backend.py:ConfigBackend.describe */
json_t *cb2_proto_describe(void) { return NULL; }

/* PoP: cb2_proto_destination @ hermes_cli/config_backend.py:ConfigBackend.destination */
json_t *cb2_proto_destination(void) { return NULL; }
