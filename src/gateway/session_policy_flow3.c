/*
 * session_policy_flow3.c -- gateway/session_policy.py FLOW cores, third wave
 * (universal-seam convention, twin of session_policy_flow2).
 *
 * Ports 16 UNCLAIMED gateway/session_policy.py functions (PoP-grep verified
 * across src/ + include/; the 29 flow1+flow2 claims are SEAMED, never
 * re-claimed). Cluster: stored-receipt adoption, the launch-secret lifecycle,
 * and the turn-result / scope accessors:
 *   refusal:   _refuse, _policy_refused
 *   receipt:   _adopt_stored_policy, _stored_policy, _check_stored_surface
 *   secrets:   _extract_config_secrets, bind_launch_key, rebind_launch_key,
 *              _secret_refs, release_launch_secrets
 *   skills:    render_launch_skills
 *   turn:      policy_for_source, workspace_unavailable_result,
 *              refused_turn_result, release_session_workspaces,
 *              policy_turn_toolsets
 *
 * Every live I/O (foreign-module helpers, the policy.current_toolsets read,
 * runner/adapter methods, persistence scope, i18n / failure-copy builders,
 * hmac / hashlib, logging, the owner_scope context manager) enters through
 * sp3_seams_t.call(ctx, name, args); args is OWNED by sp3_call (freed after the
 * borrowed call); results are NEW json_t with NULL == Python None and
 * {"__raise__":K,"msg":M} == a raised exception. BaseException kinds
 * (SystemExit/KeyboardInterrupt/CancelledError/GeneratorExit) pass through
 * every `except Exception` site; all other kinds are Exception-family (the
 * fixture domain scripts only Exception subclasses + the base trio, so no
 * partial exception-parent table is needed). Fail-closed: the harness delivers
 * a RuntimeError raise spec for an unscripted seam.
 *
 * Claimed siblings are seams: launch_skills, restore_policy, _key_slot,
 * unbind_turn_workspace. Ported siblings compose inline (_policy_refused into
 * _stored_policy/_check_stored_surface; _adopt_stored_policy into _stored_policy;
 * _secret_refs into release_launch_secrets; workspace_unavailable_result into
 * refused_turn_result).
 *
 * Seam name table (name -> live call):
 *   logger.warning                 logging.getLogger(__name__).warning / logger.warning
 *   is_forward_policy              gateway.session_a2a.is_forward_policy(policy)
 *   _is_secret_payload_key         agent.credential_persistence._is_secret_payload_key(key)
 *   launch_skills                  session_policy.launch_skills(params)        [claimed]
 *   build_preloaded_skills_prompt  agent.skill_commands.build_preloaded_skills_prompt(skills, task_id=)
 *   format_missing_skills          agent.skill_commands.format_missing_skills(missing)
 *   bind_editor_mcp                gateway.session_local_mcp.bind_editor_mcp(authority, sid, policy, editor_secret)
 *   same_create_retry              session_rules.local_create.same_create_retry(owner, request_json, client_request)
 *   config_reference               gateway.session_policy_credentials.config_reference(authority, sid, policy, config_secrets)
 *   fingerprint_secret_value       agent.credential_persistence.fingerprint_secret_value(api_key)
 *   hmac.compare_digest            hmac.compare_digest(a, b)
 *   authority_persistence          gateway.session_persistence.authority_persistence(authority)
 *   restore_policy                 session_policy.restore_policy(data)         [claimed]
 *   _key_slot                      session_policy._key_slot(credential_ref)    [claimed]
 *   local_adapter_map              gateway.session_local_recovery.local_adapter_map(authority)
 *   hashlib.sha256                 hashlib.sha256(data).hexdigest()  (ret == hexdigest string)
 *   runner._adapter_for_source     runner._adapter_for_source(source)
 *   adapter.authorize_source       adapter.authorize_source(source)
 *   t                              agent.i18n.t(key, path=)
 *   stamp_failure                  agent.turn_failure_copy.stamp_failure(result, kind, flag)
 *   launch_credentials_copy        agent.turn_failure_copy.launch_credentials_copy(exc)
 *   build_error_surface_from_result agent.error_surface.build_error_surface_from_result(result)
 *   owner_scope.__enter__          gateway.session_authorities.owner_scope(authority) CM enter
 *   owner_scope.__exit__           owner_scope CM exit (args.exc == pending raise spec or null)
 *   unbind_turn_workspace          session_policy.unbind_turn_workspace(task_id) [claimed]
 *   policy.current_toolsets        policy.current_toolsets(config)
 *   onboarding_chat_toolsets       agent.onboarding_session.onboarding_chat_toolsets(toolsets, store, sid, persistence=)
 *
 * Modeling notes (differential oracle tests/oracle_session_policy_flow3.py):
 *  - Mutable STATE: bind_launch_key / rebind_launch_key / release_launch_secrets
 *    mutate in.authority (out.state.authority); _extract_config_secrets mutates
 *    in.value + in.private (out.state.{value,private}). All other funcs echo
 *    out.state = null.
 *  - Secret maps (config_secrets / private / the authority _local_config_secrets
 *    values) are keyed by tuple paths in live; both sides encode a path as the
 *    compact JSON array string sp3_path_key (e.g. ["model","api_key"], [null]).
 *    PRIVATE_KEY=('editor_mcp',) -> '["editor_mcp"]'. The encoding is bijective
 *    on the fixture domain, so dict equality / pop / delete agree.
 *  - LocalSessionPolicy is modeled as a JSON object of its 20 dataclass fields;
 *    sp3_policy_norm applies the dataclass defaults for absent optional fields
 *    (the live ctor / dataclasses.replace do). _stored_policy / bind_launch_key
 *    return that full field object; the oracle denorms the real dataclass to it.
 *  - _ADOPTED_KEY_SETS dedup is reset per case on BOTH sides (the harness calls
 *    sp3_adopt_reset; the oracle clears NS["_ADOPTED_KEY_SETS"]), so the warning
 *    fires iff the unknown-key set is non-empty within a case.
 *  - str() positions accept str/int/bool/null ONLY; .strip()/.lower() are ASCII.
 *    json.loads positions accept STRICT-valid JSON strings only. Paths are POSIX.
 *  - AuthorityRefusal refusals carry {__raise__, msg(=reason), reason, field,
 *    detail}; field/detail run the live _one_line normalization (80/160 bounds,
 *    empty -> null). Scripted raise specs propagate VERBATIM.
 *  - Set-iteration domains: _secret_refs scripts <=1 truthy credential_ref and
 *    release_launch_secrets <=1 retired sid, so the ordered seam trace is
 *    deterministic (the live sets iterate in hash order).
 *  - isinstance(adapter, LocalSessionAdapter): a dict runner._adapter_for_source
 *    result models a LocalSessionAdapter; any other result models a foreign one.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include "libjson/json.h"

typedef struct sp3_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} sp3_seams_t;

/* -- universal-seam helpers -- */
static json_t *sp3_call(const sp3_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool sp3_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *sp3_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING && k->str_val) ? k->str_val : "";
}
static json_t *sp3_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static bool sp3_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
static bool sp3_truth(const json_t *v) {
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
static const json_t *sp3_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *sp3_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool sp3_is_null_node(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool sp3_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool sp3_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool sp3_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static json_t *sp3_args(void) { return json_object(); }
static void sp3_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void sp3_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void sp3_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const char *sp3_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (isfinite(d) && d == (double)(long long)d) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}
static bool sp3_scalar_eq(const json_t *a, const json_t *b) {
    bool an = sp3_is_null_node(a), bn = sp3_is_null_node(b);
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
/* Deep JSON equality (objects unordered by key, arrays ordered). */
static bool sp3_deep_eq(const json_t *a, const json_t *b) {
    if (sp3_is_null_node(a) || sp3_is_null_node(b)) return sp3_is_null_node(a) && sp3_is_null_node(b);
    if (a->type != b->type) {
        bool anum = (a->type == JSON_NUMBER || a->type == JSON_BOOL);
        bool bnum = (b->type == JSON_NUMBER || b->type == JSON_BOOL);
        if (anum && bnum) return sp3_scalar_eq(a, b);
        return false;
    }
    switch (a->type) {
        case JSON_BOOL: case JSON_NUMBER: case JSON_STRING: return sp3_scalar_eq(a, b);
        case JSON_ARRAY: {
            if (json_len(a) != json_len(b)) return false;
            for (size_t i = 0; i < json_len(a); i++)
                if (!sp3_deep_eq(json_get(a, i), json_get(b, i))) return false;
            return true;
        }
        case JSON_OBJECT: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !sp3_deep_eq(a->c.items[i], bv)) return false;
            }
            return true;
        }
        default: return false;
    }
}
static char *sp3_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (isfinite(d) && d == (double)(long long)d && d < 9.2e18 && d > -9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                snprintf(buf, sizeof(buf), "%.17g", d);
            return strdup(buf);
        }
        default: { char *s = json_serialize(v); return s ? s : strdup(""); }
    }
}
static char *sp3_str_or_empty(const json_t *v) {
    if (!sp3_truth(v)) return strdup("");
    return sp3_pystr(v);
}
static char *sp3_strip(const char *s) {
    if (!s) s = "";
    while (*s && (*s == ' ' || (*s >= '\t' && *s <= '\r'))) s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || (s[n-1] >= '\t' && s[n-1] <= '\r'))) n--;
    char *out = malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
static char *sp3_lower(const char *s) {
    char *out = strdup(s ? s : "");
    if (out) for (char *p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
    return out;
}
static bool sp3_startswith(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
static json_t *sp3_attr_err(const json_t *v, const char *attr) {
    char m[160];
    snprintf(m, sizeof(m), "'%s' object has no attribute '%s'", sp3_type_name(v), attr);
    return sp3_raise("AttributeError", m);
}
/* -- logging seam: {fmt, args?} -- */
static json_t *sp3_log_warning(const sp3_seams_t *s, const char *fmt, json_t *args_arr) {
    json_t *a = sp3_args();
    sp3_s(a, "fmt", fmt);
    if (args_arr && json_len(args_arr) > 0) json_set(a, "args", args_arr);
    else json_free(args_arr);
    return sp3_call(s, "logger.warning", a);
}
/* -- AuthorityRefusal (real class twin) -- */
static bool sp3_py_ws(unsigned char ch) {
    return ch == ' ' || (ch >= '\t' && ch <= '\r') || (ch >= 0x1c && ch <= 0x1f);
}
static char *sp3_one_line(const char *value, size_t limit) {
    if (!value) return NULL;
    size_t n = strlen(value);
    char *tmp = malloc(n + 1);
    if (!tmp) return NULL;
    size_t k = 0;
    bool pending_space = false, started = false;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)value[i];
        if (sp3_py_ws(ch)) { if (started) pending_space = true; continue; }
        if (pending_space) { tmp[k++] = ' '; pending_space = false; }
        started = true;
        tmp[k++] = (char)ch;
    }
    tmp[k] = '\0';
    size_t w = 0;
    for (size_t i = 0; i < k; i++) {
        unsigned char ch = (unsigned char)tmp[i];
        if (ch < 0x20 || (ch >= 0x7f && ch <= 0x9f)) continue;
        tmp[w++] = tmp[i];
    }
    tmp[w] = '\0';
    if (w == 0) { free(tmp); return NULL; }
    if (w > limit) w = limit;
    char *out = malloc(w + 1);
    if (!out) { free(tmp); return NULL; }
    memcpy(out, tmp, w); out[w] = '\0';
    free(tmp);
    return out;
}
static json_t *sp3_ar_raise(const char *reason, const char *field, const char *detail) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string("AuthorityRefusal"));
    json_set(r, "msg", json_string(reason ? reason : ""));
    json_set(r, "reason", json_string(reason ? reason : ""));
    char *f = sp3_one_line(field, 80);
    json_set(r, "field", f ? json_string(f) : json_null());
    free(f);
    char *d = sp3_one_line(detail, 160);
    json_set(r, "detail", d ? json_string(d) : json_null());
    free(d);
    return r;
}
/* json.loads for the strict-valid-JSON string domain. */
static json_t *sp3_jloads(const json_t *v) {
    if (!sp3_is_str(v)) {
        char m[160];
        snprintf(m, sizeof(m),
                 "the JSON object must be str, bytes or bytearray, not %s",
                 sp3_type_name(v));
        return sp3_raise("TypeError", m);
    }
    char *err = NULL;
    json_t *r = json_parse(v->str_val ? v->str_val : "", &err);
    free(err);
    if (!r) return sp3_raise("ValueError", "invalid JSON (out of domain)");
    return r;
}


/* -- module constants -- */
/* SURFACES (creation label -> agent surface). */
static const char *sp3_surface(const char *label) {
    if (!label) return NULL;
    if (strcmp(label, "cli") == 0) return "cli";
    if (strcmp(label, "tui") == 0) return "tui";
    if (strcmp(label, "gui") == 0) return "desktop";
    if (strcmp(label, "acp") == 0) return "acp";
    if (strcmp(label, "tool") == 0) return "cli";
    if (strcmp(label, "oneshot") == 0) return "cli";
    return NULL;
}
/* {SURFACES[name] for name in STORED_AS_LOCAL} == {cli,tui,desktop,acp}. */
static bool sp3_stored_local_surface(const char *p) {
    return p && (strcmp(p, "cli") == 0 || strcmp(p, "tui") == 0
                 || strcmp(p, "desktop") == 0 || strcmp(p, "acp") == 0);
}
/* LocalSessionPolicy dataclass fields, in declaration order. */
static const char *SP3_POLICY_FIELDS[] = {
    "source", "platform", "cwd", "model", "toolsets", "config_json",
    "request_json", "terminal_json", "credential_ref", "config_secret_ref",
    "editor_mcp_json", "kanban_json", "safe_mode", "ignore_user_config",
    "skills_prompt", "coding_posture", "client_env_json", "client_env_remote",
    "client_env_rules_json", "edit_approval_policy"};
#define SP3_N_FIELDS (sizeof(SP3_POLICY_FIELDS)/sizeof(SP3_POLICY_FIELDS[0]))
/* Required fields (no dataclass default), sorted alphabetically. */
static const char *SP3_REQUIRED[] = {
    "config_json", "cwd", "model", "platform", "request_json", "source",
    "terminal_json", "toolsets"};
#define SP3_N_REQUIRED (sizeof(SP3_REQUIRED)/sizeof(SP3_REQUIRED[0]))
static bool sp3_is_policy_field(const char *k) {
    for (size_t i = 0; i < SP3_N_FIELDS; i++)
        if (strcmp(SP3_POLICY_FIELDS[i], k) == 0) return true;
    return false;
}
/* Dataclass default for an optional field (NULL == None). */
static json_t *sp3_field_default(const char *k) {
    if (strcmp(k, "safe_mode") == 0 || strcmp(k, "ignore_user_config") == 0
        || strcmp(k, "coding_posture") == 0 || strcmp(k, "client_env_remote") == 0)
        return json_bool(false);
    if (strcmp(k, "edit_approval_policy") == 0) return json_string("ask");
    return json_null();
}
/* Normalize a partial policy dict to the full 20-field dataclass shape. */
static json_t *sp3_policy_norm(const json_t *d) {
    json_t *out = json_object();
    for (size_t i = 0; i < SP3_N_FIELDS; i++) {
        const char *k = SP3_POLICY_FIELDS[i];
        const json_t *v = sp3_oget(d, k);
        json_set(out, k, v ? json_copy(v) : sp3_field_default(k));
    }
    return out;
}
/* dataclasses.replace(policy, **changes) on the dict model. */
static json_t *sp3_policy_replace(const json_t *policy, const char *k1, const json_t *v1,
                                  const char *k2, const json_t *v2) {
    json_t *out = json_copy(policy);
    if (k1) json_set(out, k1, v1 ? json_copy(v1) : json_null());
    if (k2) json_set(out, k2, v2 ? json_copy(v2) : json_null());
    return out;
}
/* _SECRET_CONFIG_KEYS (hermes_cli.config), exact membership of key.lower(). */
static const char *SP3_SECRET_KEYS[] = {
    "api_key", "apikey", "key", "token", "access_token", "refresh_token", "id_token",
    "secret", "client_secret", "password", "passwd", "authorization",
    "private_key", "bearer", "jwt"};
#define SP3_N_SECRET (sizeof(SP3_SECRET_KEYS)/sizeof(SP3_SECRET_KEYS[0]))
static bool sp3_is_secret_key(const char *lower) {
    for (size_t i = 0; i < SP3_N_SECRET; i++)
        if (strcmp(SP3_SECRET_KEYS[i], lower) == 0) return true;
    return false;
}
/* containers set in _extract_config_secrets. */
static const char *SP3_CONTAINERS[] = {
    "env", "headers", "extra_headers", "docker_env", "docker_extra_args",
    "terminal_docker_env", "terminal_docker_extra_args"};
#define SP3_N_CONTAINERS (sizeof(SP3_CONTAINERS)/sizeof(SP3_CONTAINERS[0]))
static bool sp3_is_container(const char *lower) {
    for (size_t i = 0; i < SP3_N_CONTAINERS; i++)
        if (strcmp(SP3_CONTAINERS[i], lower) == 0) return true;
    return false;
}
#define SP3_PRIVATE_KEY "[\"editor_mcp\"]"   /* PRIVATE_KEY = ('editor_mcp',) */
#define SP3_PREFIX "profile-config-v1:"        /* session_policy_credentials.PREFIX */
#define SP3_PLATFORM_LOCAL "local"             /* gateway.config.Platform.LOCAL.value */

/* Compact JSON-array path key (tuple-path dict encoding), ASCII domain.
 * Mirrors json.dumps(list(path), separators=(",",":")) on the oracle side. */
static char *sp3_path_key(const json_t *path) {
    size_t cap = 64;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t k = 0;
    out[k++] = '[';
    if (sp3_is_arr(path)) {
        for (size_t i = 0; i < json_len(path); i++) {
            if (i) { if (k + 2 >= cap) { cap *= 2; out = realloc(out, cap); } out[k++] = ','; }
            const json_t *e = json_get(path, i);
            char *enc = NULL;
            char numbuf[32];
            if (!e || e->type == JSON_NULL) enc = strdup("null");
            else if (e->type == JSON_STRING) enc = json_escape_string(e->str_val ? e->str_val : "");
            else if (e->type == JSON_NUMBER) {
                snprintf(numbuf, sizeof(numbuf), "%lld", (long long)e->num_val);
                enc = strdup(numbuf);
            } else if (e->type == JSON_BOOL) enc = strdup(e->bool_val ? "true" : "false");
            else enc = json_serialize(e);
            size_t en = strlen(enc);
            while (k + en + 2 >= cap) { cap *= 2; out = realloc(out, cap); }
            memcpy(out + k, enc, en); k += en;
            free(enc);
        }
    }
    out[k++] = ']'; out[k] = '\0';
    return out;
}
/* -- _ADOPTED_KEY_SETS dedup (reset per case by the harness) -- */
static char **sp3_adopted = NULL;
static size_t sp3_adopted_n = 0;
static void sp3_adopt_reset(void) {
    for (size_t i = 0; i < sp3_adopted_n; i++) free(sp3_adopted[i]);
    free(sp3_adopted);
    sp3_adopted = NULL; sp3_adopted_n = 0;
}
static bool sp3_adopt_seen(const char *sig) {
    for (size_t i = 0; i < sp3_adopted_n; i++)
        if (strcmp(sp3_adopted[i], sig) == 0) return true;
    return false;
}
static void sp3_adopt_add(const char *sig) {
    sp3_adopted = realloc(sp3_adopted, (sp3_adopted_n + 1) * sizeof(char *));
    sp3_adopted[sp3_adopted_n++] = strdup(sig);
}
/* sorted join of a string array with ", " (malloc'd). */
static char *sp3_join_comma(char **items, size_t n) {
    size_t cap = 64; char *out = malloc(cap); size_t k = 0; out[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        size_t en = strlen(items[i]);
        while (k + en + 3 >= cap) { cap = (k + en + 3) * 2; out = realloc(out, cap); }
        if (i) { out[k++] = ','; out[k++] = ' '; }
        memcpy(out + k, items[i], en); k += en; out[k] = '\0';
    }
    return out;
}
/* collect object keys into a malloc'd sorted array of strdup'd strings. */
static char **sp3_sorted_keys(const json_t *obj, size_t *nout) {
    size_t n = sp3_is_obj(obj) ? obj->c.count : 0;
    char **keys = malloc((n ? n : 1) * sizeof(char *));
    for (size_t i = 0; i < n; i++) keys[i] = strdup(obj->c.keys[i]);
    for (size_t i = 1; i < n; i++) {
        char *t = keys[i]; size_t j = i;
        while (j > 0 && strcmp(keys[j-1], t) > 0) { keys[j] = keys[j-1]; j--; }
        keys[j] = t;
    }
    *nout = n;
    return keys;
}
static void sp3_free_keys(char **keys, size_t n) {
    for (size_t i = 0; i < n; i++) free(keys[i]);
    free(keys);
}

/* json.loads on a raw C string (strict-valid domain). */
static json_t *sp3_jloads_str(const char *str) {
    json_t *tmp = json_string(str ? str : "");
    json_t *r = sp3_jloads(tmp);
    json_free(tmp);
    return r;
}


/* -- 1. _refuse -- */
/* PoP: sp3_refuse @ gateway/session_policy.py:_refuse */
static json_t *sp3_refuse(const char *field, const char *detail) {
    return sp3_ar_raise("invalid_params", field, detail);
}

/* -- 2. _policy_refused -- */
/* PoP: sp3_policy_refused @ gateway/session_policy.py:_policy_refused */
static json_t *sp3_policy_refused(const char *field, const char *detail) {
    return sp3_ar_raise("storage_unavailable", field, detail);
}

/* -- 3. _adopt_stored_policy -- */
/* PoP: sp3_adopt_stored_policy @ gateway/session_policy.py:_adopt_stored_policy */
static json_t *sp3_adopt_stored_policy(const json_t *data, const sp3_seams_t *s) {
    if (!sp3_is_obj(data)) return json_copy(data);
    /* unknown = sorted(set(data) - _POLICY_FIELDS) */
    size_t nkeys = 0;
    char **keys = sp3_sorted_keys(data, &nkeys);
    char **unknown = malloc((nkeys ? nkeys : 1) * sizeof(char *));
    size_t nu = 0;
    for (size_t i = 0; i < nkeys; i++)
        if (!sp3_is_policy_field(keys[i])) unknown[nu++] = keys[i];
    if (nu == 0) {
        free(unknown);   /* array only; unknown[] holds borrowed keys[i] pointers */
        sp3_free_keys(keys, nkeys);
        return json_copy(data);
    }
    char *sig = sp3_join_comma(unknown, nu);
    json_t *ret = NULL;
    if (!sp3_adopt_seen(sig)) {
        sp3_adopt_add(sig);
        json_t *arr = json_array();
        json_append(arr, json_string(sig));
        json_t *lw = sp3_log_warning(s,
            "restore_policy: adopting a stored receipt whose keys this build dropped: %s", arr);
        if (sp3_raised(lw)) { ret = lw; goto out; }
        json_free(lw);
    }
    ret = json_object();
    for (size_t i = 0; i < data->c.count; i++) {
        const char *k = data->c.keys[i];
        if (sp3_is_policy_field(k)) json_set(ret, k, json_copy(data->c.items[i]));
    }
out:
    free(sig);
    free(unknown);
    sp3_free_keys(keys, nkeys);
    return ret;
}

/* -- 4. _stored_policy -- */
/* PoP: sp3_stored_policy @ gateway/session_policy.py:_stored_policy */
static json_t *sp3_stored_policy(const json_t *data, const sp3_seams_t *s) {
    if (!sp3_is_obj(data))
        return sp3_policy_refused("policy", "the stored policy is not an object");
    json_t *stored = sp3_adopt_stored_policy(data, s);
    if (sp3_raised(stored)) return stored;
    /* missing = sorted(_REQUIRED_POLICY_FIELDS - set(stored)); SP3_REQUIRED is sorted */
    const char *first_missing = NULL;
    for (size_t i = 0; i < SP3_N_REQUIRED; i++) {
        if (!json_has(stored, SP3_REQUIRED[i])) { first_missing = SP3_REQUIRED[i]; break; }
    }
    if (first_missing) {
        json_free(stored);
        return sp3_policy_refused(first_missing,
            "the stored policy has no value for this required field");
    }
    json_t *ret = sp3_policy_norm(stored);
    json_free(stored);
    return ret;
}

/* -- 5. _check_stored_surface -- */
/* {**SURFACES, 'cron':'cron', 'kanban':'cli', 'bot_room':'bot_room'} */
static const char *sp3_surfaces_ext(const char *label) {
    if (!label) return NULL;
    const char *s = sp3_surface(label);
    if (s) return s;
    if (strcmp(label, "cron") == 0) return "cron";
    if (strcmp(label, "kanban") == 0) return "cli";
    if (strcmp(label, "bot_room") == 0) return "bot_room";
    return NULL;
}
/* PoP: sp3_check_stored_surface @ gateway/session_policy.py:_check_stored_surface */
static json_t *sp3_check_stored_surface(const json_t *policy, const sp3_seams_t *s) {
    const json_t *src = sp3_oget(policy, "source");
    const json_t *plat = sp3_oget(policy, "platform");
    if (!sp3_is_str(src) || !sp3_is_str(plat))
        return sp3_policy_refused("source", "the stored source/platform is not a pair of names");
    const char *source = src->str_val ? src->str_val : "";
    const char *platform = plat->str_val ? plat->str_val : "";
    const char *ext = sp3_surfaces_ext(source);
    bool source_ok = (ext && strcmp(platform, ext) == 0)
                  || (strcmp(source, "local") == 0 && sp3_stored_local_surface(platform));
    json_t *a = sp3_args();
    sp3_j(a, "policy", policy);
    json_t *fwd = sp3_call(s, "is_forward_policy", a);
    if (sp3_raised(fwd)) return fwd;
    bool forward = sp3_truth(fwd);
    json_free(fwd);
    if (!forward && !source_ok) {
        char detail[256];
        snprintf(detail, sizeof(detail),
                 "stored source/platform %s/%s is not a known surface pair", source, platform);
        return sp3_policy_refused("source", detail);
    }
    return NULL;
}


/* -- 6. _extract_config_secrets (recursive, mutates value + private) -- */
/* PoP: sp3_extract_config_secrets @ gateway/session_policy.py:_extract_config_secrets */
static json_t *sp3_extract_config_secrets(json_t *value, json_t *private,
                                          const json_t *path, const sp3_seams_t *s) {
    bool is_dict = sp3_is_obj(value);
    bool is_list = sp3_is_arr(value);
    if (!is_dict && !is_list) return NULL;
    size_t n = is_dict ? value->c.count : json_len(value);
    for (size_t i = 0; i < n; i++) {
        const char *key = is_dict ? value->c.keys[i] : NULL;
        json_t *child = value->c.items[i];
        /* child_path = path + (key,) */
        json_t *cpath = json_copy(path);
        if (is_dict) json_append(cpath, json_string(key));
        else json_append(cpath, json_number((double)i));
        /* sensitive = isinstance(key,str) and (lower in SECRET_KEYS or in containers
         *              or _is_secret_payload_key(key)) -- short-circuit seam */
        bool sensitive = false;
        if (is_dict) {
            char *lower = sp3_lower(key);
            if (sp3_is_secret_key(lower) || sp3_is_container(lower)) {
                sensitive = true;
            } else {
                json_t *a = sp3_args();
                sp3_s(a, "key", key);
                json_t *r = sp3_call(s, "_is_secret_payload_key", a);
                if (sp3_raised(r)) { json_free(cpath); free(lower); return r; }
                sensitive = sp3_truth(r);
                json_free(r);
            }
            free(lower);
        }
        bool empty_repr = sp3_is_str(child) && child->str_val
                          && (strcmp(child->str_val, "{}") == 0 || strcmp(child->str_val, "[]") == 0);
        if (sensitive && sp3_truth(child) && !empty_repr) {
            if (!private) {
                json_free(cpath);
                return sp3_ar_raise("launch_credentials_unavailable", NULL, NULL);
            }
            char *pk = sp3_path_key(cpath);
            json_set(private, pk, json_copy(child));
            free(pk);
            if (is_dict) json_set(value, key, json_null());
            else { json_free(value->c.items[i]); value->c.items[i] = json_null(); }
        } else {
            json_t *r = sp3_extract_config_secrets(child, private, cpath, s);
            if (sp3_raised(r)) { json_free(cpath); return r; }
            json_free(r);
        }
        json_free(cpath);
    }
    return NULL;
}


/* unpack a 3-element sequence (build_preloaded_skills_prompt result). */
static json_t *sp3_unpack3(const json_t *r, const json_t **a, const json_t **b, const json_t **c) {
    if (sp3_is_null_node(r))
        return sp3_raise("TypeError", "cannot unpack non-iterable NoneType object");
    if (sp3_is_arr(r)) {
        size_t n = json_len(r);
        char m[96];
        if (n < 3) { snprintf(m, sizeof(m), "not enough values to unpack (expected 3, got %zu)", n);
            return sp3_raise("ValueError", m); }
        if (n > 3) { snprintf(m, sizeof(m), "too many values to unpack (expected 3, got %zu)", n);
            return sp3_raise("ValueError", m); }
        *a = json_get(r, 0); *b = json_get(r, 1); *c = json_get(r, 2);
        return NULL;
    }
    char m[128];
    snprintf(m, sizeof(m), "cannot unpack non-iterable %s object", sp3_type_name(r));
    return sp3_raise("TypeError", m);
}
/* ", ".join(arr) for a string array (malloc'd). */
static char *sp3_join_str_array(const json_t *arr) {
    size_t n = sp3_is_arr(arr) ? json_len(arr) : 0;
    size_t cap = 64; char *out = malloc(cap); size_t k = 0; out[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        const char *s = sp3_sval(json_get(arr, i));
        if (!s) s = "";
        size_t en = strlen(s);
        while (k + en + 3 >= cap) { cap = (k + en + 3) * 2; out = realloc(out, cap); }
        if (i) { out[k++] = ','; out[k++] = ' '; }
        memcpy(out + k, s, en); k += en; out[k] = '\0';
    }
    return out;
}

/* -- 7. render_launch_skills -- */
/* PoP: sp3_render_launch_skills @ gateway/session_policy.py:render_launch_skills */
static json_t *sp3_render_launch_skills(const json_t *params, const char *session_id,
                                        const sp3_seams_t *s) {
    json_t *la = sp3_args();
    sp3_j(la, "params", params);
    json_t *skills = sp3_call(s, "launch_skills", la);
    if (sp3_raised(skills)) return skills;
    if (!sp3_truth(skills)) { json_free(skills); return NULL; }
    json_t *ba = sp3_args();
    sp3_j(ba, "skills", skills);
    sp3_s(ba, "task_id", session_id);
    json_free(skills);
    json_t *res = sp3_call(s, "build_preloaded_skills_prompt", ba);
    if (sp3_raised(res)) return res;
    const json_t *prompt = NULL, *loaded = NULL, *missing = NULL;
    json_t *ue = sp3_unpack3(res, &prompt, &loaded, &missing);
    if (ue) { json_free(res); return ue; }
    if (!sp3_truth(loaded)) {
        json_free(res);
        return sp3_ar_raise("unknown_skill", NULL, NULL);
    }
    if (sp3_truth(missing)) {
        json_t *fa = sp3_args();
        sp3_j(fa, "missing", missing);
        json_t *fmt = sp3_call(s, "format_missing_skills", fa);
        if (sp3_raised(fmt)) { json_free(res); return fmt; }
        char *joined = sp3_join_str_array(loaded);
        json_t *arr = json_array();
        json_append(arr, fmt ? fmt : json_null());
        json_append(arr, json_string(session_id ? session_id : ""));
        json_append(arr, json_string(joined));
        free(joined);
        json_t *lw = sp3_log_warning(s, "Skipping %s for session %s; continuing with: %s", arr);
        if (sp3_raised(lw)) { json_free(res); return lw; }
        json_free(lw);
    }
    json_t *ret = sp3_truth(prompt) ? json_copy(prompt) : json_null();
    json_free(res);
    return ret;
}


/* -- 8. bind_launch_key (mutates authority) -- */
/* PoP: sp3_bind_launch_key @ gateway/session_policy.py:bind_launch_key */
static json_t *sp3_bind_launch_key(json_t *authority, const char *session_id,
                                   const json_t *policy_in, const json_t *api_key_in,
                                   json_t *cs, bool owner_key, const json_t *client_request,
                                   const sp3_seams_t *s) {
    json_t *ret = NULL;
    json_t *api_key = api_key_in ? json_copy(api_key_in) : NULL;
    json_t *old = NULL;
    char *ref = NULL, *config_ref = NULL;
    /* policy = bind_editor_mcp(authority, sid, policy, config_secrets.pop(PRIVATE_KEY, None)) */
    json_t *popped = NULL;
    if (cs && json_has(cs, SP3_PRIVATE_KEY)) {
        popped = json_copy(json_obj_get(cs, SP3_PRIVATE_KEY));
        json_obj_del(cs, SP3_PRIVATE_KEY);
    }
    json_t *ba = sp3_args();
    sp3_j(ba, "authority", authority);
    sp3_s(ba, "session_id", session_id);
    sp3_j(ba, "policy", policy_in);
    sp3_j(ba, "editor_secret", popped);
    json_free(popped);
    json_t *bound = sp3_call(s, "bind_editor_mcp", ba);
    if (sp3_raised(bound)) { ret = bound; goto out; }
    json_t *policy = sp3_policy_norm(bound);
    json_free(bound);
    /* early return: nothing private remains */
    if (sp3_is_null_node(api_key) && (!cs || cs->c.count == 0)) {
        ret = sp3_policy_replace(policy, "credential_ref", NULL, "config_secret_ref", NULL);
        json_free(policy); goto out;
    }
    /* keys / owners maps (create on the authority when absent) */
    json_t *keys = json_obj_get(authority, "_local_launch_keys");
    if (!keys) { keys = json_object(); json_set(authority, "_local_launch_keys", keys); }
    {
        char *iid = sp3_pystr(sp3_oget(authority, "instance_id"));
        char *ep = sp3_pystr(sp3_oget(authority, "epoch"));
        size_t cap = strlen(iid) + strlen(ep) + strlen(session_id ? session_id : "") + 3;
        ref = malloc(cap);
        snprintf(ref, cap, "%s:%s:%s", iid, ep, session_id ? session_id : "");
        free(iid); free(ep);
    }
    old = sp3_oget(keys, ref) ? json_copy(json_obj_get(keys, ref)) : NULL;
    json_t *owners = json_obj_get(authority, "_local_owner_key_requests");
    if (!owners) { owners = json_object(); json_set(authority, "_local_owner_key_requests", owners); }
    if (owner_key && !sp3_is_null_node(api_key) && old && json_has(owners, ref)) {
        json_t *sa = sp3_args();
        sp3_j(sa, "owner", json_obj_get(owners, ref));
        sp3_j(sa, "request_json", sp3_oget(policy, "request_json"));
        sp3_j(sa, "client_request", sp3_truth(client_request) ? client_request : NULL);
        json_t *cr_empty = NULL;
        if (!sp3_truth(client_request)) { cr_empty = json_object(); json_set(sa, "client_request", cr_empty); }
        json_t *retry = sp3_call(s, "same_create_retry", sa);
        if (sp3_raised(retry)) { ret = retry; json_free(policy); goto out; }
        bool take_old = sp3_truth(retry);
        json_free(retry);
        if (take_old) { json_free(api_key); api_key = json_copy(old); }
    }
    if (!sp3_is_null_node(api_key) && old) {
        json_t *ha = sp3_args();
        sp3_j(ha, "a", old);
        sp3_j(ha, "b", api_key);
        json_t *cmp = sp3_call(s, "hmac.compare_digest", ha);
        if (sp3_raised(cmp)) { ret = cmp; json_free(policy); goto out; }
        bool eq = sp3_truth(cmp);
        json_free(cmp);
        if (!eq) { ret = sp3_ar_raise("admission_conflict", NULL, NULL); json_free(policy); goto out; }
    }
    json_t *configs = json_obj_get(authority, "_local_config_secrets");
    if (!configs) { configs = json_object(); json_set(authority, "_local_config_secrets", configs); }
    config_ref = strdup(ref);
    bool has_cs = cs && cs->c.count > 0;
    if (has_cs && !sp3_is_null_node(sp3_oget(authority, "persistence"))) {
        json_t *ca = sp3_args();
        sp3_j(ca, "authority", authority);
        sp3_s(ca, "session_id", session_id);
        sp3_j(ca, "policy", policy);
        sp3_j(ca, "config_secrets", cs);
        json_t *cr = sp3_call(s, "config_reference", ca);
        if (sp3_raised(cr)) { ret = cr; json_free(policy); goto out; }
        free(config_ref);
        config_ref = sp3_pystr(cr);
        json_free(cr);
    }
    if (json_has(configs, config_ref) && !sp3_deep_eq(json_obj_get(configs, config_ref), cs)) {
        ret = sp3_ar_raise("admission_conflict", NULL, NULL); json_free(policy); goto out;
    }
    if (has_cs) json_set(configs, config_ref, json_copy(cs));
    if (!sp3_is_null_node(api_key) && !old && owner_key) {
        json_t *ow = json_object();
        sp3_j(ow, "request_json", sp3_oget(policy, "request_json"));
        sp3_j(ow, "client_env_json", sp3_oget(policy, "client_env_json"));
        sp3_j(ow, "client_env_rules_json", sp3_oget(policy, "client_env_rules_json"));
        json_set(owners, ref, ow);
    }
    if (!sp3_is_null_node(api_key)) json_set(keys, ref, json_copy(api_key));
    /* return replace(policy, credential_ref=..., config_secret_ref=...) */
    json_t *cred = NULL;
    if (!sp3_is_null_node(api_key)) {
        json_t *fa = sp3_args();
        sp3_j(fa, "api_key", api_key);
        json_t *fp = sp3_call(s, "fingerprint_secret_value", fa);
        if (sp3_raised(fp)) { ret = fp; json_free(policy); goto out; }
        char *fps = sp3_pystr(fp);
        json_free(fp);
        size_t cap = strlen(ref) + strlen(fps) + 2;
        char *crs = malloc(cap);
        snprintf(crs, cap, "%s#%s", ref, fps);
        free(fps);
        cred = json_string(crs);
        free(crs);
    }
    json_t *csref = has_cs ? json_string(config_ref) : NULL;
    ret = sp3_policy_replace(policy, "credential_ref", cred, "config_secret_ref", csref);
    json_free(cred); json_free(csref);
    json_free(policy);
out:
    json_free(api_key); json_free(old);
    free(ref); free(config_ref);
    return ret;
}


/* -- string-set helpers (retired / kept / chats / refs) -- */
typedef struct { char **items; size_t n; } sp3_strset_t;
static void sp3_set_init(sp3_strset_t *s) { s->items = NULL; s->n = 0; }
static bool sp3_set_has(const sp3_strset_t *s, const char *v) {
    if (!v) return false;
    for (size_t i = 0; i < s->n; i++) if (strcmp(s->items[i], v) == 0) return true;
    return false;
}
static void sp3_set_add(sp3_strset_t *s, const char *v) {
    if (!v || sp3_set_has(s, v)) return;
    s->items = realloc(s->items, (s->n + 1) * sizeof(char *));
    s->items[s->n++] = strdup(v);
}
static void sp3_set_free(sp3_strset_t *s) {
    for (size_t i = 0; i < s->n; i++) free(s->items[i]);
    free(s->items); s->items = NULL; s->n = 0;
}
/* sorted unique array from a strset (malloc'd char**). */
static char **sp3_set_sorted(const sp3_strset_t *s, size_t *nout) {
    char **a = malloc((s->n ? s->n : 1) * sizeof(char *));
    for (size_t i = 0; i < s->n; i++) a[i] = strdup(s->items[i]);
    for (size_t i = 1; i < s->n; i++) {
        char *t = a[i]; size_t j = i;
        while (j > 0 && strcmp(a[j-1], t) > 0) { a[j] = a[j-1]; j--; }
        a[j] = t;
    }
    *nout = s->n; return a;
}
/* ref.split(':', 2)[-1]: remainder after the 2nd ':' (or 1st, or whole). */
static const char *sp3_split_last2(const char *ref) {
    const char *c1 = strchr(ref, ':');
    if (!c1) return ref;
    const char *c2 = strchr(c1 + 1, ':');
    return c2 ? c2 + 1 : c1 + 1;
}
/* ref.partition('#')[2]: substring after the first '#' ("" when absent). */
static const char *sp3_after_hash(const char *ref) {
    const char *h = strchr(ref, '#');
    return h ? h + 1 : "";
}

/* -- 9. rebind_launch_key (mutates authority) -- */
/* PoP: sp3_rebind_launch_key @ gateway/session_policy.py:rebind_launch_key */
static json_t *sp3_rebind_launch_key(json_t *authority, const json_t *session,
                                     const json_t *api_key, const sp3_seams_t *s) {
    json_t *ret = NULL;
    char *slot = NULL, *slot2 = NULL;
    if (!sp3_is_str(api_key)) { return sp3_refuse("api_key", "must be a non-empty string"); }
    { char *st = sp3_strip(api_key->str_val ? api_key->str_val : "");
      bool empty = st[0] == '\0'; free(st);
      if (empty) return sp3_refuse("api_key", "must be a non-empty string"); }
    const char *sid = sp3_sval(sp3_oget(session, "session_id"));
    const json_t *sessions = sp3_oget(authority, "sessions");
    if (!sp3_is_obj(sessions)) return sp3_attr_err(authority, "sessions");
    const json_t *entry = sid ? json_obj_get(sessions, sid) : NULL;
    if (!entry) {
        char m[160]; snprintf(m, sizeof(m), "'%s'", sid ? sid : "");
        return sp3_raise("KeyError", m);
    }
    const json_t *source = sp3_oget(entry, "source");
    const char *plat = sp3_sval(sp3_oget(source, "platform"));
    if (sp3_is_null_node(source) || !plat || strcmp(plat, SP3_PLATFORM_LOCAL) != 0)
        return sp3_ar_raise("admission_conflict", NULL, NULL);
    /* ref = restore_policy(authority_persistence(authority)...policy).credential_ref */
    json_t *apa = sp3_args(); sp3_j(apa, "authority", authority);
    json_t *ap = sp3_call(s, "authority_persistence", apa);
    if (sp3_raised(ap)) return ap;
    const json_t *data = sp3_oget(ap, "receipt_policy");
    json_t *ra = sp3_args(); sp3_j(ra, "data", data);
    json_t *pol = sp3_call(s, "restore_policy", ra);
    json_free(ap);
    if (sp3_raised(pol)) return pol;
    const char *ref_borrow = sp3_sval(sp3_oget(pol, "credential_ref"));
    char *ref = ref_borrow ? strdup(ref_borrow) : NULL;   /* own it: pol is freed below */
    json_free(pol);
    if (!ref) return sp3_ar_raise("admission_conflict", NULL, NULL);
    json_t *keys = json_obj_get(authority, "_local_launch_keys");
    if (!keys) { keys = json_object(); json_set(authority, "_local_launch_keys", keys); }
    json_t *ka = sp3_args(); sp3_s(ka, "credential_ref", ref);
    json_t *sl = sp3_call(s, "_key_slot", ka);
    if (sp3_raised(sl)) { ret = sl; goto out; }
    slot = sp3_pystr(sl); json_free(sl);
    const json_t *held = json_obj_get(keys, slot);
    if (held) {
        json_t *ha = sp3_args(); sp3_j(ha, "a", held); sp3_j(ha, "b", api_key);
        json_t *cmp = sp3_call(s, "hmac.compare_digest", ha);
        if (sp3_raised(cmp)) { ret = cmp; goto out; }
        bool eq = sp3_truth(cmp); json_free(cmp);
        if (!eq) { ret = sp3_ar_raise("admission_conflict", NULL, NULL); goto out; }
        ret = NULL; goto out;
    }
    const char *fingerprint = sp3_after_hash(ref);
    if (!fingerprint[0]) { ret = sp3_ar_raise("admission_conflict", NULL, NULL); goto out; }
    json_t *fa = sp3_args(); sp3_j(fa, "api_key", api_key);
    json_t *fp = sp3_call(s, "fingerprint_secret_value", fa);
    if (sp3_raised(fp)) { ret = fp; goto out; }
    char *fps = sp3_truth(fp) ? sp3_pystr(fp) : strdup("");
    json_free(fp);
    json_t *ha2 = sp3_args(); sp3_s(ha2, "a", fingerprint); sp3_s(ha2, "b", fps);
    free(fps);
    json_t *cmp2 = sp3_call(s, "hmac.compare_digest", ha2);
    if (sp3_raised(cmp2)) { ret = cmp2; goto out; }
    bool eq2 = sp3_truth(cmp2); json_free(cmp2);
    if (!eq2) { ret = sp3_ar_raise("admission_conflict", NULL, NULL); goto out; }
    json_t *ka2 = sp3_args(); sp3_s(ka2, "credential_ref", ref);
    json_t *sl2 = sp3_call(s, "_key_slot", ka2);
    if (sp3_raised(sl2)) { ret = sl2; goto out; }
    slot2 = sp3_pystr(sl2); json_free(sl2);
    json_set(keys, slot2, json_copy(api_key));
    ret = NULL;
out:
    free(slot); free(slot2); free(ref);
    return ret;
}

/* -- 10. _secret_refs -- */
/* PoP: sp3_secret_refs @ gateway/session_policy.py:_secret_refs */
static json_t *sp3_secret_refs(json_t *authority, const sp3_strset_t *retired,
                               const sp3_seams_t *s) {
    json_t *ret = NULL;
    sp3_strset_t chats; sp3_set_init(&chats);
    sp3_strset_t refs; sp3_set_init(&refs);
    const json_t *sessions = sp3_oget(authority, "sessions");
    if (sp3_is_obj(sessions)) {
        for (size_t i = 0; i < sessions->c.count; i++) {
            const char *sid = sessions->c.keys[i];
            if (sp3_set_has(retired, sid)) continue;
            const json_t *source = sp3_oget(sessions->c.items[i], "source");
            if (sp3_is_null_node(source)) continue;
            const char *plat = sp3_sval(sp3_oget(source, "platform"));
            if (plat && strcmp(plat, SP3_PLATFORM_LOCAL) == 0)
                sp3_set_add(&chats, sp3_sval(sp3_oget(source, "chat_id")));
        }
    }
    const json_t *rl = sp3_oget(authority, "retired_local");
    if (sp3_is_arr(rl)) {
        for (size_t i = 0; i < json_len(rl); i++) {
            const char *sid = sp3_sval(json_get(rl, i));
            if (sid && !sp3_set_has(retired, sid)) sp3_set_add(&chats, sid);
        }
    }
    if (chats.n == 0) { ret = json_array(); goto out; }
    json_t *ma = sp3_args(); sp3_j(ma, "authority", authority);
    json_t *map = sp3_call(s, "local_adapter_map", ma);
    if (sp3_raised(map)) { ret = map; goto out; }
    const json_t *adapter = sp3_oget(map, SP3_PLATFORM_LOCAL);
    const json_t *policies = sp3_oget(adapter, "policies");
    if (!sp3_truth(policies)) policies = NULL;
    size_t nc = 0; char **cs = sp3_set_sorted(&chats, &nc);
    for (size_t i = 0; i < nc; i++) {
        const json_t *policy = sp3_oget(policies, cs[i]);
        if (!sp3_truth(policy)) continue;   /* filter(None, ...) */
        const json_t *cred = sp3_oget(policy, "credential_ref");
        if (sp3_truth(cred)) {
            json_t *ka = sp3_args(); sp3_j(ka, "credential_ref", cred);
            json_t *sl = sp3_call(s, "_key_slot", ka);
            if (sp3_raised(sl)) { ret = sl; sp3_free_keys(cs, nc); goto out; }
            if (sp3_truth(sl)) sp3_set_add(&refs, sp3_sval(sl));
            json_free(sl);
        }
        const json_t *csref = sp3_oget(policy, "config_secret_ref");
        if (sp3_truth(csref)) sp3_set_add(&refs, sp3_sval(csref));
    }
    sp3_free_keys(cs, nc);
    json_free(map);
    {
        size_t nr = 0; char **rs = sp3_set_sorted(&refs, &nr);
        ret = json_array();
        for (size_t i = 0; i < nr; i++) json_append(ret, json_string(rs[i]));
        sp3_free_keys(rs, nr);
    }
out:
    sp3_set_free(&chats); sp3_set_free(&refs);
    return ret;
}


/* -- 11. release_launch_secrets (mutates authority) -- */
/* PoP: sp3_release_launch_secrets @ gateway/session_policy.py:release_launch_secrets */
static json_t *sp3_release_launch_secrets(json_t *authority, const json_t *session_ids,
                                          const sp3_seams_t *s) {
    json_t *ret = NULL;
    sp3_strset_t retired; sp3_set_init(&retired);
    if (sp3_is_arr(session_ids)) {
        for (size_t i = 0; i < json_len(session_ids); i++) {
            char *sid = sp3_pystr(json_get(session_ids, i));
            sp3_set_add(&retired, sid);
            free(sid);
        }
    }
    if (retired.n == 0) goto out;
    json_t *kept = sp3_secret_refs(authority, &retired, s);
    if (sp3_raised(kept)) { ret = kept; goto out; }
    sp3_strset_t keptset; sp3_set_init(&keptset);
    if (sp3_is_arr(kept))
        for (size_t i = 0; i < json_len(kept); i++) sp3_set_add(&keptset, sp3_sval(json_get(kept, i)));
    json_free(kept);
    char *iid = sp3_pystr(sp3_oget(authority, "instance_id"));
    char *ep = sp3_pystr(sp3_oget(authority, "epoch"));
    size_t pcap = strlen(iid) + strlen(ep) + 3;   /* "i:e:" + NUL (2 colons + terminator) */
    char *prefix = malloc(pcap);
    snprintf(prefix, pcap, "%s:%s:", iid, ep);
    free(iid); free(ep);
    /* keys / owners: `getattr(...) or {}` -- a falsy map is a fresh local (no mutation) */
    json_t *keys_auth = json_obj_get(authority, "_local_launch_keys");
    json_t *keys_local = NULL;
    json_t *keys = sp3_truth(keys_auth) ? keys_auth : (keys_local = json_object());
    json_t *owners_auth = json_obj_get(authority, "_local_owner_key_requests");
    json_t *owners_local = NULL;
    json_t *owners = sp3_truth(owners_auth) ? owners_auth : (owners_local = json_object());
    {
        size_t nk = keys->c.count;
        char **krefs = malloc((nk ? nk : 1) * sizeof(char *));
        for (size_t i = 0; i < nk; i++) krefs[i] = strdup(keys->c.keys[i]);
        for (size_t i = 0; i < nk; i++) {
            const char *ref = krefs[i];
            const char *tail = sp3_split_last2(ref);
            if (sp3_set_has(&retired, tail) && !sp3_set_has(&keptset, ref)) {
                json_obj_del(keys, ref);
                json_obj_del(owners, ref);
            }
        }
        sp3_free_keys(krefs, nk);
    }
    json_t *configs_auth = json_obj_get(authority, "_local_config_secrets");
    json_t *configs_local = NULL;
    json_t *configs = sp3_truth(configs_auth) ? configs_auth : (configs_local = json_object());
    {
        size_t nc = configs->c.count;
        char **crefs = malloc((nc ? nc : 1) * sizeof(char *));
        for (size_t i = 0; i < nc; i++) crefs[i] = strdup(configs->c.keys[i]);
        for (size_t i = 0; i < nc; i++) {
            const char *ref = crefs[i];
            char *owner = NULL;
            bool owner_is_null = false;
            if (sp3_startswith(ref, SP3_PREFIX)) {
                json_t *parsed = sp3_jloads_str(ref + strlen(SP3_PREFIX));
                if (sp3_raised(parsed)) { ret = parsed; sp3_free_keys(crefs, nc); free(prefix);
                    json_free(keys_local); json_free(owners_local); json_free(configs_local);
                    sp3_set_free(&keptset); goto out; }
                const json_t *sess = sp3_oget(parsed, "session");
                if (sp3_is_str(sess)) owner = strdup(sess->str_val ? sess->str_val : "");
                else owner_is_null = true;   /* non-str owner never matches the str retired set */
                json_free(parsed);
            } else if (sp3_startswith(ref, prefix)) {
                owner = strdup(ref + strlen(prefix));
            } else {
                owner_is_null = true;
            }
            bool in_retired = !owner_is_null && owner && sp3_set_has(&retired, owner);
            if (in_retired && !sp3_set_has(&keptset, ref)) json_obj_del(configs, ref);
            free(owner);
        }
        sp3_free_keys(crefs, nc);
    }
    json_t *editors_auth = json_obj_get(authority, "_local_editor_mcp");
    json_t *editors_local = NULL;
    json_t *editors = sp3_truth(editors_auth) ? editors_auth : (editors_local = json_object());
    {
        char *pid = sp3_pystr(sp3_oget(authority, "profile_id"));
        size_t nr = 0; char **rs = sp3_set_sorted(&retired, &nr);
        for (size_t i = 0; i < nr; i++) {
            size_t dcap = strlen(pid) + strlen(rs[i]) + 2;
            char *data = malloc(dcap);
            snprintf(data, dcap, "%s:%s", pid, rs[i]);
            json_t *sa = sp3_args(); sp3_s(sa, "data", data);
            free(data);
            json_t *dig = sp3_call(s, "hashlib.sha256", sa);
            if (sp3_raised(dig)) { ret = dig; sp3_free_keys(rs, nr); free(pid); free(prefix);
                json_free(keys_local); json_free(owners_local); json_free(configs_local);
                json_free(editors_local); sp3_set_free(&keptset); goto out; }
            char *hex = sp3_pystr(dig); json_free(dig);
            size_t kcap = strlen(hex) + 16;
            char *ekey = malloc(kcap);
            snprintf(ekey, kcap, "editor-session:%s", hex);
            free(hex);
            json_obj_del(editors, ekey);
            free(ekey);
        }
        sp3_free_keys(rs, nr);
        free(pid);
    }
    free(prefix);
    json_free(keys_local); json_free(owners_local);
    json_free(configs_local); json_free(editors_local);
    sp3_set_free(&keptset);
    ret = NULL;
out:
    sp3_set_free(&retired);
    return ret;
}


/* -- 12. policy_for_source -- */
/* PoP: sp3_policy_for_source @ gateway/session_policy.py:policy_for_source */
static json_t *sp3_policy_for_source(const json_t *runner, const json_t *source,
                                     const sp3_seams_t *s) {
    if (sp3_is_null_node(source)) return NULL;
    const char *plat = sp3_sval(sp3_oget(source, "platform"));
    if (!plat || strcmp(plat, SP3_PLATFORM_LOCAL) != 0) return NULL;
    json_t *aa = sp3_args(); sp3_j(aa, "source", source);
    json_t *adapter = sp3_call(s, "runner._adapter_for_source", aa);
    if (sp3_raised(adapter)) return adapter;
    if (!sp3_is_obj(adapter)) { json_free(adapter); return NULL; }  /* not LocalSessionAdapter */
    json_t *za = sp3_args(); sp3_j(za, "source", source);
    json_t *auth = sp3_call(s, "adapter.authorize_source", za);
    if (sp3_raised(auth)) { json_free(adapter); return auth; }
    bool ok = sp3_truth(auth);
    json_free(auth);
    if (!ok) { json_free(adapter); return NULL; }
    const json_t *policies = sp3_oget(adapter, "policies");
    const char *chat = sp3_sval(sp3_oget(source, "chat_id"));
    const json_t *policy = (chat && sp3_is_obj(policies)) ? json_obj_get(policies, chat) : NULL;
    json_t *policy_copy = policy ? json_copy(policy) : NULL;
    json_free(adapter);
    if (!policy_copy)
        return sp3_ar_raise("storage_unavailable", "policy",
            "the session has no restored policy (its receipt was refused or not yet recovered)");
    json_t *norm = sp3_policy_norm(policy_copy);
    json_free(policy_copy);
    return norm;
}

/* -- 13. workspace_unavailable_result -- */
/* PoP: sp3_workspace_unavailable_result @ gateway/session_policy.py:workspace_unavailable_result */
static json_t *sp3_workspace_unavailable_result(const json_t *policy, const sp3_seams_t *s) {
    json_t *ta = sp3_args();
    sp3_s(ta, "key", "gateway.errors.workspace_unavailable");
    sp3_j(ta, "path", sp3_oget(policy, "cwd"));
    json_t *text = sp3_call(s, "t", ta);
    if (sp3_raised(text)) return text;
    json_t *result = json_object();
    sp3_j(result, "final_response", text);
    json_set(result, "messages", json_array());
    json_set(result, "api_calls", json_number(0));
    json_set(result, "tools", json_array());
    json_set(result, "failed", json_bool(true));
    json_set(result, "completed", json_bool(false));
    sp3_j(result, "error", text);
    json_free(text);
    json_t *sa = sp3_args();
    sp3_j(sa, "result", result);
    sp3_s(sa, "kind", "workspace_unavailable");
    sp3_b(sa, "flag", false);
    json_free(result);
    json_t *ret = sp3_call(s, "stamp_failure", sa);
    if (sp3_raised(ret)) return ret;
    return ret ? ret : json_null();
}

/* -- 14. refused_turn_result -- */
/* PoP: sp3_refused_turn_result @ gateway/session_policy.py:refused_turn_result */
static json_t *sp3_refused_turn_result(const json_t *policy, const json_t *exc,
                                       const sp3_seams_t *s) {
    const char *reason = sp3_sval(sp3_oget(exc, "reason"));
    json_t *result = NULL;
    if (reason && strcmp(reason, "workspace_unavailable") == 0) {
        result = sp3_workspace_unavailable_result(policy, s);
        if (sp3_raised(result)) return result;
    } else if (reason && strcmp(reason, "launch_credentials_unavailable") == 0) {
        json_t *exc_spec = sp3_ar_raise(reason, sp3_sval(sp3_oget(exc, "field")),
                                        sp3_sval(sp3_oget(exc, "detail")));
        json_t *ca = sp3_args(); sp3_j(ca, "exc", exc_spec);
        json_free(exc_spec);
        json_t *text = sp3_call(s, "launch_credentials_copy", ca);
        if (sp3_raised(text)) return text;
        result = json_object();
        sp3_j(result, "final_response", text);
        json_set(result, "messages", json_array());
        json_set(result, "api_calls", json_number(0));
        json_set(result, "tools", json_array());
        json_set(result, "failed", json_bool(true));
        json_set(result, "completed", json_bool(false));
        sp3_j(result, "error", text);
        json_free(text);
        json_t *sa = sp3_args();
        sp3_j(sa, "result", result);
        sp3_s(sa, "kind", "launch_credentials_unavailable");
        sp3_b(sa, "flag", false);
        json_free(result);
        result = sp3_call(s, "stamp_failure", sa);
        if (sp3_raised(result)) return result;
    } else {
        return NULL;
    }
    json_t *ba = sp3_args(); sp3_j(ba, "result", result);
    json_t *surface = sp3_call(s, "build_error_surface_from_result", ba);
    if (sp3_raised(surface)) { json_free(result); return surface; }
    json_set(result, "error_surface", surface ? surface : json_null());
    return result;
}

/* -- 15. release_session_workspaces (owner_scope CM + unbind seam) -- */
/* PoP: sp3_release_session_workspaces @ gateway/session_policy.py:release_session_workspaces */
static json_t *sp3_release_session_workspaces(const json_t *authority, const json_t *session_ids,
                                              const sp3_seams_t *s) {
    json_t *ea = sp3_args(); sp3_j(ea, "authority", authority);
    json_t *enter = sp3_call(s, "owner_scope.__enter__", ea);
    if (sp3_raised(enter)) return enter;
    json_free(enter);
    json_t *pending = NULL;
    if (sp3_is_arr(session_ids)) {
        for (size_t i = 0; i < json_len(session_ids); i++) {
            json_t *ua = sp3_args();
            sp3_j(ua, "task_id", json_get(session_ids, i));
            json_t *r = sp3_call(s, "unbind_turn_workspace", ua);
            if (sp3_raised(r)) { pending = r; break; }
            json_free(r);
        }
    }
    json_t *xa = sp3_args(); sp3_j(xa, "exc", pending);
    json_t *exit = sp3_call(s, "owner_scope.__exit__", xa);
    if (sp3_raised(exit)) { json_free(pending); return exit; }
    bool suppress = sp3_truth(exit);
    json_free(exit);
    if (pending) {
        if (suppress) { json_free(pending); return NULL; }
        return pending;
    }
    return NULL;
}

/* -- 16. policy_turn_toolsets -- */
/* PoP: sp3_policy_turn_toolsets @ gateway/session_policy.py:policy_turn_toolsets */
static json_t *sp3_policy_turn_toolsets(const json_t *authority, const json_t *policy,
                                        const json_t *config, const char *session_id,
                                        const sp3_seams_t *s) {
    json_t *store = NULL;
    if (!sp3_is_null_node(authority)) {
        json_t *pa = sp3_args(); sp3_j(pa, "authority", authority);
        store = sp3_call(s, "authority_persistence", pa);
        if (sp3_raised(store)) return store;
    }
    json_t *ca = sp3_args(); sp3_j(ca, "config", config);
    json_t *ts = sp3_call(s, "policy.current_toolsets", ca);
    if (sp3_raised(ts)) { json_free(store); return ts; }
    json_t *ts_list = NULL;
    if (sp3_is_arr(ts)) ts_list = json_copy(ts);
    else {
        char m[128];
        snprintf(m, sizeof(m), "'%s' object is not iterable", sp3_type_name(ts));
        json_free(ts); json_free(store);
        return sp3_raise("TypeError", m);
    }
    json_free(ts);
    json_t *oa = sp3_args();
    sp3_j(oa, "toolsets", ts_list);
    sp3_j(oa, "store", store);
    sp3_s(oa, "session_id", session_id);
    sp3_j(oa, "persistence", store);
    json_free(ts_list); json_free(store);
    json_t *ret = sp3_call(s, "onboarding_chat_toolsets", oa);
    if (sp3_raised(ret)) return ret;
    return ret ? ret : json_null();
}
