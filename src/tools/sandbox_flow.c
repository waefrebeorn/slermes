/*
 * sandbox_flow.c — FLOW port of the UNCLAIMED sandbox surface of
 * tools/environments/sandbox_scope.py (the quarry's tools/ sandbox module;
 * the legacy src/tools/sandbox.c path-security infra has no live Python
 * counterpart and is not a differential port target).
 *
 * Ports the decision sequences of the scope/identity guard: hosted-scope
 * resolution, sandbox identity keys (percent-quoted), visibility predicates,
 * identity refusal flows, hosted-environment admission/creation, the
 * ScopedEnvironment delegation wrappers (_guarded / _lifecycle / process
 * ops / attribute passthrough) and the ScopedPipe byte-pipe proxy. Every
 * live external call (current_scope, bind_state, provider.create_environment,
 * inner.<op>, pipe.<op>, the wrapped fn) enters through the universal seam;
 * the cores perform no I/O.
 *
 * ── Universal seam ─────────────────────────────────────────────────────────
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * name identifies the live call (table below); args is a BORROWED object;
 * the result is a NEW json_t the caller frees. NULL == Python None (the
 * port normalizes it to a json_null node internally). A scripted exception
 * is {"__raise__": "<Kind>", "msg": "..."}; every sb_* entry propagates it.
 * The module has NO `except Exception` swallow sites (AST audit: the only
 * try/except is accepts_scope's `except (TypeError, ValueError)` around
 * inspect.signature, which is not seamed — modeled by the signature_error
 * descriptor), so BaseException kinds (SystemExit / KeyboardInterrupt /
 * GeneratorExit / StopIteration) pass through every flow unchanged. No
 * exception-parent table is needed. Fail-closed: no seam channel installed
 * -> RuntimeError raise spec; the harness dispatcher returns a RuntimeError
 * raise spec for a MISSING or EMPTY-ARRAY script (recorded before propagate).
 *
 * Seam name -> live call (args keys; ret):
 *   current_scope               hermes_scope.current_scope {}; scope|null
 *   bind_state                  agent.runtime_scope.bind_state {owner}; CM
 *                               marker (enter/exit are silent no-ops on both
 *                               sides; a scripted raise fires at the call)
 *   provider.create_environment provider.create_environment {scope,sandbox,
 *                               **kwargs}; {inner,inner_repr}|null
 *   inner.<name>                inner method call {args,kwargs} (guarded) or
 *                               {name,argv,child_env?,scope} / {name,scope}
 *                               (process ops); scripted result
 *   pipe.<op>                   pipe method call (per-op args below); result
 *   fn                          _scoped_call's wrapped fn {args,kwargs}; result
 *
 * ── JSON record model (all ASCII; bytes modeled as str) ───────────────────
 *   scope     {"tenant": str, "agent": str}   is_local derived tenant=="local"
 *   identity  {"tenant": str, "agent": str}   SandboxIdentity; key derived
 *   env       {"__scoped_env__": true, "inner": {<attr>: <desc>},
 *              "inner_repr": str, "sandbox": identity, "owner": scope|null}
 *   desc      {"callable": true, "params": [{"name","kind"}]}  method
 *             {"callable": false, "value": <json>}             attribute
 *             {"signature_error": true}  uninspectable callable (accepts_scope
 *             only; never an inner op — those must be callable or absent)
 *   param kind  pos_only | pos_or_kw | var_pos | kw_only | var_kw
 *   pipe desc {"methods": [<op>, ...], "closed": <json>}  (closed may be absent)
 *   pipe state{"identity": identity, "pipe": <pipe desc>}
 *   stream    {"stdin": <pipe desc>, "stdout": <pipe desc>, "poll": <json>}
 *             (keys may be absent -> AttributeError; null stream -> NoneType)
 *   ref       str | {"__process_ref__": true, "sandbox": str, "name": str}
 *   path      str | {"__fspath__": str} | null | other (TypeError)
 *   provider  {"serves_hosted_scopes": <json>} | null | other
 *
 * ── Projections (ret) ─────────────────────────────────────────────────────
 *   scope/identity -> {"tenant","agent"}; env -> the env record (copied);
 *   guarded ProcessStream -> {"ref":{"sandbox":key,"name"},
 *     "stdin":{"__scoped_pipe__":identity},"stdout":{...},
 *     "poll":{"__scoped_call__":"poll","identity":identity}};
 *   __getattr__ callable -> {"__wrapped__":"guarded"|"lifecycle","name",
 *     "identity"} (branch projected by _LIFECYCLE membership; the wrapper's
 *     behaviour is exercised by the env_guarded/env_lifecycle flows);
 *   _scoped_call wrapper -> {"__scoped_call__":what,"identity":identity};
 *   ScopedPipe (iter/enter) -> {"__scoped_pipe__":identity};
 *   _op -> the pipe descriptor.
 *
 * ── Domains / exclusions (documented, enforced by the generator) ──────────
 *   - scope/identity records carry only tenant+agent (non-empty strings).
 *   - inner method calls are always well-bound (argument-binding TypeErrors
 *     for inner ops are out of domain); the create_environment scope/sandbox
 *     kwarg collision IS modeled (stub __module__ pinned to sandbox_provider).
 *   - event.sandbox_key truthy values: str / int / bool only (float/container
 *     str() projections excluded). Integral JSON numbers read as int.
 *   - __fspath__ doubles return str only; bytes paths excluded.
 *   - pipe op results are str/int/bool/null (bytes-as-str).
 *   - setattr values are non-callable JSON.
 *   - current_scope scripts: null or a scope record.
 *
 * Ownership: every sb_* entry returns an OWNED json_t (result or raise spec).
 * args builders take ownership of what json_set receives; borrowed fixture
 * nodes are json_copy'd. The harness frees the returned node.
 */
#ifndef SANDBOX_FLOW_C
#define SANDBOX_FLOW_C

#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>

/* ── universal seam channel ── */
typedef struct sb_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} sb_seams_t;

/* ── raise specs ── */
static bool sb_is_raise(const json_t *v) {
    if (!v || v->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(v, "__raise__");
    return k && k->type == JSON_STRING;
}
static json_t *sb_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static json_t *sb_raise_fmt(const char *kind, const char *fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return sb_raise(kind, buf);
}
/* BaseException kinds that pass through except Exception (no swallow sites
 * exist in this module; kept for the harness/oracle classification parity). */
static bool sb_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0
                 || strcmp(kind, "KeyboardInterrupt") == 0
                 || strcmp(kind, "GeneratorExit") == 0);
}

/* ── Python truthiness / None ── */
static bool sb_truthy(const json_t *v) {
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
static bool sb_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }

/* ── seam call (takes ownership of args; NULL result normalized to null) ── */
static json_t *sb_call(const sb_seams_t *s, const char *name, json_t *args) {
    json_t *a = args ? args : json_object();
    json_t *r;
    if (s && s->call) r = s->call(s->ctx, name, a);
    else r = sb_raise("RuntimeError", "no seam channel installed");
    json_free(a);
    if (!r) r = json_null();
    return r;
}

/* ── type names (integral JSON numbers read as int) ── */
static const char *sb_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (isfinite(v->num_val) && v->num_val == floor(v->num_val)) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}
static json_t *sb_attr_err(const char *tn, const char *attr) {
    return sb_raise_fmt("AttributeError", "'%s' object has no attribute '%s'", tn, attr);
}
/* attribute error on a double carrier: objects are SimpleNamespace doubles */
static json_t *sb_double_attr_err(const json_t *carrier, const char *attr) {
    if (carrier && carrier->type == JSON_OBJECT)
        return sb_attr_err("types.SimpleNamespace", attr);
    return sb_attr_err(sb_type_name(carrier), attr);
}

/* ── urllib.parse.quote(s, safe='') ── */
static char *sb_quote(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n * 3 + 1);
    if (!out) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
            || c == '-' || c == '_' || c == '.' || c == '~') {
            out[k++] = (char)c;
        } else {
            static const char hex[] = "0123456789ABCDEF";
            out[k++] = '%';
            out[k++] = hex[c >> 4];
            out[k++] = hex[c & 15];
        }
    }
    out[k] = '\0';
    return out;
}
/* SandboxIdentity.key: f"sandbox:{quote(tenant)}/{quote(agent)}" */
static char *sb_key(const char *tenant, const char *agent) {
    char *qt = sb_quote(tenant ? tenant : "");
    char *qa = sb_quote(agent ? agent : "");
    size_t n = strlen(qt) + strlen(qa) + 16;
    char *out = malloc(n);
    if (out) snprintf(out, n, "sandbox:%s/%s", qt, qa);
    free(qt); free(qa);
    return out;
}

/* ── Python repr() of a str (ASCII scope) ── */
static char *sb_repr_str(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = strlen(s);
    char *out = malloc(n * 4 + 3);
    if (!out) return NULL;
    size_t k = 0;
    out[k++] = q;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == (unsigned char)q || c == '\\') { out[k++] = '\\'; out[k++] = (char)c; }
        else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
        else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; }
        else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            static const char hex[] = "0123456789abcdef";
            out[k++] = '\\'; out[k++] = 'x';
            out[k++] = hex[c >> 4]; out[k++] = hex[c & 15];
        } else out[k++] = (char)c;
    }
    out[k++] = q;
    out[k] = '\0';
    return out;
}

/* ── posixpath.normpath (CPython algorithm, verbatim) ── */
static char *sb_normpath(const char *path) {
    if (!path || !path[0]) { char *d = malloc(2); if (d) { d[0] = '.'; d[1] = 0; } return d; }
    int initial_slashes = 0;
    if (path[0] == '/') {
        initial_slashes = 1;
        if (path[1] == '/' && path[2] != '/') initial_slashes = 2;
    }
    /* split on '/', fold components */
    size_t n = strlen(path);
    char **comps = malloc(sizeof(char *) * (n + 1));
    size_t nc = 0;
    size_t start = 0;
    for (size_t i = 0; i <= n; i++) {
        if (i == n || path[i] == '/') {
            size_t len = i - start;
            char *c = malloc(len + 1);
            if (c) { memcpy(c, path + start, len); c[len] = 0; comps[nc++] = c; }
            start = i + 1;
        }
    }
    char **keep = malloc(sizeof(char *) * (nc + 1));
    size_t nk = 0;
    for (size_t i = 0; i < nc; i++) {
        const char *c = comps[i];
        if (c[0] == '\0' || strcmp(c, ".") == 0) continue;
        if (strcmp(c, "..") != 0 || (!initial_slashes && nk == 0)
            || (nk > 0 && strcmp(keep[nk - 1], "..") == 0)) {
            keep[nk++] = comps[i];
        } else if (nk > 0) {
            nk--;   /* popped entry stays owned by comps[] (freed below) */
        }
    }
    size_t total = (size_t)initial_slashes + 1;
    for (size_t i = 0; i < nk; i++) total += strlen(keep[i]) + 1;
    char *out = malloc(total + 1);
    if (out) {
        size_t k = 0;
        for (int i = 0; i < initial_slashes; i++) out[k++] = '/';
        for (size_t i = 0; i < nk; i++) {
            if (i) out[k++] = '/';
            size_t l = strlen(keep[i]);
            memcpy(out + k, keep[i], l);
            k += l;
        }
        out[k] = 0;
        if (k == 0) { out[0] = '.'; out[1] = 0; }
    }
    for (size_t i = 0; i < nc; i++) free(comps[i]);
    /* keep[] entries alias comps[] — freed exactly once above */
    free(comps); free(keep);
    return out;
}

/* ── scope / identity helpers ── */
static json_t *sb_scope_proj(const json_t *scope) {
    json_t *p = json_object();
    const json_t *t = json_obj_get(scope, "tenant");
    const json_t *a = json_obj_get(scope, "agent");
    json_set(p, "tenant", t ? json_copy(t) : json_null());
    json_set(p, "agent", a ? json_copy(a) : json_null());
    return p;
}
/* PoP: sb_sandbox_identity @ tools/environments/sandbox_scope.py:sandbox_identity */
static json_t *sb_sandbox_identity(const json_t *scope) {
    if (sb_is_none(scope)) return sb_attr_err("NoneType", "tenant");
    if (scope->type != JSON_OBJECT) return sb_attr_err(sb_type_name(scope), "tenant");
    if (!json_has(scope, "tenant")) return sb_attr_err("types.SimpleNamespace", "tenant");
    if (!json_has(scope, "agent")) return sb_attr_err("types.SimpleNamespace", "agent");
    return sb_scope_proj(scope);
}
/* PoP: sb_identity_key @ tools/environments/sandbox_scope.py:key */
static json_t *sb_identity_key(const char *tenant, const char *agent) {
    char *k = sb_key(tenant, agent);
    json_t *r = json_string(k ? k : "");
    free(k);
    return r;
}
/* PoP: sb_hosted_scope @ tools/environments/sandbox_scope.py:hosted_scope */
static json_t *sb_hosted_scope(const sb_seams_t *s) {
    json_t *r = sb_call(s, "current_scope", NULL);
    if (sb_is_raise(r)) return r;
    if (sb_is_none(r)) return r;
    if (r->type != JSON_OBJECT) {
        const char *tn = sb_type_name(r);
        json_free(r);
        return sb_attr_err(tn, "is_local");
    }
    const char *tenant = json_get_str(r, "tenant", "");
    if (strcmp(tenant, "local") == 0) { json_free(r); return json_null(); }
    return r;
}
/* PoP: sb_current_sandbox_key @ tools/environments/sandbox_scope.py:current_sandbox_key */
static json_t *sb_current_sandbox_key(const sb_seams_t *s) {
    json_t *hs = sb_hosted_scope(s);
    if (sb_is_raise(hs)) return hs;
    if (sb_is_none(hs)) { json_free(hs); return json_string(""); }
    json_t *id = sb_sandbox_identity(hs);
    json_free(hs);
    if (sb_is_raise(id)) return id;
    json_t *k = sb_identity_key(json_get_str(id, "tenant", ""), json_get_str(id, "agent", ""));
    json_free(id);
    return k;
}
/* PoP: sb_sandbox_visible @ tools/environments/sandbox_scope.py:sandbox_visible */
static json_t *sb_sandbox_visible(const sb_seams_t *s, const json_t *sandbox_key) {
    json_t *ck = sb_current_sandbox_key(s);
    if (sb_is_raise(ck)) return ck;
    bool eq = sandbox_key && sandbox_key->type == JSON_STRING
           && strcmp(sandbox_key->str_val, ck->str_val) == 0;
    json_free(ck);
    return json_bool(eq);
}
/* PoP: sb_require_identity @ tools/environments/sandbox_scope.py:require_identity */
static json_t *sb_require_identity(const sb_seams_t *s, const json_t *identity, const char *what) {
    json_t *hs = sb_hosted_scope(s);
    if (sb_is_raise(hs)) return hs;
    if (sb_is_none(hs)) {
        json_free(hs);
        return sb_raise_fmt("CrossAgentHandleRefused", "%s on a hosted sandbox needs a hosted scope bound", what);
    }
    json_t *si = sb_sandbox_identity(hs);
    if (sb_is_raise(si)) { json_free(hs); return si; }
    bool same = identity && identity->type == JSON_OBJECT
             && json_has(identity, "tenant") && json_has(identity, "agent")
             && strcmp(json_get_str(si, "tenant", ""), json_get_str(identity, "tenant", "")) == 0
             && strcmp(json_get_str(si, "agent", ""), json_get_str(identity, "agent", "")) == 0;
    json_free(si);
    if (!same) {
        json_free(hs);
        return sb_raise_fmt("CrossAgentHandleRefused", "%s names another agent's sandbox; refused", what);
    }
    return hs;
}

/* ── accepts_scope / hosted interface ── */
static const char *SB_REQUIRED_OPS[] = { "execute", "cleanup", "start_process", "attach_process", "stop_process", "stop_idle" };
static const char *SB_OPTIONAL_OPS[] = { "fetch_file", "fetch_realpath", "get_temp_dir" };
#define SB_NREQ 6
#define SB_NOPT 3
static bool sb_in_lifecycle(const char *name) {
    return strcmp(name, "cleanup") == 0 || strcmp(name, "stop_idle") == 0;
}
/* PoP: sb_accepts_scope @ tools/environments/sandbox_scope.py:accepts_scope */
static json_t *sb_accepts_scope(const json_t *method) {
    /* inspect.signature failures (TypeError/ValueError) -> False; modeled by
     * signature_error / non-object descriptors (no seam inside). */
    if (!method || method->type != JSON_OBJECT) return json_bool(false);
    if (sb_truthy(json_obj_get(method, "signature_error"))) return json_bool(false);
    const json_t *params = json_obj_get(method, "params");
    if (!params || params->type != JSON_ARRAY) return json_bool(false);
    for (size_t i = 0; i < params->c.count; i++) {
        const json_t *p = params->c.items[i];
        const char *nm = json_get_str(p, "name", "");
        const char *kind = json_get_str(p, "kind", "");
        if ((strcmp(nm, "scope") == 0 && strcmp(kind, "pos_only") != 0)
            || strcmp(kind, "var_kw") == 0)
            return json_bool(true);
    }
    return json_bool(false);
}
static bool sb_desc_callable(const json_t *d) {
    return d && d->type == JSON_OBJECT && sb_truthy(json_obj_get(d, "callable"));
}
/* PoP: sb_require_hosted_interface @ tools/environments/sandbox_scope.py:require_hosted_interface */
static json_t *sb_require_hosted_interface(const json_t *inner, const char *env_type) {
    const char *missing[SB_NREQ + SB_NOPT];
    size_t nmiss = 0;
    const char *blind[SB_NREQ + SB_NOPT];
    size_t nblind = 0;
    for (int i = 0; i < SB_NREQ; i++) {
        const json_t *d = (inner && inner->type == JSON_OBJECT) ? json_obj_get(inner, SB_REQUIRED_OPS[i]) : NULL;
        if (!sb_desc_callable(d)) missing[nmiss++] = SB_REQUIRED_OPS[i];
    }
    for (int i = 0; i < SB_NREQ + SB_NOPT; i++) {
        const char *op = i < SB_NREQ ? SB_REQUIRED_OPS[i] : SB_OPTIONAL_OPS[i - SB_NREQ];
        bool in_missing = false;
        for (size_t j = 0; j < nmiss; j++) if (strcmp(missing[j], op) == 0) { in_missing = true; break; }
        if (in_missing) continue;
        const json_t *d = json_obj_get(inner, op);
        if (sb_desc_callable(d)) {
            json_t *as = sb_accepts_scope(d);
            bool ok = as->type == JSON_BOOL && as->bool_val;
            json_free(as);
            if (!ok) blind[nblind++] = op;
        }
    }
    if (nmiss == 0 && nblind == 0) return json_null();
    char *rt = sb_repr_str(env_type ? env_type : "");
    char parts[1024];
    size_t k = 0;
    parts[0] = 0;
    if (nmiss) {
        k += (size_t)snprintf(parts + k, sizeof(parts) - k, "missing ");
        for (size_t j = 0; j < nmiss; j++)
            k += (size_t)snprintf(parts + k, sizeof(parts) - k, "%s%s", j ? ", " : "", missing[j]);
    }
    if (nblind) {
        if (nmiss) k += (size_t)snprintf(parts + k, sizeof(parts) - k, "; ");
        for (size_t j = 0; j < nblind; j++)
            k += (size_t)snprintf(parts + k, sizeof(parts) - k, "%s%s", j ? ", " : "", blind[j]);
        k += (size_t)snprintf(parts + k, sizeof(parts) - k, " must accept scope=");
    }
    json_t *r = sb_raise_fmt("HostedSandboxRequired",
        "terminal.backend %s does not implement the hosted operation interface: %s", rt, parts);
    free(rt);
    return r;
}

/* ── module predicates ── */
/* PoP: sb_sandbox_lexical_path @ tools/environments/sandbox_scope.py:sandbox_lexical_path */
static json_t *sb_sandbox_lexical_path(const sb_seams_t *s, const json_t *path) {
    json_t *hs = sb_hosted_scope(s);
    if (sb_is_raise(hs)) return hs;
    if (sb_is_none(hs)) { json_free(hs); return json_null(); }
    json_free(hs);
    const char *p = NULL;
    if (path && path->type == JSON_STRING) p = path->str_val;
    else if (path && path->type == JSON_OBJECT && json_has(path, "__fspath__")) {
        const json_t *f = json_obj_get(path, "__fspath__");
        if (f && f->type == JSON_STRING) p = f->str_val;
        else return sb_raise_fmt("TypeError", "expected str, bytes or os.PathLike object, not %s", sb_type_name(path));
    } else {
        return sb_raise_fmt("TypeError", "expected str, bytes or os.PathLike object, not %s", sb_type_name(path));
    }
    char *norm = sb_normpath((p && p[0]) ? p : ".");
    json_t *r = json_string(norm ? norm : ".");
    free(norm);
    return r;
}
/* PoP: sb_producer_fields @ tools/environments/sandbox_scope.py:producer_fields */
static json_t *sb_producer_fields(const json_t *sandbox_key) {
    if (!sb_truthy(sandbox_key)) return json_object();
    json_t *r = json_object();
    json_set(r, "sandbox_key", json_copy(sandbox_key));
    return r;
}
/* str(event.get("sandbox_key") or "") — domain: str/int/bool truthy values */
static json_t *sb_str_or_empty(const json_t *v) {
    if (!sb_truthy(v)) return json_string("");
    if (v->type == JSON_STRING) return json_string(v->str_val);
    if (v->type == JSON_BOOL) return json_string(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        return json_string(buf);
    }
    /* float/container str() projections are out of domain (header) */
    return json_string("");
}
/* PoP: sb_event_visible @ tools/environments/sandbox_scope.py:event_visible */
static json_t *sb_event_visible(const sb_seams_t *s, const json_t *event) {
    if (!event || event->type != JSON_OBJECT)
        return sb_attr_err(sb_type_name(event), "get");
    const json_t *sk = json_obj_get(event, "sandbox_key");
    json_t *key = sb_str_or_empty(sk);
    json_t *r = sb_sandbox_visible(s, key);
    json_free(key);
    return r;
}

/* ── admit / create ── */
/* PoP: sb_admit_env @ tools/environments/sandbox_scope.py:admit_env */
static json_t *sb_admit_env(const sb_seams_t *s, const json_t *env) {
    json_t *hs = sb_hosted_scope(s);
    if (sb_is_raise(hs)) return hs;
    if (sb_is_none(env) || sb_is_none(hs)) {
        json_free(hs);
        return env ? json_copy(env) : json_null();
    }
    json_t *si = sb_sandbox_identity(hs);
    json_free(hs);
    if (sb_is_raise(si)) return si;
    const json_t *sbx = (env && env->type == JSON_OBJECT) ? json_obj_get(env, "sandbox") : NULL;
    bool ok = env && env->type == JSON_OBJECT
           && sb_truthy(json_obj_get(env, "__scoped_env__"))
           && sbx && sbx->type == JSON_OBJECT
           && strcmp(json_get_str(sbx, "tenant", ""), json_get_str(si, "tenant", "")) == 0
           && strcmp(json_get_str(sbx, "agent", ""), json_get_str(si, "agent", "")) == 0;
    json_free(si);
    if (!ok)
        return sb_raise("CrossAgentHandleRefused",
            "a cached environment that is not this agent's sandbox was reached; refused");
    return json_copy(env);
}
/* PoP: sb_create_hosted_environment @ tools/environments/sandbox_scope.py:create_hosted_environment */
static json_t *sb_create_hosted_environment(const sb_seams_t *s, const char *env_type,
                                            const json_t *provider, const json_t *kwargs) {
    json_t *hs = sb_hosted_scope(s);
    if (sb_is_raise(hs)) return hs;
    if (sb_is_none(hs)) {
        json_free(hs);
        return sb_raise("CrossAgentHandleRefused", "a hosted sandbox can only be created inside a hosted scope");
    }
    const json_t *serves = (provider && provider->type == JSON_OBJECT)
                         ? json_obj_get(provider, "serves_hosted_scopes") : NULL;
    if (!(serves && serves->type == JSON_BOOL && serves->bool_val)) {
        char *rt = sb_repr_str(env_type ? env_type : "");
        json_t *r = sb_raise_fmt("HostedSandboxRequired",
            "terminal.backend %s cannot serve a hosted scope: tools for a hosted agent run only in "
            "a remote sandbox backend that declares serves_hosted_scopes", rt);
        free(rt);
        json_free(hs);
        return r;
    }
    json_t *id = sb_sandbox_identity(hs);
    if (sb_is_raise(id)) { json_free(hs); return id; }
    json_t *kw = kwargs ? json_copy(kwargs) : json_object();
    json_t *key = sb_identity_key(json_get_str(id, "tenant", ""), json_get_str(id, "agent", ""));
    json_set(kw, "task_id", key);
    /* kwarg collision at call binding (before the seam fires); the provider
     * double's __module__ is pinned to sandbox_provider on both sides. */
    for (size_t i = 0; i < kw->c.count; i++) {
        const char *k = kw->c.keys[i];
        if (strcmp(k, "scope") == 0 || strcmp(k, "sandbox") == 0) {
            json_t *r = sb_raise_fmt("TypeError",
                "sandbox_provider.create_environment() got multiple values for keyword argument '%s'", k);
            json_free(kw); json_free(id); json_free(hs);
            return r;
        }
    }
    json_t *a = json_object();
    json_set(a, "scope", sb_scope_proj(hs));
    json_set(a, "sandbox", json_copy(id));
    for (size_t i = 0; i < kw->c.count; i++)
        json_set(a, kw->c.keys[i], json_copy(kw->c.items[i]));
    json_free(kw);
    json_t *r = sb_call(s, "provider.create_environment", a);
    if (sb_is_raise(r)) { json_free(id); json_free(hs); return r; }
    const json_t *inner = sb_is_none(r) ? NULL : json_obj_get(r, "inner");
    const char *inner_repr = sb_is_none(r) ? "" : json_get_str(r, "inner_repr", "");
    json_t *chk = sb_require_hosted_interface(inner, env_type);
    if (sb_is_raise(chk)) { json_free(r); json_free(id); json_free(hs); return chk; }
    json_free(chk);
    json_t *env = json_object();
    json_set(env, "__scoped_env__", json_bool(true));
    json_set(env, "inner", inner ? json_copy(inner) : json_object());
    json_set(env, "inner_repr", json_string(inner_repr));
    json_set(env, "sandbox", id);
    json_set(env, "owner", sb_scope_proj(hs));
    json_free(r); json_free(hs);
    return env;
}

/* ── ScopedEnvironment ── */
static const json_t *sb_env_inner(const json_t *env) {
    const json_t *in = json_obj_get(env, "inner");
    return (in && in->type == JSON_OBJECT) ? in : NULL;
}
static const json_t *sb_env_sandbox(const json_t *env) { return json_obj_get(env, "sandbox"); }
/* inner attribute load: absent -> AttributeError; returns borrowed desc */
static const json_t *sb_inner_get(const json_t *env, const char *name, json_t **err) {
    const json_t *inner = sb_env_inner(env);
    const json_t *d = inner ? json_obj_get(inner, name) : NULL;
    if (!d) { *err = sb_attr_err("types.SimpleNamespace", name); return NULL; }
    *err = NULL;
    return d;
}
static json_t *sb_not_callable_err(const json_t *d) {
    const json_t *v = json_obj_get(d, "value");
    return sb_raise_fmt("TypeError", "'%s' object is not callable", sb_type_name(v));
}
/* PoP: sb_env_init @ tools/environments/sandbox_scope.py:__init__ */
static json_t *sb_env_init(const json_t *inner, const char *inner_repr,
                           const json_t *identity, const json_t *owner) {
    if (!sb_is_none(owner)) {
        json_t *oi = sb_sandbox_identity(owner);
        if (sb_is_raise(oi)) return oi;
        bool same = identity && identity->type == JSON_OBJECT
                 && strcmp(json_get_str(oi, "tenant", ""), json_get_str(identity, "tenant", "")) == 0
                 && strcmp(json_get_str(oi, "agent", ""), json_get_str(identity, "agent", "")) == 0;
        json_free(oi);
        if (!same)
            return sb_raise("CrossAgentHandleRefused", "a sandbox's owning scope must name that sandbox's agent");
    }
    json_t *env = json_object();
    json_set(env, "__scoped_env__", json_bool(true));
    json_set(env, "inner", inner ? json_copy(inner) : json_object());
    json_set(env, "inner_repr", json_string(inner_repr ? inner_repr : ""));
    json_set(env, "sandbox", identity ? json_copy(identity) : json_null());
    json_set(env, "owner", sb_is_none(owner) ? json_null() : sb_scope_proj(owner));
    return env;
}
/* PoP: sb_env_getattr @ tools/environments/sandbox_scope.py:__getattr__ */
static json_t *sb_env_getattr(const json_t *env, const char *name) {
    json_t *err = NULL;
    const json_t *d = sb_inner_get(env, name, &err);
    if (err) return err;
    if (sb_desc_callable(d)) {
        json_t *r = json_object();
        json_set(r, "__wrapped__", json_string(sb_in_lifecycle(name) ? "lifecycle" : "guarded"));
        json_set(r, "name", json_string(name));
        const json_t *sbx = sb_env_sandbox(env);
        json_set(r, "identity", sbx ? sb_scope_proj(sbx) : json_null());
        return r;
    }
    const json_t *v = json_obj_get(d, "value");
    return v ? json_copy(v) : json_null();
}
/* PoP: sb_env_setattr @ tools/environments/sandbox_scope.py:__setattr__ */
static json_t *sb_env_setattr(json_t *env, const char *name, const json_t *value) {
    const json_t *inner = sb_env_inner(env);
    if (inner) {
        json_t *d = json_object();
        json_set(d, "callable", json_bool(false));
        json_set(d, "value", value ? json_copy(value) : json_null());
        json_set((json_t *)inner, name, d);
    }
    return json_null();
}
/* PoP: sb_env_repr @ tools/environments/sandbox_scope.py:__repr__ */
static json_t *sb_env_repr(const json_t *env) {
    const json_t *sbx = sb_env_sandbox(env);
    char *key = sb_key(json_get_str(sbx, "tenant", ""), json_get_str(sbx, "agent", ""));
    const char *ir = json_get_str(env, "inner_repr", "");
    size_t n = strlen(ir) + strlen(key) + 32;
    char *buf = malloc(n);
    if (buf) snprintf(buf, n, "ScopedEnvironment(%s, %s)", ir, key);
    json_t *r = json_string(buf ? buf : "");
    free(buf); free(key);
    return r;
}
/* PoP: sb_env_own_name @ tools/environments/sandbox_scope.py:_own_name */
static json_t *sb_env_own_name(const json_t *env, const json_t *ref, const char *what) {
    if (ref && ref->type == JSON_OBJECT && sb_truthy(json_obj_get(ref, "__process_ref__"))) {
        const json_t *sbx = sb_env_sandbox(env);
        char *key = sb_key(json_get_str(sbx, "tenant", ""), json_get_str(sbx, "agent", ""));
        const char *rs = json_get_str(ref, "sandbox", "");
        bool same = strcmp(rs, key) == 0;
        free(key);
        if (!same)
            return sb_raise_fmt("CrossAgentHandleRefused", "%s names a process in another agent's sandbox; refused", what);
        const json_t *nm = json_obj_get(ref, "name");
        return nm ? json_copy(nm) : json_null();
    }
    return ref ? json_copy(ref) : json_null();
}
/* PoP: sb_env_guard_stream @ tools/environments/sandbox_scope.py:_guard_stream */
static json_t *sb_env_guard_stream(const json_t *env, const json_t *stream, const json_t *name) {
    if (sb_is_none(stream)) return sb_attr_err("NoneType", "stdin");
    if (stream->type != JSON_OBJECT) return sb_attr_err(sb_type_name(stream), "stdin");
    if (!json_has(stream, "stdin")) return sb_attr_err("types.SimpleNamespace", "stdin");
    if (!json_has(stream, "stdout")) return sb_attr_err("types.SimpleNamespace", "stdout");
    if (!json_has(stream, "poll")) return sb_attr_err("types.SimpleNamespace", "poll");
    const json_t *sbx = sb_env_sandbox(env);
    json_t *idp = sbx ? sb_scope_proj(sbx) : json_null();
    char *key = sb_key(json_get_str(sbx, "tenant", ""), json_get_str(sbx, "agent", ""));
    json_t *r = json_object();
    json_t *ref = json_object();
    json_set(ref, "sandbox", json_string(key ? key : ""));
    json_set(ref, "name", name ? json_copy(name) : json_null());
    json_set(r, "ref", ref);
    json_t *sin = json_object();
    json_set(sin, "__scoped_pipe__", json_copy(idp));
    json_set(r, "stdin", sin);
    json_t *sout = json_object();
    json_set(sout, "__scoped_pipe__", json_copy(idp));
    json_set(r, "stdout", sout);
    json_t *poll = json_object();
    json_set(poll, "__scoped_call__", json_string("poll"));
    json_set(poll, "identity", idp);
    json_set(r, "poll", poll);
    free(key);
    return r;
}
/* guarded call core: require_identity + scope injection + inner.<name> seam */
static json_t *sb_guarded_call(const sb_seams_t *s, const json_t *env, const char *name,
                               const json_t *method, const json_t *args, const json_t *kwargs) {
    json_t *scope = sb_require_identity(s, sb_env_sandbox(env), name);
    if (sb_is_raise(scope)) return scope;
    json_t *kw = kwargs ? json_copy(kwargs) : json_object();
    json_t *as = sb_accepts_scope(method);
    bool accepts = as->type == JSON_BOOL && as->bool_val;
    json_free(as);
    if (accepts) json_set(kw, "scope", sb_scope_proj(scope));
    json_free(scope);
    json_t *a = json_object();
    json_set(a, "args", args ? json_copy(args) : json_array());
    json_set(a, "kwargs", kw);
    char evname[256];
    snprintf(evname, sizeof(evname), "inner.%s", name);
    return sb_call(s, evname, a);
}
/* PoP: sb_env_guarded @ tools/environments/sandbox_scope.py:_guarded */
static json_t *sb_env_guarded(const sb_seams_t *s, const json_t *env, const char *name,
                              const json_t *method, const json_t *args, const json_t *kwargs) {
    return sb_guarded_call(s, env, name, method, args, kwargs);
}
/* PoP: sb_env_lifecycle @ tools/environments/sandbox_scope.py:_lifecycle */
static json_t *sb_env_lifecycle(const sb_seams_t *s, const json_t *env, const char *name,
                                const json_t *method, const json_t *args, const json_t *kwargs) {
    json_t *hs = sb_hosted_scope(s);
    if (sb_is_raise(hs)) return hs;
    if (!sb_is_none(hs)) {
        json_free(hs);
        return sb_guarded_call(s, env, name, method, args, kwargs);
    }
    json_free(hs);
    const json_t *owner = json_obj_get(env, "owner");
    if (sb_is_none(owner))
        return sb_raise_fmt("CrossAgentHandleRefused", "%s on a hosted sandbox with no owning scope; refused", name);
    json_t *a = json_object();
    json_set(a, "owner", sb_scope_proj(owner));
    json_t *bs = sb_call(s, "bind_state", a);
    if (sb_is_raise(bs)) return bs;
    json_free(bs);   /* CM enter/exit are silent no-op doubles on both sides */
    return sb_guarded_call(s, env, name, method, args, kwargs);
}
/* PoP: sb_env_start_process @ tools/environments/sandbox_scope.py:start_process */
static json_t *sb_env_start_process(const sb_seams_t *s, const json_t *env, const char *name,
                                    const json_t *argv, const json_t *child_env) {
    json_t *scope = sb_require_identity(s, sb_env_sandbox(env), "start_process");
    if (sb_is_raise(scope)) return scope;
    json_t *err = NULL;
    const json_t *d = sb_inner_get(env, "start_process", &err);
    if (err) { json_free(scope); return err; }
    if (!sb_desc_callable(d)) { json_free(scope); return sb_not_callable_err(d); }
    json_t *a = json_object();
    json_set(a, "name", json_string(name));
    json_set(a, "argv", argv ? json_copy(argv) : json_array());
    if (!sb_is_none(child_env)) json_set(a, "child_env", json_copy(child_env));
    json_set(a, "scope", sb_scope_proj(scope));
    json_free(scope);
    json_t *stream = sb_call(s, "inner.start_process", a);
    if (sb_is_raise(stream)) return stream;
    json_t *nm = json_string(name);
    json_t *r = sb_env_guard_stream(env, stream, nm);
    json_free(nm); json_free(stream);
    return r;
}
/* attach/stop: attribute load BEFORE the require_identity kwarg seam */
static json_t *sb_env_process_op(const sb_seams_t *s, const json_t *env, const json_t *ref,
                                 const char *op) {
    json_t *name = sb_env_own_name(env, ref, op);
    if (sb_is_raise(name)) return name;
    json_t *err = NULL;
    const json_t *d = sb_inner_get(env, op, &err);
    if (err) { json_free(name); return err; }
    if (!sb_desc_callable(d)) { json_free(name); return sb_not_callable_err(d); }
    json_t *scope = sb_require_identity(s, sb_env_sandbox(env), op);
    if (sb_is_raise(scope)) { json_free(name); return scope; }
    json_t *a = json_object();
    json_set(a, "name", name);
    json_set(a, "scope", sb_scope_proj(scope));
    json_free(scope);
    char evname[256];
    snprintf(evname, sizeof(evname), "inner.%s", op);
    json_t *r = sb_call(s, evname, a);
    if (sb_is_raise(r)) return r;
    if (strcmp(op, "stop_process") == 0) return r;
    if (sb_is_none(r)) return r;   /* attach: None -> None */
    json_t *g = sb_env_guard_stream(env, r, name);
    json_free(r);
    return g;
}
/* PoP: sb_env_attach_process @ tools/environments/sandbox_scope.py:attach_process */
static json_t *sb_env_attach_process(const sb_seams_t *s, const json_t *env, const json_t *ref) {
    return sb_env_process_op(s, env, ref, "attach_process");
}
/* PoP: sb_env_stop_process @ tools/environments/sandbox_scope.py:stop_process */
static json_t *sb_env_stop_process(const sb_seams_t *s, const json_t *env, const json_t *ref) {
    return sb_env_process_op(s, env, ref, "stop_process");
}

/* ── _scoped_call ── */
/* PoP: sb_scoped_call @ tools/environments/sandbox_scope.py:_scoped_call */
static json_t *sb_scoped_call(const sb_seams_t *s, const json_t *identity, const char *what,
                              const json_t *args, const json_t *kwargs) {
    json_t *scope = sb_require_identity(s, identity, what);
    if (sb_is_raise(scope)) return scope;
    json_free(scope);
    json_t *a = json_object();
    json_set(a, "args", args ? json_copy(args) : json_array());
    json_set(a, "kwargs", kwargs ? json_copy(kwargs) : json_object());
    return sb_call(s, "fn", a);
}

/* ── ScopedPipe ── */
static const json_t *sb_pipe_desc(const json_t *st) {
    const json_t *p = json_obj_get(st, "pipe");
    return (p && p->type == JSON_OBJECT) ? p : NULL;
}
static bool sb_pipe_has(const json_t *st, const char *op) {
    const json_t *p = sb_pipe_desc(st);
    const json_t *m = p ? json_obj_get(p, "methods") : NULL;
    if (!m || m->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < m->c.count; i++) {
        const json_t *n = m->c.items[i];
        if (n && n->type == JSON_STRING && strcmp(n->str_val, op) == 0) return true;
    }
    return false;
}
/* PoP: sb_pipe_op @ tools/environments/sandbox_scope.py:_op */
static json_t *sb_pipe_op(const sb_seams_t *s, const json_t *st, const char *what) {
    char buf[256];
    snprintf(buf, sizeof(buf), "stream %s", what);
    json_t *scope = sb_require_identity(s, json_obj_get(st, "identity"), buf);
    if (sb_is_raise(scope)) return scope;
    json_free(scope);
    const json_t *p = sb_pipe_desc(st);
    return p ? json_copy(p) : json_null();
}
/* _op + method presence + pipe.<op> seam */
static json_t *sb_pipe_call(const sb_seams_t *s, const json_t *st, const char *what,
                            const char *op, json_t *args) {
    json_t *r = sb_pipe_op(s, st, what);
    if (sb_is_raise(r)) { json_free(args); return r; }
    json_free(r);
    if (!sb_pipe_has(st, op)) {
        json_free(args);
        return sb_attr_err("types.SimpleNamespace", op);
    }
    char evname[256];
    snprintf(evname, sizeof(evname), "pipe.%s", op);
    return sb_call(s, evname, args);
}
/* PoP: sb_pipe_read @ tools/environments/sandbox_scope.py:read */
static json_t *sb_pipe_read(const sb_seams_t *s, const json_t *st, double size) {
    json_t *a = json_object();
    json_set(a, "size", json_number(size));
    return sb_pipe_call(s, st, "read", "read", a);
}
/* PoP: sb_pipe_read1 @ tools/environments/sandbox_scope.py:read1 */
static json_t *sb_pipe_read1(const sb_seams_t *s, const json_t *st, double size) {
    const char *op = sb_pipe_has(st, "read1") ? "read1" : "read";
    json_t *a = json_object();
    json_set(a, "size", json_number(size));
    return sb_pipe_call(s, st, "read1", op, a);
}
/* PoP: sb_pipe_readline @ tools/environments/sandbox_scope.py:readline */
static json_t *sb_pipe_readline(const sb_seams_t *s, const json_t *st, double size) {
    json_t *a = json_object();
    json_set(a, "size", json_number(size));
    return sb_pipe_call(s, st, "readline", "readline", a);
}
/* PoP: sb_pipe_readlines @ tools/environments/sandbox_scope.py:readlines */
static json_t *sb_pipe_readlines(const sb_seams_t *s, const json_t *st, double hint) {
    json_t *a = json_object();
    json_set(a, "hint", json_number(hint));
    return sb_pipe_call(s, st, "readlines", "readlines", a);
}
/* PoP: sb_pipe_readinto @ tools/environments/sandbox_scope.py:readinto */
static json_t *sb_pipe_readinto(const sb_seams_t *s, const json_t *st, const json_t *buffer) {
    json_t *a = json_object();
    json_set(a, "buffer", buffer ? json_copy(buffer) : json_null());
    return sb_pipe_call(s, st, "readinto", "readinto", a);
}
/* PoP: sb_pipe_write @ tools/environments/sandbox_scope.py:write */
static json_t *sb_pipe_write(const sb_seams_t *s, const json_t *st, const json_t *data) {
    json_t *a = json_object();
    json_set(a, "data", data ? json_copy(data) : json_null());
    return sb_pipe_call(s, st, "write", "write", a);
}
/* PoP: sb_pipe_writelines @ tools/environments/sandbox_scope.py:writelines */
static json_t *sb_pipe_writelines(const sb_seams_t *s, const json_t *st, const json_t *lines) {
    json_t *a = json_object();
    json_set(a, "lines", lines ? json_copy(lines) : json_null());
    return sb_pipe_call(s, st, "writelines", "writelines", a);
}
/* PoP: sb_pipe_flush @ tools/environments/sandbox_scope.py:flush */
static json_t *sb_pipe_flush(const sb_seams_t *s, const json_t *st) {
    return sb_pipe_call(s, st, "flush", "flush", NULL);
}
/* PoP: sb_pipe_close @ tools/environments/sandbox_scope.py:close */
static json_t *sb_pipe_close(const sb_seams_t *s, const json_t *st) {
    return sb_pipe_call(s, st, "close", "close", NULL);
}
/* PoP: sb_pipe_closed @ tools/environments/sandbox_scope.py:closed */
static json_t *sb_pipe_closed(const sb_seams_t *s, const json_t *st) {
    json_t *r = sb_pipe_op(s, st, "closed");
    if (sb_is_raise(r)) return r;
    json_free(r);
    const json_t *p = sb_pipe_desc(st);
    if (!p || !json_has(p, "closed")) return sb_attr_err("types.SimpleNamespace", "closed");
    return json_bool(sb_truthy(json_obj_get(p, "closed")));
}
/* PoP: sb_pipe_readable @ tools/environments/sandbox_scope.py:readable */
static json_t *sb_pipe_readable(const sb_seams_t *s, const json_t *st) {
    json_t *r = sb_pipe_call(s, st, "readable", "readable", NULL);
    if (sb_is_raise(r)) return r;
    bool v = sb_truthy(r);
    json_free(r);
    return json_bool(v);
}
/* PoP: sb_pipe_writable @ tools/environments/sandbox_scope.py:writable */
static json_t *sb_pipe_writable(const sb_seams_t *s, const json_t *st) {
    json_t *r = sb_pipe_call(s, st, "writable", "writable", NULL);
    if (sb_is_raise(r)) return r;
    bool v = sb_truthy(r);
    json_free(r);
    return json_bool(v);
}
/* PoP: sb_pipe_setattr @ tools/environments/sandbox_scope.py:__setattr__ */
static json_t *sb_pipe_setattr(const char *name) {
    char *rt = sb_repr_str(name ? name : "");
    json_t *r = sb_raise_fmt("AttributeError", "a hosted process pipe has no settable attribute %s", rt);
    free(rt);
    return r;
}
/* PoP: sb_pipe_iter @ tools/environments/sandbox_scope.py:__iter__ */
static json_t *sb_pipe_iter(const sb_seams_t *s, const json_t *st) {
    json_t *r = sb_pipe_op(s, st, "iteration");
    if (sb_is_raise(r)) return r;
    json_free(r);
    json_t *out = json_object();
    const json_t *id = json_obj_get(st, "identity");
    json_set(out, "__scoped_pipe__", id ? sb_scope_proj(id) : json_null());
    return out;
}
/* PoP: sb_pipe_next @ tools/environments/sandbox_scope.py:__next__ */
static json_t *sb_pipe_next(const sb_seams_t *s, const json_t *st) {
    json_t *r = sb_pipe_op(s, st, "iteration");
    if (sb_is_raise(r)) return r;
    json_free(r);
    if (!sb_pipe_has(st, "readline")) return sb_attr_err("types.SimpleNamespace", "readline");
    json_t *a = json_object();
    json_set(a, "size", json_number(-1));   /* live __next__ calls readline() */
    json_t *line = sb_call(s, "pipe.readline", a);
    if (sb_is_raise(line)) return line;
    if (!sb_truthy(line)) { json_free(line); return sb_raise("StopIteration", ""); }
    return line;
}
/* PoP: sb_pipe_enter @ tools/environments/sandbox_scope.py:__enter__ */
static json_t *sb_pipe_enter(const sb_seams_t *s, const json_t *st) {
    json_t *r = sb_pipe_op(s, st, "context");
    if (sb_is_raise(r)) return r;
    json_free(r);
    json_t *out = json_object();
    const json_t *id = json_obj_get(st, "identity");
    json_set(out, "__scoped_pipe__", id ? sb_scope_proj(id) : json_null());
    return out;
}
/* PoP: sb_pipe_exit @ tools/environments/sandbox_scope.py:__exit__ */
static json_t *sb_pipe_exit(const sb_seams_t *s, const json_t *st) {
    json_t *r = sb_pipe_close(s, st);
    if (sb_is_raise(r)) return r;
    json_free(r);
    return json_null();
}
/* PoP: sb_pipe_repr @ tools/environments/sandbox_scope.py:__repr__ */
static json_t *sb_pipe_repr(const json_t *st) {
    const json_t *id = json_obj_get(st, "identity");
    char *key = sb_key(json_get_str(id, "tenant", ""), json_get_str(id, "agent", ""));
    size_t n = strlen(key) + 16;
    char *buf = malloc(n);
    if (buf) snprintf(buf, n, "ScopedPipe(%s)", key);
    json_t *r = json_string(buf ? buf : "");
    free(buf); free(key);
    return r;
}

#endif /* SANDBOX_FLOW_C */
