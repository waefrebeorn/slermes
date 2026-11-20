/*
 * auth_flow2.c — FLOW port of the STILL-UNCLAIMED remainder of
 * hermes_cli/auth.py (LIVE authority). Self-contained: libjson only. Every
 * function is static au2_*; the harness (#include "cli/auth_flow2.c") wires
 * the universal seam to scripted results and records the ordered trace.
 *
 * The authoritative gap set is live_parity_scan.json's
 * modules["hermes_cli/auth.py"].gaps (12 REAL_GAP entries). A CRLF-aware PoP
 * grep of src/ at authoring time showed 9 of those 12 already claimed
 * (_usable_declared_secret, _nonempty_str, _token_pairs_by_id,
 * _merge_pool_row_generation, _entry_ids, _suppressed_source_list,
 * get_auth_status, login_command, logout_command). Only the three below were
 * UNCLAIMED and are claimed here — nothing else in auth.py is re-ported:
 *
 *   PoP: au2_primary_failure_wording  @ hermes_cli/auth.py:primary_failure_wording
 *   PoP: au2_is_terminal_refresh_error @ hermes_cli/auth.py:is_terminal_refresh_error
 *   PoP: au2_resolve_nous_access_token @ hermes_cli/auth.py:_resolve_nous_access_token
 *
 * Claimed siblings are NOT re-ported: is_rate_limited_auth_error (auth.c) is
 * transcribed inline as a pure leaf (no PoP re-claim); every effectful
 * collaborator (lock CMs, cache, transaction CM, portal/verify/refresh
 * helpers, httpx.Client CM, anon_auth imports) enters through ONE universal
 * seam, mirroring the oracle's recording stubs.
 *
 * SEAM DOCTRINE (identical to auth_flow.c):
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * returns a NEW json_t* (NULL == Python None). A raising live call is scripted
 * {"__raise__": "<ExcClass>", "msg": ...}; the cores propagate it exactly
 * where the live try/with lets it escape. A MISSING seam key fail-closes: the
 * harness delivers {"__raise__":"RuntimeError","msg":"unscripted seam: <n>"};
 * a present JSON null is a valid None result. A JSON array script is one
 * result PER CALL (last repeats, empty -> None).
 *
 * ── Full builtin exception hierarchy ──
 * au2_exc_parent mirrors py3.14's builtin parent chain: BaseException ->
 * (SystemExit, KeyboardInterrupt, GeneratorExit, Exception); Exception ->
 * (ValueError, TypeError, RuntimeError, OSError, ...); AuthError's parent is
 * RuntimeError (auth_constants.AuthError subclasses RuntimeError). Unknown
 * custom kinds default to Exception. au2_caught() walks the chain, and the
 * BaseException trio stays OUTSIDE Exception. This is used by au2_is_exception
 * so an except-Exception clause can never swallow a BaseException passthrough.
 *
 * Context-manager exit model: on normal return or on a body raise, every
 * entered CM's "<name>_exit" seam fires innermost-first. An exit raise
 * OVERRIDES the pending outcome (outermost exit raise wins), matching Python;
 * none of the live CMs in this flow suppress (their __exit__ returns None),
 * so no suppression path is modelled. An enter that raises never pushes the
 * CM and therefore never fires its exit.
 *
 * Ownership: seam results and returned json_t* are malloc'd (caller frees);
 * inputs are borrowed; au2_seam borrows args. A propagated raise dict is
 * returned upward as-is; a caught raise dict is freed at the catch site.
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

typedef struct au2_trace { json_t *events; } au2_trace_t;

typedef struct au2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} au2_seams_t;

static void au2_trace_add(au2_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void au2_trace_free(au2_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *au2_seam(const au2_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}

/* ── JSON value helpers ── */

static bool au2_jtruth(const json_t *v) {
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
static const char *au2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool au2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool au2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool au2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }

static char *au2_xsf(const char *fmt, ...) {
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

/* ── exception model (FULL builtin parent hierarchy, py3.14) ── */

static const char *au2_raise_class(const json_t *r) {
    if (au2_is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool au2_is_raise(const json_t *r) { return au2_raise_class(r) != NULL; }

static json_t *au2_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

static bool au2_in_names(const char *k, const char *const *names) {
    for (size_t i = 0; names[i]; i++) if (!strcmp(k, names[i])) return true;
    return false;
}
static const char *au2_exc_parent(const char *k) {
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
    if (au2_in_names(k, EXC_CHILDREN)) return "Exception";
    static const char *const ARITH_CHILDREN[] = {
        "FloatingPointError", "OverflowError", "ZeroDivisionError", NULL
    };
    if (au2_in_names(k, ARITH_CHILDREN)) return "ArithmeticError";
    static const char *const IMPORT_CHILDREN[] = { "ModuleNotFoundError", "ZipImportError", NULL };
    if (au2_in_names(k, IMPORT_CHILDREN)) return "ImportError";
    static const char *const LOOKUP_CHILDREN[] = { "IndexError", "KeyError", NULL };
    if (au2_in_names(k, LOOKUP_CHILDREN)) return "LookupError";
    static const char *const OSERROR_CHILDREN[] = {
        "BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", NULL
    };
    if (au2_in_names(k, OSERROR_CHILDREN)) return "OSError";
    static const char *const CONN_CHILDREN[] = {
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", NULL
    };
    if (au2_in_names(k, CONN_CHILDREN)) return "ConnectionError";
    static const char *const RUNTIME_CHILDREN[] = { "NotImplementedError", "RecursionError", NULL };
    if (au2_in_names(k, RUNTIME_CHILDREN)) return "RuntimeError";
    if (!strcmp(k, "AuthError")) return "RuntimeError";  /* auth_constants.AuthError(RuntimeError) */
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    static const char *const UNICODE_CHILDREN[] = {
        "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError", NULL
    };
    if (au2_in_names(k, UNICODE_CHILDREN)) return "UnicodeError";
    if (!strcmp(k, "JSONDecodeError")) return "ValueError";
    static const char *const WARNING_CHILDREN[] = {
        "UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", NULL
    };
    if (au2_in_names(k, WARNING_CHILDREN)) return "Warning";
    static const char *const NAME_CHILDREN[] = { "UnboundLocalError", NULL };
    if (au2_in_names(k, NAME_CHILDREN)) return "NameError";
    if (!strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "BaseExceptionGroup";
    if (!strcmp(k, "PythonFinalizationError")) return "RuntimeError";
    if (!strcmp(k, "_IncompleteInputError")) return "SyntaxError";
    return "Exception";
}
static bool au2_caught(const json_t *r, const char *cls) {
    const char *k = au2_raise_class(r);
    if (k && !strcmp(k, "ExceptionGroup") && !strcmp(cls, "Exception")) return true;
    while (k) {
        if (!strcmp(k, cls)) return true;
        k = au2_exc_parent(k);
    }
    return false;
}
static bool au2_is_exception(const json_t *r) { return au2_caught(r, "Exception"); }

/* ── args builders ── */

static void au2_aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void au2_aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void au2_aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void au2_aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ── CM helpers ── */

/* Fire "<name>_enter" with args. Returns its result (possibly a raise). */
static json_t *au2_enter(const au2_seams_t *s, const char *name, const json_t *args) {
    char *n = au2_xsf("%s_enter", name);
    json_t *r = au2_seam(s, n, args);
    free(n);
    return r;
}
/* Fire "<name>_exit". Returns its result (raise => override; truthy non-raise
 * is ignored: none of the live CMs here suppress). */
static json_t *au2_exit(const au2_seams_t *s, const char *name) {
    char *n = au2_xsf("%s_exit", name);
    json_t *a = json_object();
    json_t *r = au2_seam(s, n, a);
    json_free(a);
    free(n);
    return r;
}
/* Unwind entered CMs innermost-first. On an exit raise, replace the pending
 * outcome (outermost exit raise wins). ret is owned by caller / transferred. */
static json_t *au2_unwind(const au2_seams_t *s, const char *const *stack, int n, json_t *ret) {
    for (int i = n - 1; i >= 0; i--) {
        json_t *ex = au2_exit(s, stack[i]);
        if (au2_is_raise(ex)) {
            if (ret) json_free(ret);
            ret = ex;
        } else {
            json_free(ex);
        }
    }
    return ret;
}

/* ════════════════════ FLOW 1: primary_failure_wording ════════════════════ */

/* Pure leaf transcribed inline: is_rate_limited_auth_error (claimed in
 * auth.c). isinstance(error, AuthError) and not error.relogin_required and
 * error.code == CODEX_RATE_LIMITED_CODE ("codex_rate_limited"). */
static bool au2_is_rate_limited(const json_t *error) {
    const char *kind = json_get_str(error, "kind", NULL);
    if (!kind || strcmp(kind, "AuthError")) return false;      /* isinstance gate */
    if (au2_jtruth(json_obj_get(error, "relogin_required"))) return false;
    const char *code = json_get_str(error, "code", NULL);
    return code && !strcmp(code, "codex_rate_limited");
}

/* PoP: au2_primary_failure_wording @ hermes_cli/auth.py:primary_failure_wording */
static json_t *au2_primary_failure_wording(const json_t *error) {
    json_t *out = json_array();
    if (au2_is_rate_limited(error)) {
        json_append(out, json_string("rate-limited (429)"));
        json_append(out, json_string("Primary provider quota exhausted"));
    } else {
        json_append(out, json_string("auth failed"));
        json_append(out, json_string("Primary auth failed"));
    }
    return out;
}

/* ════════════════════ FLOW 2: is_terminal_refresh_error ════════════════════ */

/* Method OAuthProviderFlow.is_terminal_refresh_error(self, exc):
 * isinstance(exc, AuthError) and exc.provider == self.provider_id
 * and exc.code in self.terminal_refresh_codes and bool(exc.relogin_required) */
/* PoP: au2_is_terminal_refresh_error @ hermes_cli/auth.py:is_terminal_refresh_error */
static json_t *au2_is_terminal_refresh_error(const json_t *self, const json_t *exc) {
    bool ok = false;
    do {
        const char *kind = json_get_str(exc, "kind", NULL);
        if (!kind || strcmp(kind, "AuthError")) break;        /* isinstance gate */
        const char *ep = json_get_str(exc, "provider", NULL);
        const char *pid = json_get_str(self, "provider_id", NULL);
        if (!ep || !pid || strcmp(ep, pid)) break;
        const char *code = json_get_str(exc, "code", NULL);
        if (!code) break;
        const json_t *codes = json_obj_get(self, "terminal_refresh_codes");
        if (!au2_is_arr(codes)) break;
        bool found = false;
        for (size_t i = 0; i < codes->c.count; i++) {
            const char *c = au2_jstr(codes->c.items[i]);
            if (c && !strcmp(c, code)) { found = true; break; }
        }
        if (!found) break;
        ok = au2_jtruth(json_obj_get(exc, "relogin_required"));
    } while (0);
    return json_bool(ok);
}

/* ════════════════════ FLOW 3: _resolve_nous_access_token ════════════════════ */

/* _memo(token): if memoable: with LOCK: CACHE[key] = (monotonic(), token);
 * return token. Order: lock_enter -> monotonic -> cache_set -> lock_exit. */
static json_t *au2_memo(const au2_seams_t *s, bool memoable, const json_t *cache_key,
                        const json_t *token) {
    if (!memoable) return json_copy(token);
    json_t *en = au2_enter(s, "cache_lock", json_object());
    if (au2_is_raise(en)) return en;
    json_t *ret = NULL;
    json_t *mn = au2_seam(s, "monotonic", json_object());
    if (au2_is_raise(mn)) {
        json_t *ex = au2_exit(s, "cache_lock");
        ret = au2_is_raise(ex) ? ex : mn;
        if (!au2_is_raise(ex)) { /* keep mn */ }
        if (au2_is_raise(ex)) json_free(mn);
        json_free(en);
        return ret;
    }
    json_t *a = json_object();
    au2_aset_j(a, "cache_key", cache_key);
    au2_aset_j(a, "token", token);
    json_t *cs = au2_seam(s, "cache_set", a);
    json_free(a);
    json_t *ex = au2_exit(s, "cache_lock");
    json_free(en);
    if (au2_is_raise(ex)) { json_free(mn); json_free(cs); return ex; }
    json_free(ex);
    if (au2_is_raise(cs)) { json_free(mn); return cs; }
    json_free(cs);
    json_free(mn);
    return json_copy(token);
}

/* _nous_err(message, "code", relogin=True) -> AuthError(msg, provider="nous",
 * code=..., relogin_required=relogin); raised. str(AuthError)==message. */
static json_t *au2_nous_err(const char *msg, const char *code) {
    return au2_raise_spec("AuthError", msg);
}

/* PoP: au2_resolve_nous_access_token @ hermes_cli/auth.py:_resolve_nous_access_token */
static json_t *au2_resolve_nous_access_token(const au2_seams_t *s, const json_t *in) {
    const json_t *tmo = json_obj_get(in, "timeout_seconds");
    double timeout_seconds = tmo && tmo->type == JSON_NUMBER ? tmo->num_val : 15.0;
    const json_t *insecure = json_obj_get(in, "insecure");
    const json_t *ca_bundle = json_obj_get(in, "ca_bundle");
    const json_t *skew = json_obj_get(in, "refresh_skew_seconds");
    double skew_s = skew && skew->type == JSON_NUMBER ? skew->num_val : 0.0;
    bool memoable = !au2_jtruth(insecure) && au2_is_null(ca_bundle);

    const char *cmstack[8];
    int cmn = 0;
    json_t *ret = NULL;
    const json_t *cache_key = NULL;   /* borrowed from hermes_home_key seam result */
    json_t *cache_key_owned = NULL;
    /* txn-block scratch, hoisted so `done:` can be reached from the cache-read
     * section without jumping into the scope of an uninitialised local. */
    json_t *auth_store = NULL, *state = NULL, *state_source_path = NULL;
    json_t *portal = NULL, *verify = NULL;

    /* ── cache read: with LOCK: cached = CACHE.get(key) ── */
    if (memoable) {
        cache_key_owned = au2_seam(s, "hermes_home_key", json_object());
        if (au2_is_raise(cache_key_owned)) { ret = cache_key_owned; cache_key_owned = NULL; goto done; }
        cache_key = cache_key_owned;
        json_t *en = au2_enter(s, "cache_lock", json_object());
        if (au2_is_raise(en)) { ret = en; goto done; }
        json_t *a = json_object();
        au2_aset_j(a, "cache_key", cache_key);
        json_t *cached = au2_seam(s, "cache_get", a);
        json_free(a);
        if (au2_is_raise(cached)) {
            json_t *ex = au2_exit(s, "cache_lock");
            ret = au2_is_raise(ex) ? ex : cached;
            if (au2_is_raise(ex)) json_free(cached);
            json_free(en);
            goto done;
        }
        json_t *ex = au2_exit(s, "cache_lock");
        json_free(en);
        if (au2_is_raise(ex)) { json_free(cached); ret = ex; goto done; }
        json_free(ex);
        /* if cached is not None and (monotonic() - cached[0]) < TTL(5.0) */
        if (!au2_is_null(cached) && au2_is_arr(cached) && cached->c.count >= 2 &&
            cached->c.items[0] && cached->c.items[0]->type == JSON_NUMBER) {
            json_t *mn = au2_seam(s, "monotonic", json_object());
            if (au2_is_raise(mn)) { json_free(cached); ret = mn; goto done; }
            double t0 = cached->c.items[0]->num_val;
            double m = mn->num_val;
            json_free(mn);
            if ((m - t0) < 5.0) {
                ret = json_copy(cached->c.items[1]);
                json_free(cached);
                goto done;
            }
        }
        json_free(cached);
    }

    /* ── with _provider_state_transaction("nous") as (store, state, path) ── */
    {
        json_t *a = json_object();
        au2_aset_s(a, "provider_id", "nous");
        json_t *en = au2_enter(s, "txn", a);
        json_free(a);
        if (au2_is_raise(en)) { ret = en; goto done; }
        if (!au2_is_arr(en) || en->c.count < 3) {
            /* malformed script: fail closed like an unscripted seam */
            json_free(en);
            char *msg = au2_xsf("unscripted seam: txn_enter");
            ret = au2_raise_spec("RuntimeError", msg);
            free(msg);
            goto done;
        }
        auth_store = json_copy(en->c.items[0]);
        state = json_copy(en->c.items[1]);
        state_source_path = json_copy(en->c.items[2]);
        json_free(en);
        cmstack[cmn++] = "txn";

        if (!au2_jtruth(state)) {
            ret = au2_nous_err("Hermes is not logged into Nous Portal.", "nous_auth_missing");
            goto done;
        }

        /* portal_base_url = _nous_portal_base_url(state) */
        json_t *a2 = json_object();
        au2_aset_j(a2, "state", state);
        portal = au2_seam(s, "nous_portal_base_url", a2);
        json_free(a2);
        if (au2_is_raise(portal)) { ret = portal; goto done; }
        const char *portal_s = au2_jstr(portal);

        /* client_id = str(state.get("client_id") or DEFAULT_NOUS_CLIENT_ID) */
        const char *cid = json_get_str(state, "client_id", NULL);
        const char *client_id = (cid && cid[0]) ? cid : "hermes-cli";

        /* verify = _resolve_verify(insecure=, ca_bundle=, auth_state=state) */
        json_t *a3 = json_object();
        au2_aset_j(a3, "insecure", insecure);
        au2_aset_j(a3, "ca_bundle", ca_bundle);
        au2_aset_j(a3, "auth_state", state);
        verify = au2_seam(s, "resolve_verify", a3);
        json_free(a3);
        if (au2_is_raise(verify)) { ret = verify; goto done; }

        /* lock_timeout = max(timeout_seconds + 5.0, AUTH_LOCK_TIMEOUT_SECONDS=15.0) */
        double lock_timeout = timeout_seconds + 5.0;
        if (lock_timeout < 15.0) lock_timeout = 15.0;

        /* with _nous_shared_store_lock(timeout_seconds=lock_timeout): */
        {
            json_t *a4 = json_object();
            au2_aset_n(a4, "timeout_seconds", lock_timeout);
            json_t *en2 = au2_enter(s, "shared_lock", a4);
            json_free(a4);
            if (au2_is_raise(en2)) { ret = en2; goto done; }
            json_free(en2);
            cmstack[cmn++] = "shared_lock";

            /* from hermes_cli.anon_auth import held_connectors_token,
             * is_guest_state, refresh_guest_state */
            json_t *imp = au2_seam(s, "import_anon_auth", json_object());
            if (au2_is_raise(imp)) { ret = imp; goto done; }
            json_free(imp);

            /* if is_guest_state(state): */
            json_t *a5 = json_object();
            au2_aset_j(a5, "state", state);
            json_t *guest = au2_seam(s, "is_guest_state", a5);
            json_free(a5);
            if (au2_is_raise(guest)) { ret = guest; goto done; }
            bool is_guest = au2_jtruth(guest);
            json_free(guest);

            if (is_guest) {
                /* if access_token := held_connectors_token(state, skew_seconds=..) */
                json_t *a6 = json_object();
                au2_aset_j(a6, "state", state);
                au2_aset_n(a6, "skew_seconds", skew_s);
                json_t *held = au2_seam(s, "held_connectors_token", a6);
                json_free(a6);
                if (au2_is_raise(held)) { ret = held; goto done; }
                if (au2_jtruth(held)) {
                    ret = au2_memo(s, memoable, cache_key, held);
                    json_free(held);
                    goto done;
                }
                json_free(held);

                /* with httpx.Client(timeout=httpx.Timeout(t or 15.0),
                 *   headers={"Accept":"application/json"}, verify=verify) as client: */
                double to = au2_jtruth(tmo) ? timeout_seconds : 15.0;
                json_t *a7 = json_object();
                au2_aset_n(a7, "timeout", to);
                json_t *hdr = json_object();
                json_set(hdr, "Accept", json_string("application/json"));
                json_set(a7, "headers", hdr);
                au2_aset_j(a7, "verify", verify);
                json_t *cen = au2_enter(s, "httpx_client", a7);
                json_free(a7);
                if (au2_is_raise(cen)) { ret = cen; goto done; }
                json_t *client = cen;   /* the scripted client value */
                cmstack[cmn++] = "httpx_client";

                /* minted = refresh_guest_state(state, client, purpose="connectors") */
                json_t *a8 = json_object();
                au2_aset_j(a8, "state", state);
                au2_aset_j(a8, "client", client);
                au2_aset_s(a8, "purpose", "connectors");
                json_t *minted = au2_seam(s, "refresh_guest_state", a8);
                json_free(a8);
                json_t *cex = au2_exit(s, "httpx_client");
                cmn--;  /* httpx_client exited (even when its exit raises) */
                if (au2_is_raise(cex)) {
                    if (minted) json_free(minted);
                    json_free(client);
                    ret = cex;
                    goto done;
                }
                json_free(cex);
                if (au2_is_raise(minted)) { json_free(client); ret = minted; goto done; }
                json_free(client);

                /* persist() = _save_provider_state_to_source(store,"nous",state,path) */
                json_t *a9 = json_object();
                au2_aset_j(a9, "auth_store", auth_store);
                au2_aset_s(a9, "provider_id", "nous");
                au2_aset_j(a9, "state", state);
                au2_aset_j(a9, "state_source_path", state_source_path);
                json_t *ps = au2_seam(s, "save_provider_state_to_source", a9);
                json_free(a9);
                if (au2_is_raise(ps)) { if (minted) json_free(minted); ret = ps; goto done; }
                json_free(ps);

                /* _write_shared_nous_state(state, renewal=True) */
                json_t *a10 = json_object();
                au2_aset_j(a10, "state", state);
                au2_aset_b(a10, "renewal", true);
                json_t *ws = au2_seam(s, "write_shared_nous_state", a10);
                json_free(a10);
                if (au2_is_raise(ws)) { if (minted) json_free(minted); ret = ws; goto done; }
                json_free(ws);

                ret = au2_memo(s, memoable, cache_key, minted);
                json_free(minted);
                goto done;
            }

            /* ── non-guest path ── */
            json_t *a11 = json_object();
            au2_aset_j(a11, "state", state);
            json_t *merged = au2_seam(s, "merge_shared_nous_oauth_state", a11);
            json_free(a11);
            if (au2_is_raise(merged)) { ret = merged; goto done; }
            bool merged_truthy = au2_jtruth(merged);
            json_free(merged);

            const json_t *access_token = json_obj_get(state, "access_token");
            const json_t *refresh_token = json_obj_get(state, "refresh_token");
            if (!au2_jstr(access_token) || !au2_jstr(access_token)[0]) {
                ret = au2_nous_err("No access token found for Nous Portal login.",
                                   "nous_auth_missing_access_token");
                goto done;
            }

            /* if not _is_expiring(state.get("expires_at"), skew): */
            json_t *a12 = json_object();
            au2_aset_j(a12, "expires_at", json_obj_get(state, "expires_at"));
            au2_aset_n(a12, "skew_seconds", skew_s);
            json_t *exp = au2_seam(s, "is_expiring", a12);
            json_free(a12);
            if (au2_is_raise(exp)) { ret = exp; goto done; }
            bool expiring = au2_jtruth(exp);
            json_free(exp);
            if (!expiring) {
                if (merged_truthy) {
                    json_t *a13 = json_object();
                    au2_aset_j(a13, "auth_store", auth_store);
                    au2_aset_s(a13, "provider_id", "nous");
                    au2_aset_j(a13, "state", state);
                    au2_aset_j(a13, "state_source_path", state_source_path);
                    json_t *ps = au2_seam(s, "save_provider_state_to_source", a13);
                    json_free(a13);
                    if (au2_is_raise(ps)) { ret = ps; goto done; }
                    json_free(ps);
                }
                ret = au2_memo(s, memoable, cache_key, access_token);
                goto done;
            }

            if (!au2_jstr(refresh_token) || !au2_jstr(refresh_token)[0]) {
                ret = au2_nous_err("Session expired and no refresh token is available.",
                                   "nous_auth_missing_refresh_token");
                goto done;
            }

            /* with httpx.Client(...) as client: refreshed = _refresh_nous_or_quarantine(...) */
            {
                double to2 = au2_jtruth(tmo) ? timeout_seconds : 15.0;
                json_t *a14 = json_object();
                au2_aset_n(a14, "timeout", to2);
                json_t *hdr2 = json_object();
                json_set(hdr2, "Accept", json_string("application/json"));
                json_set(a14, "headers", hdr2);
                au2_aset_j(a14, "verify", verify);
                json_t *cen2 = au2_enter(s, "httpx_client", a14);
                json_free(a14);
                if (au2_is_raise(cen2)) { ret = cen2; goto done; }
                json_t *client2 = cen2;
                cmstack[cmn++] = "httpx_client";

                json_t *a15 = json_object();
                au2_aset_j(a15, "client", client2);
                au2_aset_j(a15, "auth_store", auth_store);
                au2_aset_j(a15, "state", state);
                au2_aset_s(a15, "portal_base_url", portal_s);
                au2_aset_s(a15, "client_id", client_id);
                au2_aset_j(a15, "refresh_token", refresh_token);
                au2_aset_s(a15, "reason", "managed_access_token_refresh_failure");
                json_t *marker = json_object();
                json_set(marker, "__callable__", json_string("persist"));
                json_set(a15, "persist", marker);
                json_t *refreshed = au2_seam(s, "refresh_nous_or_quarantine", a15);
                json_free(a15);
                json_t *cex2 = au2_exit(s, "httpx_client");
                cmn--;  /* httpx_client exited (even when its exit raises) */
                if (au2_is_raise(cex2)) {
                    if (refreshed) json_free(refreshed);
                    json_free(client2);
                    ret = cex2;
                    goto done;
                }
                json_free(cex2);
                if (au2_is_raise(refreshed)) { json_free(client2); ret = refreshed; goto done; }
                json_free(client2);

                /* _apply_nous_refreshed_tokens(state, refreshed, refresh_token):
                 * both sides write result["access_token"] into state. */
                json_t *a16 = json_object();
                au2_aset_j(a16, "state", state);
                au2_aset_j(a16, "refreshed", refreshed);
                au2_aset_j(a16, "refresh_token", refresh_token);
                json_t *ap = au2_seam(s, "apply_nous_refreshed_tokens", a16);
                json_free(a16);
                if (au2_is_raise(ap)) { json_free(refreshed); ret = ap; goto done; }
                if (au2_is_obj(ap)) {
                    const char *at = json_get_str(ap, "access_token", NULL);
                    if (at) json_set(state, "access_token", json_string(at));
                }
                json_free(ap);
                json_free(refreshed);

                /* state["portal_base_url"] / ["client_id"] / ["tls"] = ... */
                if (portal_s) json_set(state, "portal_base_url", json_string(portal_s));
                json_set(state, "client_id", json_string(client_id));
                json_t *a17 = json_object();
                au2_aset_j(a17, "verify", verify);
                json_t *tls = au2_seam(s, "tls_state_from_verify", a17);
                json_free(a17);
                if (au2_is_raise(tls)) { ret = tls; goto done; }
                json_set(state, "tls", json_copy(tls));
                json_free(tls);

                /* persist(); _write_shared_nous_state(state, renewal=True) */
                json_t *a18 = json_object();
                au2_aset_j(a18, "auth_store", auth_store);
                au2_aset_s(a18, "provider_id", "nous");
                au2_aset_j(a18, "state", state);
                au2_aset_j(a18, "state_source_path", state_source_path);
                json_t *ps2 = au2_seam(s, "save_provider_state_to_source", a18);
                json_free(a18);
                if (au2_is_raise(ps2)) { ret = ps2; goto done; }
                json_free(ps2);
                json_t *a19 = json_object();
                au2_aset_j(a19, "state", state);
                au2_aset_b(a19, "renewal", true);
                json_t *ws2 = au2_seam(s, "write_shared_nous_state", a19);
                json_free(a19);
                if (au2_is_raise(ws2)) { ret = ws2; goto done; }
                json_free(ws2);

                ret = au2_memo(s, memoable, cache_key, json_obj_get(state, "access_token"));
                goto done;
            } /* httpx.Client (refresh) */
        } /* shared_lock */
    } /* txn block */

done:
    /* Unwind any CMs still on the stack (txn, shared_lock, httpx_client). */
    ret = au2_unwind(s, cmstack, cmn, ret);
    if (auth_store) json_free(auth_store);
    if (state) json_free(state);
    if (state_source_path) json_free(state_source_path);
    if (portal) json_free(portal);
    if (verify) json_free(verify);
    if (cache_key_owned) json_free(cache_key_owned);
    return ret;
}
