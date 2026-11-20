/*
 * tui_methods_tools_flow2.c — FLOW port of the UNCLAIMED remainder of
 * tui_gateway/methods_tools.py (hermes-agent): the slash-command dispatch
 * cluster, the goal/loop/session-cluster helpers, the toolset/config helper
 * leaves and the memory/skills/worker helpers. flow1 (tui_methods_tools_flow.c)
 * and the cron/plugins/goals flows already claim the REST of methods_tools.py;
 * every function PoP-annotated here was verified UNCLAIMED by a CRLF-aware
 * `PoP: ... @ tui_gateway/methods_tools.py:<fn>` grep over src/ first.
 *
 * Ports the DECISION SEQUENCES — branch order, error-code/message composition,
 * seam call order and exception paths — not just leaf outputs. Every live
 * external call enters through ONE universal seam, tmt2_call(ctx,name,args).
 * The harness wires it to a recording stub fed the case's scripted results
 * (in.io); the oracle installs a Python reference model emitting the identical
 * event shape {"name","args","ret"} from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tmt2_-prefixed.
 *
 * Seam name -> live call (args are informational projections):
 *   ok / err                 _ok(rid,result) / _err(rid,code,msg,data=None)
 *   exec_out                 _exec_out(rid,output)  (claimed sibling, seamed)
 *   tools_mod                _tools_mod(module) -> module marker string
 *   load_cfg                 _load_cfg()
 *   sessions.get             _sessions.get(key)
 *   dispatch                 _methods[method](rid, params)
 *   busy_error               _busy_error(rid,session,cmd)
 *   rewind_prelude           _rewind_prelude(rid,session,cmd,empty)
 *   rewind_or_err            _rewind_or_err(...)
 *   session_key_or_err       _session_key_or_err(rid,session,module,label)
 *   session_toolsets         _session_toolsets(session) -> [enabled,disabled]
 *   session_cwd              _session_cwd(session)
 *   session_home_scope       with _session_home_scope(session)
 *   profile_scope            with _session_profile_runtime_scope(session)
 *   apply_model_switch       _apply_model_switch(...)
 *   session_not_owned_err    _session_not_owned_err(rid,exc)
 *   tui_model_friction       _tui_model_friction(cmd,session,*extra)
 *   session_uses_compute_host / compute_host_slash / compress_live_with_feedback
 *   mcp_server_rows / mcp_plugin_write_error / reset_session_agent
 *   write_config_key         _write_config_key(key,enabled)
 *   set_session_context / clear_session_context
 *   resolve_session_platform / resolve_model / load_tool_progress_mode
 *   skill_scaffold_projection / flatten_message_text / undo_rollback_hint_for
 *   slash_worker_ctor / attach_worker / logger.warning
 *   bind_module / plugin_commands.register
 *   build_init_prompt_for_cwd / get_session_cwd / handle_pending_subcommand
 *   dispatch_goal_command
 *   <module>.<method>        a method on a _tools_mod() module double
 *   agent.<method>           a method on the session's agent double
 *   mgr.unsupported_backend_reason
 *
 * Representation contract (shared with tests/oracle_tui_methods_tools_flow2.py):
 * - NULL json_t* == Python None; a raise is {"__raise__":cls,"message":str(exc)}.
 * - Full builtin exception hierarchy: BaseException-only kinds
 *   (SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError) pass through
 *   every except-Exception site; the ValueError family includes UnicodeError,
 *   OSError family includes PermissionError/FileNotFoundError, etc.
 * - Scripted raises carry str(exc)==message verbatim.
 * - Tuples project to lists; sets to sorted lists; integral floats to ints.
 * - Seam args drop None-valued keys on both sides (absent-vs-null parity).
 * - ASCII fixture corpus; POSIX paths; integral numbers.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct tmt2_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} tmt2_ctx_t;

/* ── basic helpers ── */
static bool tmt2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *tmt2_raise_kind(const json_t *v) {
    return tmt2_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static json_t *tmt2_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type ? type : "RuntimeError");
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
/* BaseException-only kinds pass through every except-Exception site. */
static bool tmt2_is_base_exc(const char *k) {
    return k && (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt")
                 || !strcmp(k, "GeneratorExit") || !strcmp(k, "CancelledError"));
}
/* ValueError family (except ValueError). */
static bool tmt2_is_value_error(const char *k) {
    return k && (!strcmp(k, "ValueError") || !strcmp(k, "UnicodeError")
                 || !strcmp(k, "UnicodeDecodeError") || !strcmp(k, "UnicodeEncodeError"));
}
/* OSError family (except OSError/PermissionError/FileNotFoundError). */
static bool tmt2_is_os_error(const char *k) {
    return k && (!strcmp(k, "OSError") || !strcmp(k, "PermissionError")
                 || !strcmp(k, "FileNotFoundError") || !strcmp(k, "FileExistsError")
                 || !strcmp(k, "IsADirectoryError") || !strcmp(k, "NotADirectoryError")
                 || !strcmp(k, "TimeoutError"));
}
/* SessionNotOwned project class. */
static bool tmt2_is_not_owned(const char *k) {
    return k && !strcmp(k, "SessionNotOwned");
}
/* Python truthiness on the fixture domain. */
static bool tmt2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *tmt2_sv(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static const char *tmt2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
static char *tmt2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s); char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1); return o;
}
static char *tmt2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tmt2_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return tmt2_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return tmt2_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char b[64]; double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(b, sizeof b, "%lld", (long long)d);
            else snprintf(b, sizeof b, "%.17g", d);
            return tmt2_strdup(b);
        }
        default: return json_serialize(v);
    }
}
static bool tmt2_ws(unsigned char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'||c=='\v';
}
static char *tmt2_strip(const char *s) {
    if (!s) s = "";
    while (*s && tmt2_ws((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && tmt2_ws((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - s); char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; } return o;
}
static char *tmt2_lower(const char *s) {
    char *o = tmt2_strdup(s); if (o) for (char *p=o; *p; p++) *p=(char)tolower((unsigned char)*p); return o;
}
/* (arg or "").strip() -> str; a truthy non-string produces AttributeError. */
static char *tmt2_str_strip(const json_t *v, json_t **raise_out) {
    if (raise_out) *raise_out = NULL;
    if (!tmt2_truthy(v)) return tmt2_strdup("");
    const char *s = tmt2_sv(v);
    if (!s) {
        if (raise_out) {
            char m[128];
            snprintf(m, sizeof m, "'%s' object has no attribute 'strip'", tmt2_type_name(v));
            *raise_out = tmt2_err_spec("AttributeError", m);
        }
        return tmt2_strdup("");
    }
    return tmt2_strip(s);
}
/* str(value or "") — never raises on the scalar fixture domain. */
static char *tmt2_str_or_empty(const json_t *v) {
    return tmt2_truthy(v) ? tmt2_pystr(v) : tmt2_strdup("");
}
/* "\n".join(parts) then strip — _joined_output semantics. */
static char *tmt2_joined(const char *a, const char *b) {
    if (!a) a = ""; if (!b) b = "";
    size_t na = strlen(a), nb = strlen(b);
    char *buf = malloc(na + nb + 2);
    if (!buf) return tmt2_strdup("");
    size_t k = 0;
    if (na) { memcpy(buf + k, a, na); k += na; }
    if (na && nb) buf[k++] = '\n';
    if (nb) { memcpy(buf + k, b, nb); k += nb; }
    buf[k] = '\0';
    char *out = tmt2_strip(buf); free(buf); return out;
}
/* substring of s[:n] (codepoint == byte on ASCII corpus). */
static char *tmt2_slice(const char *s, size_t n) {
    if (!s) return tmt2_strdup("");
    size_t l = strlen(s); if (n > l) n = l;
    char *o = malloc(n + 1); if (o) { memcpy(o, s, n); o[n]='\0'; } return o;
}

/* ── args builders (values copied into a fresh object) ── */
static json_t *tmt2_a0(void) { return json_object(); }
static json_t *tmt2_a1(const char *k, const json_t *v) {
    json_t *o = json_object();
    if (v) json_set(o, k, json_copy(v)); return o;
}
static json_t *tmt2_a2(const char *k1, const json_t *v1, const char *k2, const json_t *v2) {
    json_t *o = json_object();
    if (v1) json_set(o, k1, json_copy(v1));
    if (v2) json_set(o, k2, json_copy(v2));
    return o;
}
static json_t *tmt2_as(const char *k, const char *v) {
    json_t *o = json_object(); if (v) json_set_hdr(o, k, v); return o;
}
static json_t *tmt2_an(const char *k, double v) {
    json_t *o = json_object(); json_set(o, k, json_number(v)); return o;
}
static json_t *tmt2_ab(const char *k, bool v) {
    json_t *o = json_object(); json_set(o, k, json_bool(v)); return o;
}
/* err args: rid, code, message, optional data (data key only when present). */
static json_t *tmt2_err_args(const json_t *rid, int code, const char *msg, const json_t *data) {
    json_t *o = json_object();
    if (rid) json_set(o, "rid", json_copy(rid));
    json_set(o, "code", json_number(code));
    json_set_hdr(o, "message", msg ? msg : "");
    if (data) json_set(o, "data", json_copy(data));
    return o;
}
/* ok args: rid + result. */
static json_t *tmt2_ok_args(const json_t *rid, const json_t *result) {
    json_t *o = json_object();
    if (rid) json_set(o, "rid", json_copy(rid));
    json_set(o, "result", result ? json_copy(result) : json_null());
    return o;
}

/* ── the one universal seam (fail-closed) ── */
static json_t *tmt2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return tmt2_err_spec(type, msg);
}
static json_t *tmt2_call(tmt2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_t *clean = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
            json_set(clean, args->c.keys[i], json_copy(v));
        }
    }
    json_free(args);
    json_set(ev, "args", clean);
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char m[256]; snprintf(m, sizeof m, "no script for %s", name);
        ret = tmt2_err_spec("RuntimeError", m);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char m[256]; snprintf(m, sizeof m, "script exhausted for %s", name);
            ret = tmt2_err_spec("RuntimeError", m);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) ret = tmt2_make_exc(raise);
            else { json_t *r = json_obj_get(used, "ret"); ret = r ? json_copy(r) : json_null(); }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}
/* call and propagate a raise verbatim. */
#define T2C(name, args) tmt2_call(ctx, (name), (args))
#define T2_RET_IF_RAISE(v) do { if (tmt2_is_raise(v)) return (v); } while (0)

/* ── PoP port functions ── */

/* PoP: tmt2_session_toolsets @ tui_gateway/methods_tools.py:_session_toolsets */
static json_t *tmt2_session_toolsets(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    bool has = tmt2_truthy(session);
    const json_t *agent = has ? json_obj_get(session, "agent") : NULL;
    if (agent && agent->type != JSON_NULL) {
        json_t *e = T2C("agent.enabled_toolsets", tmt2_a0()); T2_RET_IF_RAISE(e);
        json_t *d = T2C("agent.disabled_toolsets", tmt2_a0()); T2_RET_IF_RAISE(d);
        json_t *r = json_array(); json_append(r, e); json_append(r, d); return r;
    }
    json_t *sc = T2C("session_source", tmt2_a1("session", session)); T2_RET_IF_RAISE(sc);
    json_t *ps = T2C("profile_scope", tmt2_a1("session", session)); T2_RET_IF_RAISE(ps);
    json_t *plat = T2C("resolve_agent_platform", tmt2_a1("source", sc)); T2_RET_IF_RAISE(plat);
    json_t *en = T2C("load_enabled_toolsets", tmt2_a1("platform", plat)); T2_RET_IF_RAISE(en);
    json_t *dis = T2C("load_disabled_toolsets", tmt2_a0()); T2_RET_IF_RAISE(dis);
    json_t *r = json_array(); json_append(r, en); json_append(r, dis); return r;
}

/* PoP: tmt2_toolset_rows @ tui_gateway/methods_tools.py:_toolset_rows */
static json_t *tmt2_toolset_rows(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *params = json_obj_get(args, "params");
    bool with_tools = json_get_bool(args, "with_tools", false);
    const json_t *sid = params ? json_obj_get(params, "session_id") : NULL;
    json_t *tm = T2C("tools_mod", tmt2_as("module", "toolsets")); T2_RET_IF_RAISE(tm);
    json_t *sess = T2C("sessions.get", tmt2_a1("key", sid)); T2_RET_IF_RAISE(sess);
    json_t *sts = T2C("session_toolsets", tmt2_a1("session", sess)); T2_RET_IF_RAISE(sts);
    const json_t *en = (sts && sts->type == JSON_ARRAY && json_len(sts) > 0) ? json_get(sts, 0) : NULL;
    bool enabled_nonempty = tmt2_truthy(en);
    json_t *all = T2C("toolsets.get_all_toolsets", tmt2_a0()); T2_RET_IF_RAISE(all);
    json_t *rows = json_array();
    if (!all || all->type != JSON_OBJECT) return rows;
    /* sorted keys */
    size_t n = all->c.count;
    const char **keys = malloc((n ? n : 1) * sizeof(char *));
    for (size_t i = 0; i < n; i++) keys[i] = all->c.keys[i];
    for (size_t i = 0; i < n; i++) for (size_t j = i + 1; j < n; j++)
        if (strcmp(keys[i], keys[j]) > 0) { const char *t=keys[i]; keys[i]=keys[j]; keys[j]=t; }
    for (size_t i = 0; i < n; i++) {
        json_t *info = T2C("toolsets.get_toolset_info", tmt2_as("name", keys[i])); T2_RET_IF_RAISE(info);
        if (!tmt2_truthy(info)) continue;
        json_t *row = json_object();
        json_set_hdr(row, "name", keys[i]);
        const json_t *desc = json_obj_get(info, "description");
        const json_t *cnt = json_obj_get(info, "tool_count");
        json_set(row, "description", desc ? json_copy(desc) : json_null());
        json_set(row, "tool_count", cnt ? json_copy(cnt) : json_null());
        bool on = true;
        if (enabled_nonempty && en->type == JSON_ARRAY) {
            on = false;
            for (size_t k = 0; k < json_len(en); k++) {
                const char *s = tmt2_sv(json_get(en, k));
                if (s && !strcmp(s, keys[i])) { on = true; break; }
            }
        }
        json_set(row, "enabled", json_bool(on));
        if (with_tools) {
            const json_t *tools = json_obj_get(info, "resolved_tools");
            json_set(row, "tools", tools ? json_copy(tools) : json_null());
        }
        json_append(rows, row);
    }
    free(keys);
    return rows;
}

/* PoP: tmt2_skill_dispatch_or_refuse @ tui_gateway/methods_tools.py:_skill_dispatch_or_refuse */
static json_t *tmt2_skill_dispatch_or_refuse(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *sid = json_obj_get(args, "sid");
    const char *base = tmt2_sv(json_obj_get(args, "base"));
    const json_t *arg = json_obj_get(args, "arg");
    json_t *pa = json_object();
    json_set_hdr(pa, "name", base ? base : "");
    if (arg) json_set(pa, "arg", json_copy(arg));
    if (sid) json_set(pa, "session_id", json_copy(sid));
    json_t *dp = json_object(); json_set_hdr(dp, "method", "command.dispatch");
    json_set(dp, "rid", json_obj_get(args, "rid") ? json_copy(json_obj_get(args, "rid")) : json_null());
    json_set(dp, "params", pa);
    json_t *dispatched = T2C("dispatch", dp); T2_RET_IF_RAISE(dispatched);
    const json_t *err = dispatched ? json_obj_get(dispatched, "error") : NULL;
    const json_t *result = dispatched ? json_obj_get(dispatched, "result") : NULL;
    const json_t *typ = (result && result->type == JSON_OBJECT) ? json_obj_get(result, "type") : NULL;
    if (err || tmt2_truthy(typ)) return dispatched;
    char m[256]; snprintf(m, sizeof m, "skill command: use command.dispatch for /%s", base ? base : "");
    return T2C("err", tmt2_err_args(json_obj_get(args, "rid"), 4018, m, NULL));
}

/* PoP: tmt2_bundle_key_for @ tui_gateway/methods_tools.py:_bundle_key_for */
static json_t *tmt2_bundle_key_for(tmt2_ctx_t *ctx, const json_t *args) {
    const char *name = tmt2_sv(json_obj_get(args, "name"));
    json_t *rc = T2C("hermes_cli.commands.resolve_command", tmt2_as("name", name));
    if (tmt2_is_raise(rc)) return json_null();
    if (rc && rc->type != JSON_NULL) return json_null();
    json_t *bk = T2C("agent.skill_bundles.resolve_bundle_command_key", tmt2_as("name", name));
    if (tmt2_is_raise(bk)) return json_null();
    return bk;
}

/* PoP: tmt2_dispatch_bundle @ tui_gateway/methods_tools.py:_dispatch_bundle */
static json_t *tmt2_dispatch_bundle(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const char *name = tmt2_sv(json_obj_get(args, "name"));
    const json_t *arg = json_obj_get(args, "arg");
    const json_t *session = json_obj_get(args, "session");
    json_t *bk = T2C("bundle_key_for", tmt2_as("name", name)); T2_RET_IF_RAISE(bk);
    if (!tmt2_truthy(bk)) return json_null();
    const char *bks = tmt2_sv(bk);
    const json_t *skey = (session && session->type == JSON_OBJECT) ? json_obj_get(session, "session_key") : NULL;
    json_t *ba = json_object();
    json_set(ba, "bundle_key", json_copy(bk));
    json_set(ba, "arg", arg ? json_copy(arg) : json_null());
    json_set(ba, "task_id", skey ? json_copy(skey) : json_string(""));
    json_t *plat = T2C("resolve_session_platform", tmt2_a0()); T2_RET_IF_RAISE(plat);
    json_set(ba, "platform", plat ? json_copy(plat) : json_null());
    json_t *res = T2C("agent.skill_bundles.build_bundle_invocation_message", ba);
    if (tmt2_is_raise(res)) {
        char m[256]; snprintf(m, sizeof m, "bundle dispatch failed: %s", json_get_str(res, "message", ""));
        return T2C("err", tmt2_err_args(rid, 4018, m, NULL));
    }
    if (!tmt2_truthy(res)) {
        char m[256]; snprintf(m, sizeof m, "failed to load bundle: %s", bks ? bks : "");
        return T2C("err", tmt2_err_args(rid, 4018, m, NULL));
    }
    json_t *msg = json_get(res, 0); json_t *loaded = json_get(res, 1); json_t *missing = json_get(res, 2);
    json_t *sb = T2C("agent.skill_bundles.get_skill_bundles", tmt2_a0()); T2_RET_IF_RAISE(sb);
    const json_t *entry = (sb && sb->type == JSON_OBJECT && bks) ? json_obj_get(sb, bks) : NULL;
    const json_t *bn = (entry && entry->type == JSON_OBJECT) ? json_obj_get(entry, "name") : NULL;
    const char *bname = tmt2_sv(bn);
    if (!bname && bks) { bname = bks; while (*bname == '/') bname++; }
    size_t nl = loaded ? json_len(loaded) : 0;
    char notice[1024];
    snprintf(notice, sizeof notice, "\342\232\241 Loading bundle: %s (%zu skills)", bname ? bname : "", nl);
    if (missing && json_len(missing) > 0) {
        strncat(notice, "\nSkipped missing skills: ", sizeof notice - strlen(notice) - 1);
        for (size_t i = 0; i < json_len(missing); i++) {
            if (i) strncat(notice, ", ", sizeof notice - strlen(notice) - 1);
            const char *s = tmt2_sv(json_get(missing, i));
            strncat(notice, s ? s : "", sizeof notice - strlen(notice) - 1);
        }
    }
    json_t *disp = T2C("skill_scaffold_projection", tmt2_a1("message", msg)); T2_RET_IF_RAISE(disp);
    json_t *payload = json_object();
    json_set_hdr(payload, "type", "send");
    json_set(payload, "message", msg ? json_copy(msg) : json_null());
    json_set_hdr(payload, "notice", notice);
    json_set(payload, "display", disp ? json_copy(disp) : json_null());
    return T2C("ok", tmt2_ok_args(rid, payload));
}

/* PoP: tmt2_dispatch_skill @ tui_gateway/methods_tools.py:_dispatch_skill */
static json_t *tmt2_dispatch_skill(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const char *name = tmt2_sv(json_obj_get(args, "name"));
    const json_t *arg = json_obj_get(args, "arg");
    const json_t *session = json_obj_get(args, "session");
    char key[256]; snprintf(key, sizeof key, "/%s", name ? name : "");
    char *lk = tmt2_lower(key);
    json_t *tm = T2C("tools_mod", tmt2_as("module", "agent.skill_commands"));
    if (tmt2_is_raise(tm)) return json_null();  /* contextlib.suppress(Exception) */
    json_t *cmds = T2C("agent.skill_commands.get_interactive_skill_commands", tmt2_a0());
    if (tmt2_is_raise(cmds)) return json_null();  /* contextlib.suppress(Exception) */
    if (!cmds || cmds->type != JSON_OBJECT || !json_obj_get(cmds, lk)) { free(lk); return json_null(); }
    const json_t *entry = json_obj_get(cmds, lk);
    const json_t *skey = (session && session->type == JSON_OBJECT) ? json_obj_get(session, "session_key") : NULL;
    json_t *sa = json_object(); json_set(sa, "arg", arg ? json_copy(arg) : json_null());
    json_set(sa, "interactive", json_bool(true));
    json_t *extra = T2C("agent.skill_commands.split_stacked_skill_commands", sa);
    if (tmt2_is_raise(extra)) { free(lk); return json_null(); }
    if (extra && extra->type == JSON_ARRAY && json_len(extra) > 0 && tmt2_truthy(json_get(extra, 0))) {
        json_t *stack_args = json_object();
        json_set(stack_args, "keys", json_array());
        json_append(json_obj_get(stack_args, "keys"), json_string(lk));
        if (json_get(extra, 0)->type == JSON_ARRAY)
            for (size_t i = 0; i < json_len(json_get(extra, 0)); i++)
                json_append(json_obj_get(stack_args, "keys"), json_copy(json_get(json_get(extra, 0), i)));
        json_set(stack_args, "instruction", json_get(extra, 1) ? json_copy(json_get(extra, 1)) : json_string(""));
        json_set(stack_args, "task_id", skey ? json_copy(skey) : json_string(""));
        json_t *stacked = T2C("agent.skill_commands.build_stacked_skill_invocation_message", stack_args);
        if (tmt2_is_raise(stacked)) { free(lk); return json_null(); }
        if (tmt2_truthy(stacked)) {
            json_t *msg = json_get(stacked, 0); json_t *loaded = json_get(stacked, 1); json_t *missing = json_get(stacked, 2);
            char notice[1024];
            snprintf(notice, sizeof notice, "\342\232\241 Loading %zu stacked skills: ", loaded ? json_len(loaded) : 0);
            for (size_t i = 0; i < (loaded ? json_len(loaded) : 0); i++) {
                if (i) strncat(notice, ", ", sizeof notice - strlen(notice) - 1);
                const char *s = tmt2_sv(json_get(loaded, i)); strncat(notice, s ? s : "", sizeof notice - strlen(notice) - 1);
            }
            if (missing && json_len(missing) > 0) {
                strncat(notice, "\nSkipped missing skills: ", sizeof notice - strlen(notice) - 1);
                for (size_t i = 0; i < json_len(missing); i++) { if (i) strncat(notice, ", ", sizeof notice - strlen(notice) - 1);
                    const char *s = tmt2_sv(json_get(missing, i)); strncat(notice, s ? s : "", sizeof notice - strlen(notice) - 1); }
            }
            const json_t *cn = (entry && entry->type == JSON_OBJECT) ? json_obj_get(entry, "name") : NULL;
            json_t *disp = T2C("skill_scaffold_projection", tmt2_a1("message", msg)); T2_RET_IF_RAISE(disp);
            json_t *p = json_object(); json_set_hdr(p, "type", "skill");
            json_set(p, "message", msg ? json_copy(msg) : json_null());
            json_set_hdr(p, "name", tmt2_sv(cn) ? tmt2_sv(cn) : (name ? name : ""));
            json_set_hdr(p, "notice", notice);
            json_set(p, "display", disp ? json_copy(disp) : json_null());
            free(lk); return T2C("ok", tmt2_ok_args(rid, p));
        }
    }
    json_t *ba = json_object(); json_set_hdr(ba, "key", lk);
    json_set(ba, "instruction", json_get(extra, 1) ? json_copy(json_get(extra, 1)) : json_string(""));
    json_set(ba, "task_id", skey ? json_copy(skey) : json_string(""));
    json_t *msg = T2C("agent.skill_commands.build_skill_invocation_message", ba);
    if (tmt2_is_raise(msg)) { free(lk); return json_null(); }
    if (tmt2_truthy(msg)) {
        const json_t *cn = (entry && entry->type == JSON_OBJECT) ? json_obj_get(entry, "name") : NULL;
        json_t *disp = T2C("skill_scaffold_projection", tmt2_a1("message", msg)); T2_RET_IF_RAISE(disp);
        json_t *p = json_object(); json_set_hdr(p, "type", "skill");
        json_set(p, "message", msg ? json_copy(msg) : json_null());
        json_set_hdr(p, "name", tmt2_sv(cn) ? tmt2_sv(cn) : (name ? name : ""));
        json_set(p, "display", disp ? json_copy(disp) : json_null());
        free(lk); return T2C("ok", tmt2_ok_args(rid, p));
    }
    free(lk); return json_null();
}

/* PoP: tmt2_prompt_builtin @ tui_gateway/methods_tools.py:_prompt_builtin */
/* Returns the closure as a marker object; the fixture then drives it via
 * func="prompt_builtin.call" with args {rid, module, fn, kw, arg}. */
static json_t *tmt2_prompt_builtin(tmt2_ctx_t *ctx, const json_t *args) {
    const char *module = tmt2_sv(json_obj_get(args, "module"));
    const char *fn = tmt2_sv(json_obj_get(args, "fn"));
    json_t *m = json_object();
    json_set_hdr(m, "__closure__", "prompt_builtin");
    json_set_hdr(m, "module", module ? module : "");
    json_set_hdr(m, "fn", fn ? fn : "");
    const json_t *kw = json_obj_get(args, "kw");
    if (kw) json_set(m, "kw", json_copy(kw));
    return m;
}
/* The closure body: getattr(_tools_mod(module), fn)(arg or kw=arg) -> ok send. */
static json_t *tmt2_prompt_builtin_call(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const char *module = tmt2_sv(json_obj_get(args, "module"));
    const char *fn = tmt2_sv(json_obj_get(args, "fn"));
    const json_t *arg = json_obj_get(args, "arg");
    bool has_kw = json_obj_get(args, "kw") && tmt2_truthy(json_obj_get(args, "kw"));
    const char *kw = has_kw ? tmt2_sv(json_obj_get(args, "kw")) : NULL;
    char seam[256]; snprintf(seam, sizeof seam, "%s.%s", module ? module : "", fn ? fn : "");
    json_t *ba = json_object();
    if (has_kw && kw) json_set(ba, kw, arg ? json_copy(arg) : json_null());
    else json_set(ba, "arg", arg ? json_copy(arg) : json_null());
    json_t *msg = T2C(seam, ba); T2_RET_IF_RAISE(msg);
    json_t *payload = json_object(); json_set_hdr(payload, "type", "send");
    json_set(payload, "message", msg ? json_copy(msg) : json_null());
    return T2C("ok", tmt2_ok_args(rid, payload));
}

/* PoP: tmt2_dispatch_quick @ tui_gateway/methods_tools.py:_dispatch_quick */
static json_t *tmt2_dispatch_quick(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const char *name = tmt2_sv(json_obj_get(args, "name"));
    json_t *cfg = T2C("load_cfg", tmt2_a0()); T2_RET_IF_RAISE(cfg);
    const json_t *qc = NULL;
    if (cfg && cfg->type == JSON_OBJECT) {
        const json_t *qcs = json_obj_get(cfg, "quick_commands");
        if (qcs && qcs->type == JSON_OBJECT && name) qc = json_obj_get(qcs, name);
    }
    if (!tmt2_truthy(qc)) return json_null();
    const json_t *typ = json_obj_get(qc, "type");
    const char *ts = tmt2_sv(typ);
    if (ts && !strcmp(ts, "exec")) {
        json_t *env = T2C("tools.environments.local.build_subprocess_env", tmt2_a0()); T2_RET_IF_RAISE(env);
        json_t *ra = json_object();
        json_set(ra, "command", json_obj_get(qc, "command") ? json_copy(json_obj_get(qc, "command")) : json_string(""));
        json_set(ra, "shell", json_bool(true));
        json_set(ra, "env", env ? json_copy(env) : json_null());
        json_set(ra, "timeout", json_number(30));
        json_t *r = T2C("subprocess.run", ra); T2_RET_IF_RAISE(r);
        const char *so = tmt2_sv(json_obj_get(r, "stdout"));
        const char *se = tmt2_sv(json_obj_get(r, "stderr"));
        char *joined = tmt2_joined(so, se);
        char *out4000 = tmt2_slice(joined, 4000); free(joined);
        json_t *output = json_string(out4000); free(out4000);
        if (tmt2_truthy(output)) {
            json_t *red = T2C("agent.redact.redact_sensitive_text", tmt2_a1("text", output));
            T2_RET_IF_RAISE(red); json_free(output); output = red;
        }
        const json_t *rc = json_obj_get(r, "returncode");
        double rcv = (rc && rc->type == JSON_NUMBER) ? rc->num_val : 0.0;
        if (rcv != 0.0) {
            if (!tmt2_truthy(output)) {
                char m[256]; snprintf(m, sizeof m, "quick command failed with exit code %.0f", rcv);
                json_free(output); return T2C("err", tmt2_err_args(rid, 4018, m, NULL));
            }
            return T2C("err", tmt2_err_args(rid, 4018, tmt2_sv(output) ? tmt2_sv(output) : "", NULL));
        }
        return T2C("exec_out", tmt2_a2("rid", rid, "output", output));
    }
    if (ts && !strcmp(ts, "alias")) {
        json_t *p = json_object(); json_set_hdr(p, "type", "alias");
        const json_t *tg = json_obj_get(qc, "target");
        json_set_hdr(p, "target", tmt2_sv(tg) ? tmt2_sv(tg) : "");
        return T2C("ok", tmt2_ok_args(rid, p));
    }
    return json_null();
}

/* PoP: tmt2_profile_skill_command @ tui_gateway/methods_tools.py:_profile_skill_command */
static json_t *tmt2_profile_skill_command(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    const char *base = tmt2_sv(json_obj_get(args, "base"));
    json_t *hs = T2C("session_home_scope", tmt2_a1("session", session));
    if (tmt2_is_raise(hs)) return json_null();  /* except Exception -> None */
    char key[256]; snprintf(key, sizeof key, "/%s", base ? base : "");
    json_t *cmds = T2C("agent.skill_commands.get_interactive_skill_commands", tmt2_a0());
    if (tmt2_is_raise(cmds)) return json_null();
    bool found = false;
    if (cmds && cmds->type == JSON_OBJECT) found = json_obj_get(cmds, key) != NULL;
    json_free(hs);
    return json_bool(found);
}

/* PoP: tmt2_cmd_init @ tui_gateway/methods_tools.py:_cmd_init */
static json_t *tmt2_cmd_init(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *session = json_obj_get(args, "session");
    const json_t *arg = json_obj_get(args, "arg");
    const json_t *skey = (session && session->type == JSON_OBJECT) ? json_obj_get(session, "session_key") : NULL;
    json_t *cwd = T2C("get_session_cwd", tmt2_a1("session_key", skey));
    if (tmt2_is_raise(cwd)) cwd = json_null();  /* contextlib.suppress */
    if (!tmt2_truthy(cwd)) { json_t *f = T2C("session_cwd", tmt2_a1("session", session)); T2_RET_IF_RAISE(f); cwd = f; }
    json_t *ba = json_object();
    if (arg) json_set(ba, "extra", json_copy(arg));
    json_set(ba, "cwd", cwd ? json_copy(cwd) : json_null());
    json_set(ba, "session_key", skey ? json_copy(skey) : json_null());
    json_t *msg = T2C("build_init_prompt_for_cwd", ba); T2_RET_IF_RAISE(msg);
    json_t *p = json_object(); json_set_hdr(p, "type", "send");
    json_set(p, "message", msg ? json_copy(msg) : json_null());
    return T2C("ok", tmt2_ok_args(rid, p));
}

/* PoP: tmt2_cmd_moa @ tui_gateway/methods_tools.py:_cmd_moa */
static json_t *tmt2_cmd_moa(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    const json_t *session = json_obj_get(args, "session");
    const json_t *arg = json_obj_get(args, "arg");
    if (!tmt2_truthy(arg)) return T2C("err", tmt2_err_args(rid, 4004, "moa usage", NULL));
    if (!tmt2_truthy(session)) return T2C("err", tmt2_err_args(rid, 4001, "no active session", NULL));
    json_t *cfg = T2C("load_cfg", tmt2_a0());
    if (tmt2_is_raise(cfg)) {
        char m[256]; snprintf(m, sizeof m, "moa unavailable: %s", json_get_str(cfg, "message", ""));
        return T2C("err", tmt2_err_args(rid, 5030, m, NULL));
    }
    const json_t *mcfg = json_obj_get(cfg, "moa");
    json_t *norm = T2C("hermes_cli.moa_config.normalize_moa_config", tmt2_a1("config", mcfg));
    T2_RET_IF_RAISE(norm);
    const json_t *preset = (norm && norm->type == JSON_OBJECT) ? json_obj_get(norm, "default_preset") : NULL;
    const char *ps = tmt2_sv(preset) ? tmt2_sv(preset) : "";
    /* session["moa_one_shot_restore"] = {...} — seam records the write */
    const json_t *mo = json_obj_get(session, "model_override");
    json_t *restore = json_object();
    json_set(restore, "override", mo ? json_copy(mo) : json_null());
    json_t *av = T2C("agent.attr", tmt2_as("name", "model"));
    json_t *pv = T2C("agent.attr", tmt2_as("name", "provider"));
    json_set(restore, "model", av ? json_copy(av) : json_null());
    json_set(restore, "provider", pv ? json_copy(pv) : json_null());
    json_t *w = json_object(); json_set(w, "key", json_string("moa_one_shot_restore")); json_set(w, "value", restore);
    json_t *wr = T2C("session.write", w); T2_RET_IF_RAISE(wr);
    const json_t *agent = json_obj_get(session, "agent");
    if (agent && agent->type != JSON_NULL) {
        char sw[256]; snprintf(sw, sizeof sw, "%s --provider moa", ps);
        json_t *sa = json_object();
        if (params) json_set(sa, "session_id", json_obj_get(params, "session_id") ? json_copy(json_obj_get(params, "session_id")) : json_string(""));
        json_set_hdr(sa, "override", sw);
        json_set(sa, "confirm_expensive_model", json_bool(false));
        json_set(sa, "pin_session_override", json_bool(true));
        json_set(sa, "persist_override", json_bool(false));
        json_set(sa, "count_switch", json_bool(false));
        json_t *swr = T2C("apply_model_switch", sa);
        if (tmt2_is_raise(swr)) {
            json_t *d = T2C("session.del", tmt2_as("key", "moa_one_shot_restore")); T2_RET_IF_RAISE(d);
            if (tmt2_is_value_error(tmt2_raise_kind(swr)) || !strcmp(tmt2_raise_kind(swr), "SessionNotOwned"))
                return T2C("session_not_owned_err", tmt2_a1("exc", json_obj_get(swr, "message")));
            char m[256]; snprintf(m, sizeof m, "moa unavailable: %s", json_get_str(swr, "message", ""));
            return T2C("err", tmt2_err_args(rid, 5030, m, NULL));
        }
    } else {
        json_t *ov = json_object(); json_set_hdr(ov, "provider", "moa"); json_set_hdr(ov, "model", ps);
        json_set_hdr(ov, "base_url", "moa://local"); json_set_hdr(ov, "api_key", "moa-virtual-provider");
        json_set_hdr(ov, "api_mode", "chat_completions");
        json_t *w2 = json_object(); json_set(w2, "key", json_string("model_override")); json_set(w2, "value", ov);
        json_t *r2 = T2C("session.write", w2); T2_RET_IF_RAISE(r2);
    }
    char notice[512]; snprintf(notice, sizeof notice,
        "MoA one-shot queued with preset %s; previous model will be restored after this turn.", ps);
    json_t *p = json_object(); json_set_hdr(p, "type", "send");
    json_set_hdr(p, "notice", notice);
    json_set(p, "message", arg ? json_copy(arg) : json_null());
    return T2C("ok", tmt2_ok_args(rid, p));
}

/* PoP: tmt2_cmd_focus @ tui_gateway/methods_tools.py:_cmd_focus */
static json_t *tmt2_cmd_focus(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    const json_t *arg = json_obj_get(args, "arg");
    json_t *cfg = T2C("load_cfg", tmt2_a0()); T2_RET_IF_RAISE(cfg);
    const json_t *disp = json_obj_get(cfg, "display");
    bool cur = false;
    if (disp && disp->type == JSON_OBJECT) cur = tmt2_truthy(json_obj_get(disp, "focus_view"));
    json_t *ra = json_object(); json_set(ra, "arg", arg ? json_copy(arg) : json_null()); json_set(ra, "cur", json_bool(cur));
    json_t *action = T2C("hermes_cli.focus_view.resolve_focus_arg", ra); T2_RET_IF_RAISE(action);
    const char *a = tmt2_sv(action) ? tmt2_sv(action) : "";
    if (!strcmp(a, "usage")) return T2C("err", tmt2_err_args(rid, 4004, "usage: /focus [on|off|status]", NULL));
    if (!strcmp(a, "status")) {
        const json_t *saved0 = (disp && disp->type == JSON_OBJECT) ? json_obj_get(disp, "focus_saved_tool_progress") : NULL;
        json_t *saved = (json_t *)saved0;
        if (!tmt2_truthy(saved)) { saved = T2C("load_tool_progress_mode", tmt2_a0()); T2_RET_IF_RAISE(saved); }
        json_t *fa = json_object(); json_set(fa, "cur", json_bool(cur)); json_set(fa, "saved", saved ? json_copy(saved) : json_null());
        json_t *out = T2C("hermes_cli.focus_view.format_focus_status", fa); T2_RET_IF_RAISE(out);
        return T2C("exec_out", tmt2_a2("rid", rid, "output", out));
    }
    bool target = tmt2_truthy(action);
    json_t *dp = json_object(); json_set_hdr(dp, "key", "focus");
    json_set_hdr(dp, "value", target ? "on" : "off");
    if (params) json_set(dp, "session_id", json_obj_get(params, "session_id") ? json_copy(json_obj_get(params, "session_id")) : json_string(""));
    json_t *q = json_object(); json_set_hdr(q, "method", "config.set"); json_set(q, "rid", json_copy(rid)); json_set(q, "params", dp);
    json_t *res = T2C("dispatch", q); T2_RET_IF_RAISE(res);
    if (res && json_obj_get(res, "error")) return res;
    const json_t *rr = json_obj_get(res, "result");
    const json_t *tp = (rr && rr->type == JSON_OBJECT) ? json_obj_get(rr, "tool_progress") : NULL;
    if (!tmt2_truthy(tp)) tp = json_string("all");
    json_t *fa = json_object(); json_set(fa, "target", json_bool(target)); json_set(fa, "tool_progress", json_copy(tp));
    json_t *out = T2C("hermes_cli.focus_view.format_focus_toggle_message", fa); T2_RET_IF_RAISE(out);
    return T2C("exec_out", tmt2_a2("rid", rid, "output", out));
}

/* PoP: tmt2_cmd_retry @ tui_gateway/methods_tools.py:_cmd_retry */
static json_t *tmt2_cmd_retry(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *session = json_obj_get(args, "session");
    if (!tmt2_truthy(session)) return T2C("err", tmt2_err_args(rid, 4001, "no active session to retry", NULL));
    json_t *b1 = T2C("busy_error", tmt2_a1("cmd", json_string("retry"))); T2_RET_IF_RAISE(b1);
    if (tmt2_truthy(b1)) return b1;
    json_t *pre = T2C("rewind_prelude", tmt2_a2("cmd", json_string("retry"), "empty", json_string("no previous user message to retry")));
    T2_RET_IF_RAISE(pre);
    const json_t *uind = (pre && pre->type == JSON_ARRAY && json_len(pre) > 1) ? json_get(pre, 1) : NULL;
    if (!tmt2_truthy(uind) || json_len(uind) == 0) {
        return T2C("err", tmt2_err_args(rid, 4018, "no previous user message to retry", NULL));
    }
    json_t *hv = T2C("session_rules.turns.history_before_user_originated_turn", tmt2_a1("index", json_get(uind, json_len(uind) - 1)));
    T2_RET_IF_RAISE(hv);
    json_t *live = tmt2_truthy(hv) ? json_get(hv, 1) : json_null();
    const json_t *content = (live && live->type == JSON_OBJECT) ? json_obj_get(live, "content") : NULL;
    json_t *txt = T2C("session_rules.turns.retryable_user_text", tmt2_a1("content", content));
    if (tmt2_is_raise(txt)) {
        if (tmt2_is_value_error(tmt2_raise_kind(txt)))
            return T2C("err", tmt2_err_args(rid, 4018, json_get_str(txt, "message", ""), NULL));
        return txt;
    }
    json_t *ro = json_object();
    json_set(ro, "keep", json_number((double)(json_len(uind) - 1)));
    json_set(ro, "code", json_number(4018)); json_set_hdr(ro, "prefix", "");
    json_set(ro, "require_retryable", json_bool(true));
    json_t *rw = T2C("rewind_or_err", ro); T2_RET_IF_RAISE(rw);
    const json_t *er = (rw && rw->type == JSON_ARRAY && json_len(rw) > 1) ? json_get(rw, 1) : NULL;
    if (tmt2_truthy(er)) return json_copy(er);
    const json_t *rr0 = json_get(rw, 0);
    const json_t *c2 = (rr0 && rr0->type == JSON_ARRAY && json_len(rr0) > 1) ? json_obj_get(json_get(rr0, 1), "content") : NULL;
    json_t *txt2 = T2C("session_rules.turns.retryable_user_text", tmt2_a1("content", c2)); T2_RET_IF_RAISE(txt2);
    json_t *fr = T2C("tui_model_friction", tmt2_a2("cmd", json_string("retry"), "session", session)); T2_RET_IF_RAISE(fr);
    json_t *p = json_object(); json_set_hdr(p, "type", "send");
    json_set(p, "message", txt2 ? json_copy(txt2) : json_null());
    return T2C("ok", tmt2_ok_args(rid, p));
}

/* PoP: tmt2_cmd_steer @ tui_gateway/methods_tools.py:_cmd_steer */
static json_t *tmt2_cmd_steer(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *arg = json_obj_get(args, "arg");
    const json_t *session = json_obj_get(args, "session");
    if (!tmt2_truthy(arg)) return T2C("err", tmt2_err_args(rid, 4004, "usage: /steer <prompt>", NULL));
    const char *s = tmt2_sv(arg) ? tmt2_sv(arg) : "";
    size_t l = strlen(s);
    char shown[256]; snprintf(shown, sizeof shown, "%s%s", (l > 80 ? (char*)tmt2_slice(s,80) : s), (l > 80 ? "..." : ""));
    bool running = (session && session->type == JSON_OBJECT) ? tmt2_truthy(json_obj_get(session, "running")) : false;
    if (!running) {
        char m[512]; snprintf(m, sizeof m, "No agent running; sent as next turn: %s", shown);
        json_t *p = json_object(); json_set_hdr(p, "type", "send");
        json_set(p, "message", json_copy(arg));
        json_set_hdr(p, "notice", m);
        return T2C("ok", tmt2_ok_args(rid, p));
    }
    json_t *st = T2C("agent.steer", tmt2_a1("arg", arg));
    if (!tmt2_is_raise(st) && tmt2_truthy(st)) {
        char m[512]; snprintf(m, sizeof m, "\342\217\251 Steer queued \342\200\224 arrives after the next tool call: %s", shown);
        return T2C("exec_out", tmt2_a2("rid", rid, "output", json_string(m)));
    }
    json_t *p = json_object(); json_set_hdr(p, "type", "send"); json_set(p, "message", json_copy(arg));
    return T2C("ok", tmt2_ok_args(rid, p));
}

/* PoP: tmt2_cmd_goal @ tui_gateway/methods_tools.py:_cmd_goal */
static json_t *tmt2_cmd_goal(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *session = json_obj_get(args, "session");
    const json_t *arg = json_obj_get(args, "arg");
    json_t *ps = T2C("profile_scope", tmt2_a1("session", session)); T2_RET_IF_RAISE(ps);
    json_t *sk = T2C("session_key_or_err", tmt2_a2("module", json_string("hermes_cli.goals"), "label", json_string("goals")));
    T2_RET_IF_RAISE(sk);
    const json_t *er = (sk && sk->type == JSON_ARRAY && json_len(sk) > 2) ? json_get(sk, 2) : NULL;
    if (tmt2_truthy(er)) return json_copy(er);
    const json_t *skey = json_get(sk, 0);
    json_t *cfg = T2C("load_cfg", tmt2_a0());
    double max_turns = 20;
    if (!tmt2_is_raise(cfg)) {
        const json_t *g = json_obj_get(cfg, "goals");
        const json_t *mtv = (g && g->type == JSON_OBJECT) ? json_obj_get(g, "max_turns") : NULL;
        if (tmt2_truthy(mtv) && mtv->type == JSON_NUMBER) max_turns = mtv->num_val;
    }
    json_t *ma = json_object(); json_set(ma, "session_id", skey ? json_copy(skey) : json_null());
    json_set(ma, "default_max_turns", json_number(max_turns));
    json_t *mgr = T2C("hermes_cli.goals.GoalManager", ma); T2_RET_IF_RAISE(mgr);
    json_t *lu = T2C("hermes_cli.goals.last_user_message_from_db", tmt2_a1("session_id", skey)); T2_RET_IF_RAISE(lu);
    json_t *da = json_object(); json_set(da, "mgr", mgr ? json_copy(mgr) : json_null());
    json_set(da, "arg", arg ? json_copy(arg) : json_null());
    json_set(da, "last_user_message", lu ? json_copy(lu) : json_null());
    json_t *res = T2C("dispatch_goal_command", da); T2_RET_IF_RAISE(res);
    bool is_err = json_get_bool(res, "error", false);
    const json_t *output = json_obj_get(res, "output");
    if (is_err) return T2C("err", tmt2_err_args(rid, 4004, tmt2_sv(output) ? tmt2_sv(output) : "", NULL));
    const json_t *prompt = json_obj_get(res, "prompt");
    if (!tmt2_truthy(prompt)) return T2C("exec_out", tmt2_a2("rid", rid, "output", output ? json_copy(output) : json_string("")));
    json_t *p = json_object(); json_set_hdr(p, "type", "send");
    json_set(p, "notice", output ? json_copy(output) : json_null());
    json_set(p, "message", json_copy(prompt));
    if (!json_get_bool(res, "kickoff", true)) {
        json_t *nt = json_object(); json_set(nt, "notice", json_copy(output ? output : json_string("")));
        char *cur = tmt2_strdup(tmt2_sv(json_obj_get(p, "notice")) ? tmt2_sv(json_obj_get(p, "notice")) : "");
        char *add = malloc(strlen(cur) + 64); if (add) { snprintf(add, strlen(cur)+64, "%s\nContinuing now \342\200\224 taking the next step.", cur); json_set_hdr(p, "notice", add); free(add); }
        free(cur); json_free(nt);
        json_set_hdr(p, "display", "/goal resume");
    }
    return T2C("ok", tmt2_ok_args(rid, p));
}

/* PoP: tmt2_cmd_loop @ tui_gateway/methods_tools.py:_cmd_loop */
static json_t *tmt2_cmd_loop(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *session = json_obj_get(args, "session");
    const json_t *arg = json_obj_get(args, "arg");
    json_t *sk = T2C("session_key_or_err", tmt2_a2("module", json_string("hermes_cli.loops"), "label", json_string("loops")));
    T2_RET_IF_RAISE(sk);
    const json_t *er = (sk && sk->type == JSON_ARRAY && json_len(sk) > 2) ? json_get(sk, 2) : NULL;
    if (tmt2_truthy(er)) return json_copy(er);
    const json_t *skey = json_get(sk, 0);
    json_t *lm = T2C("hermes_cli.loops.LoopManager", tmt2_a1("session_id", skey)); T2_RET_IF_RAISE(lm);
    json_t *da = json_object(); json_set(da, "mgr", lm ? json_copy(lm) : json_null());
    json_set(da, "arg", arg ? json_copy(arg) : json_null());
    json_t *res = T2C("hermes_cli.loops.dispatch_loop_command", da); T2_RET_IF_RAISE(res);
    json_t *output = json_string(tmt2_sv(json_obj_get(res, "output")) ? tmt2_sv(json_obj_get(res, "output")) : "");
    if (json_get_bool(res, "created", false)) {
        json_t *gb = T2C("hermes_cli.loops.goal_blocks_loop_tick", tmt2_a1("session_id", skey));
        if (!tmt2_is_raise(gb) && tmt2_truthy(gb)) {
            char *o = malloc(strlen(tmt2_sv(output)) + 200);
            if (o) {
                snprintf(o, strlen(tmt2_sv(output))+200,
                    "%s\nNote: an active /goal is driving this session \342\200\224 loop wakeups defer until the goal finishes, pauses, or parks.",
                    tmt2_sv(output));
                json_free(output); output = json_string(o); free(o);
            }
        }
    }
    return T2C("exec_out", tmt2_a2("rid", rid, "output", output));
}

/* PoP: tmt2_cmd_undo @ tui_gateway/methods_tools.py:_cmd_undo */
static json_t *tmt2_cmd_undo(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *session = json_obj_get(args, "session");
    const json_t *arg = json_obj_get(args, "arg");
    if (!tmt2_truthy(session)) return T2C("err", tmt2_err_args(rid, 4001, "no active session to undo", NULL));
    json_t *b = T2C("busy_error", tmt2_a1("cmd", json_string("undo"))); T2_RET_IF_RAISE(b);
    if (tmt2_truthy(b)) return b;
    const json_t *skey = json_obj_get(session, "session_key");
    if (!tmt2_truthy(skey)) return T2C("err", tmt2_err_args(rid, 4001, "no session key for undo", NULL));
    char *astr = tmt2_str_strip(arg, NULL);
    int n = 1;
    if (*astr) {
        char *tok = tmt2_strdup(astr);
        char *p = tok; while (*p && !tmt2_ws((unsigned char)*p)) p++; *p = '\0';
        errno = 0; char *end = NULL;
        long pv = strtol(tok, &end, 10);
        if (end == tok || *end != '\0' || errno == ERANGE) {
            char m[256];
            snprintf(m, sizeof m, "undo: invalid count '%s' \342\200\224 use /undo or /undo N", astr);
            free(tok); free(astr);
            return T2C("err", tmt2_err_args(rid, 4004, m, NULL));
        }
        n = (int)(pv < 1 ? 1 : pv);
        free(tok);
    }
    json_t *pre = T2C("rewind_prelude", tmt2_a2("cmd", json_string("undo"), "empty", json_string("no user messages to undo")));
    T2_RET_IF_RAISE(pre);
    const json_t *uind = (pre && pre->type == JSON_ARRAY && json_len(pre) > 1) ? json_get(pre, 1) : NULL;
    if (!tmt2_truthy(uind) || json_len(uind) == 0)
        return T2C("err", tmt2_err_args(rid, 4018, "no user messages to undo", NULL));
    int tu = n; if (tu > (int)json_len(uind)) tu = (int)json_len(uind);
    json_t *ro = json_object();
    json_set(ro, "keep", json_number((double)(json_len(uind) - (size_t)tu)));
    json_set(ro, "code", json_number(4004)); json_set_hdr(ro, "prefix", "undo: ");
    json_t *rw = T2C("rewind_or_err", ro); T2_RET_IF_RAISE(rw);
    const json_t *er = (rw && rw->type == JSON_ARRAY && json_len(rw) > 1) ? json_get(rw, 1) : NULL;
    if (tmt2_truthy(er)) { free(astr); return json_copy(er); }
    const json_t *rr0 = json_get(rw, 0);
    const json_t *live = (rr0 && rr0->type == JSON_ARRAY && json_len(rr0) > 1) ? json_get(rr0, 1) : NULL;
    const json_t *content = (live && live->type == JSON_OBJECT) ? json_obj_get(live, "content") : NULL;
    json_t *txt = T2C("session_rules.turns.flatten_message_text", tmt2_a1("content", content)); T2_RET_IF_RAISE(txt);
    int rcount = (rr0 && rr0->type == JSON_ARRAY && json_len(rr0) > 2 && json_get(rr0,2)->type == JSON_NUMBER)
        ? (int)json_get(rr0, 2)->num_val : 0;
    /* agent hooks: three guarded steps, exceptions suppressed */
    json_t *mm = T2C("agent.attr", tmt2_as("name", "_memory_manager"));
    if (!tmt2_is_raise(mm) && tmt2_truthy(mm)) {
        json_t *hs = T2C("agent.on_session_switch", tmt2_a1("session_key", skey)); (void)hs;
        json_t *iv = T2C("agent.invalidate_system_prompt", tmt2_a0()); (void)iv;
        json_t *fl = T2C("agent.set_last_flushed", tmt2_an("len", (double)tu)); (void)fl;
    }
    json_t *fr = T2C("tui_model_friction", tmt2_a2("cmd", json_string("undo"), "session", session));
    if (tmt2_is_raise(fr)) { free(astr); return fr; }
    char notice[512];
    snprintf(notice, sizeof notice, "\342\206\266 Undid %d %s (%d message(s)). Edit and resubmit, or send a new message.",
             tu, (tu == 1 ? "turn" : "turns"), rcount);
    json_t *hint = T2C("tui_gateway.checkpoints.undo_rollback_hint_for", tmt2_a2("session", session, "live_view", live));
    if (!tmt2_is_raise(hint) && tmt2_truthy(hint)) {
        const json_t *ht = (hint && hint->type == JSON_OBJECT) ? json_obj_get(hint, "text") : NULL;
        strncat(notice, "\n", sizeof notice - strlen(notice) - 1);
        strncat(notice, tmt2_sv(ht) ? tmt2_sv(ht) : "", sizeof notice - strlen(notice) - 1);
    }
    json_t *p = json_object(); json_set_hdr(p, "type", "prefill");
    json_set(p, "message", txt ? json_copy(txt) : json_null());
    json_set_hdr(p, "notice", notice);
    free(astr);
    return T2C("ok", tmt2_ok_args(rid, p));
}

/* PoP: tmt2_cmd_compress @ tui_gateway/methods_tools.py:_cmd_compress */
static json_t *tmt2_cmd_compress(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    const json_t *session = json_obj_get(args, "session");
    if (!tmt2_truthy(session)) return T2C("err", tmt2_err_args(rid, 4001, "no active session to compress", NULL));
    json_t *b = T2C("busy_error", tmt2_a1("cmd", json_string("compress"))); T2_RET_IF_RAISE(b);
    if (tmt2_truthy(b)) return b;
    const json_t *sid = params ? json_obj_get(params, "session_id") : NULL;
    json_t *uch = T2C("session_uses_compute_host", tmt2_a1("session", session)); T2_RET_IF_RAISE(uch);
    if (tmt2_truthy(uch)) {
        json_t *ch = T2C("compute_host_slash", tmt2_a2("sid", sid, "cmd", json_string("compress"))); T2_RET_IF_RAISE(ch);
        const char *st = tmt2_sv(json_obj_get(ch, "status")) ? tmt2_sv(json_obj_get(ch, "status")) : "";
        const json_t *text = json_obj_get(ch, "text");
        if (!strcmp(st, "failed") || !strcmp(st, "rejected"))
            return T2C("err", tmt2_err_args(rid, !strcmp(st,"failed") ? 5019 : 4009, tmt2_sv(text)?tmt2_sv(text):"", NULL));
        if (!strcmp(st, "pending")) {
            json_t *p = json_object(); json_set_hdr(p, "type", "exec"); json_set_hdr(p, "status", "pending");
            json_set(p, "output", text ? json_copy(text) : json_null());
            return T2C("ok", tmt2_ok_args(rid, p));
        }
        return T2C("exec_out", tmt2_a2("rid", rid, "output", text ? json_copy(text) : json_string("")));
    }
    json_t *out = T2C("compress_live_with_feedback", tmt2_a1("session", session));
    if (tmt2_is_raise(out)) {
        const char *k = tmt2_raise_kind(out);
        if (tmt2_is_not_owned(k)) return T2C("session_not_owned_err", tmt2_a1("exc", json_obj_get(out, "message")));
        if (!strcmp(k, "CompressionBusy")) return T2C("err", tmt2_err_args(rid, 4009, json_get_str(out, "message", ""), NULL));
        json_t *f = T2C("agent.conversation_compression.finalize_context_engine_compression_notification", tmt2_a1("session", session)); (void)f;
        char m[256]; snprintf(m, sizeof m, "compress failed: %s", json_get_str(out, "message", ""));
        return T2C("err", tmt2_err_args(rid, 5009, m, NULL));
    }
    return T2C("exec_out", tmt2_a2("rid", rid, "output", out));
}

/* PoP: tmt2_pending_subcommand_store @ tui_gateway/methods_tools.py:_pending_subcommand_store */
static json_t *tmt2_pending_subcommand_store(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    if (tmt2_truthy(session)) {
        json_t *store = T2C("agent.attr", tmt2_as("name", "_memory_store"));
        if (!tmt2_is_raise(store) && tmt2_truthy(store)) return store;
    }
    json_t *st = T2C("tools.memory_tool.load_on_disk_store", tmt2_a0());
    if (tmt2_is_raise(st)) return json_null();
    return st;
}

/* PoP: tmt2_write_approval_mode_setter @ tui_gateway/methods_tools.py:_write_approval_mode_setter */
static json_t *tmt2_write_approval_mode_setter(tmt2_ctx_t *ctx, const json_t *args) {
    const char *sub = tmt2_sv(json_obj_get(args, "subsystem"));
    char key[256]; snprintf(key, sizeof key, "%s.write_approval", sub ? sub : "");
    json_t *m = json_object(); json_set_hdr(m, "__closure__", "write_approval_mode_setter");
    json_set_hdr(m, "key", key);
    return m;
}
static json_t *tmt2_write_approval_mode_setter_call(tmt2_ctx_t *ctx, const json_t *args) {
    const char *key = tmt2_sv(json_obj_get(args, "key")) ? tmt2_sv(json_obj_get(args, "key")) : "";
    bool enabled = tmt2_truthy(json_obj_get(args, "enabled"));
    json_t *w = json_object(); json_set_hdr(w, "key", key); json_set(w, "enabled", json_bool(enabled));
    return T2C("write_config_key", w);
}

/* PoP: tmt2_run_pending_review @ tui_gateway/methods_tools.py:_run_pending_review */
static json_t *tmt2_run_pending_review(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const char *sub = tmt2_sv(json_obj_get(args, "subsystem"));
    const json_t *arg = json_obj_get(args, "arg");
    const json_t *session = json_obj_get(args, "session");
    bool unknown_s = json_obj_get(args, "unknown") && tmt2_truthy(json_obj_get(args, "unknown"));
    const char *unknown = unknown_s ? tmt2_sv(json_obj_get(args, "unknown")) : "";
    json_t *store = NULL;
    if (sub && !strcmp(sub, "memory")) store = tmt2_pending_subcommand_store(ctx, tmt2_a1("session", session));
    json_t *sa = json_object(); json_set_hdr(sa, "subsystem", sub ? sub : "");
    json_set(sa, "arg", arg ? json_copy(arg) : json_null());
    json_set(sa, "memory_store", store ? json_copy(store) : json_null());
    json_set(sa, "set_mode_fn", json_string("write_approval_mode_setter"));
    json_t *out = T2C("handle_pending_subcommand", sa); T2_RET_IF_RAISE(out);
    json_t *o = tmt2_truthy(out) ? out : json_string(unknown ? unknown : "");
    return T2C("exec_out", tmt2_a2("rid", rid, "output", o));
}

/* PoP: tmt2_cmd_memory @ tui_gateway/methods_tools.py:_cmd_memory */
static json_t *tmt2_cmd_memory(tmt2_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    const json_t *rid = json_obj_get(args, "rid");
    if (rid) json_set(a, "rid", json_copy(rid));
    json_set_hdr(a, "subsystem", "memory");
    if (json_obj_get(args, "arg")) json_set(a, "arg", json_copy(json_obj_get(args, "arg")));
    if (json_obj_get(args, "session")) json_set(a, "session", json_copy(json_obj_get(args, "session")));
    if (json_obj_get(args, "unknown")) json_set(a, "unknown", json_copy(json_obj_get(args, "unknown")));
    return tmt2_run_pending_review(ctx, a);
}

/* PoP: tmt2_cmd_skills @ tui_gateway/methods_tools.py:_cmd_skills */
static json_t *tmt2_cmd_skills(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *arg = json_obj_get(args, "arg");
    const json_t *session = json_obj_get(args, "session");
    char *astr = tmt2_str_strip(arg, NULL);
    /* split() -> list */
    char *toks[64]; int nt = 0;
    for (char *p = astr; *p && nt < 64; ) {
        while (*p && tmt2_ws((unsigned char)*p)) p++;
        if (!*p) break; toks[nt++] = p;
        while (*p && !tmt2_ws((unsigned char)*p)) p++;
        if (*p) *p++ = '\0';
    }
    json_t *rc = T2C("hermes_cli.commands.resolve_command", tmt2_as("name", "skills")); T2_RET_IF_RAISE(rc);
    const json_t *allowed = (rc && rc->type == JSON_OBJECT) ? json_obj_get(rc, "desktop_subcommands") : NULL;
    if (!nt) { free(astr); return json_null(); }
    char *first = tmt2_lower(toks[0]);
    bool in_allowed = false;
    if (allowed && allowed->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(allowed); i++) { const char *s=tmt2_sv(json_get(allowed,i)); if (s && !strcmp(s, first)) { in_allowed=true; break; } }
    if (!in_allowed) { free(first); free(astr); return json_null(); }
    bool is_approval = (!strcmp(first, "approval") || !strcmp(first, "mode"));
    if (!is_approval) {
        bool enabled = json_get_bool(args, "wa_enabled", false);
        double pending = json_get_num(args, "wa_pending", 0);
        if (!enabled && pending == 0) {
            free(first); free(astr);
            return T2C("exec_out", tmt2_a2("rid", rid, "output", json_string(
                "Skill write approval is off (skills.write_approval). Enable it with /skills approval on, then review staged writes with /skills pending.")));
        }
    }
    free(first); free(astr);
    json_t *a = json_object();
    if (rid) json_set(a, "rid", json_copy(rid));
    json_set_hdr(a, "subsystem", "skills");
    json_set(a, "arg", arg ? json_copy(arg) : json_null());
    json_set(a, "session", session ? json_copy(session) : json_null());
    if (json_obj_get(args, "unknown")) json_set(a, "unknown", json_copy(json_obj_get(args, "unknown")));
    return tmt2_run_pending_review(ctx, a);
}

/* PoP: tmt2_slash_worker_for @ tui_gateway/methods_tools.py:_slash_worker_for */
static json_t *tmt2_slash_worker_for(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *sid = json_obj_get(args, "sid");
    const json_t *session = json_obj_get(args, "session");
    const json_t *w0 = (session && session->type == JSON_OBJECT) ? json_obj_get(session, "slash_worker") : NULL;
    if (tmt2_truthy(w0)) {
        json_t *r = json_array(); json_append(r, json_copy(w0)); json_append(r, json_null()); return r;
    }
    json_t *sp = T2C("sessions_lock", tmt2_a0());
    if (tmt2_is_raise(sp)) {
        json_t *e = T2C("err", tmt2_err_args(rid, 5030, json_get_str(sp, "message", ""), NULL));
        if (tmt2_is_raise(e)) return e;
        json_t *r = json_array(); json_append(r, json_null()); json_append(r, e); return r;
    }
    json_t *w1 = T2C("session.get", tmt2_as("key", "slash_worker")); T2_RET_IF_RAISE(w1);
    if (tmt2_truthy(w1)) { json_t *r = json_array(); json_append(r, w1); json_append(r, json_null()); return r; }
    json_t *model = T2C("agent.attr", tmt2_as("name", "model"));
    if (tmt2_is_raise(model) || !tmt2_truthy(model)) { json_t *rm = T2C("resolve_model", tmt2_a0()); T2_RET_IF_RAISE(rm); model = rm; }
    const json_t *prov = json_obj_get(session, "provider");
    const json_t *bu = json_obj_get(session, "base_url");
    json_t *ca = json_object();
    if (sid) json_set(ca, "session_key", json_copy(sid));
    json_set(ca, "model", model ? json_copy(model) : json_null());
    json_set(ca, "provider", prov ? json_copy(prov) : json_null());
    json_set(ca, "base_url", bu ? json_copy(bu) : json_null());
    json_t *w = T2C("slash_worker_ctor", ca);
    if (tmt2_is_raise(w)) {
        json_t *lw = T2C("logger.warning", tmt2_as("message", json_get_str(w, "message", ""))); (void)lw;
        json_t *e = T2C("err", tmt2_err_args(rid, 5030, "slash worker start failed", NULL));
        if (tmt2_is_raise(e)) return e;
        json_t *r = json_array(); json_append(r, json_null()); json_append(r, e); return r;
    }
    json_t *at = T2C("attach_worker", tmt2_a2("sid", sid, "worker", w)); T2_RET_IF_RAISE(at);
    json_t *rr = json_array(); json_append(rr, w); json_append(rr, json_null()); return rr;
}

/* PoP: tmt2_container_checkpoint_refusal @ tui_gateway/methods_tools.py:_container_checkpoint_refusal */
static json_t *tmt2_container_checkpoint_refusal(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    const json_t *cwd = json_obj_get(args, "cwd");
    const json_t *skey = (session && session->type == JSON_OBJECT) ? json_obj_get(session, "session_key") : NULL;
    const char *task = (tmt2_truthy(skey) ? tmt2_sv(skey) : NULL);
    const char *tid = task ? task : "default";
    json_t *sc = T2C("set_session_context", tmt2_a2("task_id", json_string(tid), "cwd", cwd)); T2_RET_IF_RAISE(sc);
    json_t *ps = T2C("profile_scope", tmt2_a1("session", session)); T2_RET_IF_RAISE(ps);
    json_t *reason = T2C("mgr.unsupported_backend_reason", tmt2_as("task_id", tid));
    json_t *cl = T2C("clear_session_context", tmt2_a1("tokens", sc));
    if (tmt2_is_raise(reason)) return reason;
    if (tmt2_is_raise(cl)) return cl;
    return reason;
}

/* PoP: tmt2_configure_session_tools @ tui_gateway/methods_tools.py:_configure_session_tools */
static json_t *tmt2_configure_session_tools(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    const json_t *sid = json_obj_get(args, "sid");
    const json_t *session = json_obj_get(args, "session");
    const json_t *av = json_obj_get(params, "action");
    char *act = tmt2_lower(tmt2_str_strip(av, NULL));
    if (strcmp(act, "disable") && strcmp(act, "enable")) {
        char m[128]; snprintf(m, sizeof m, "unknown tools action: %s", act); free(act);
        return T2C("err", tmt2_err_args(rid, 4017, m, NULL));
    }
    const json_t *names = json_obj_get(params, "names");
    json_t *targets = json_array();
    if (names && names->type == JSON_ARRAY) for (size_t i=0;i<json_len(names);i++) {
        const json_t *v = json_get(names, i); char *s = tmt2_strip(tmt2_sv(v));
        if (*s) json_append(targets, json_string(s)); free(s);
    }
    if (json_len(targets) == 0) { free(act); return T2C("err", tmt2_err_args(rid, 4018, "names required", NULL)); }
    json_t *hc = T2C("tools_mod", tmt2_as("module", "hermes_cli.config")); T2_RET_IF_RAISE(hc);
    json_t *tc = T2C("tools_mod", tmt2_as("module", "hermes_cli.tools_config")); T2_RET_IF_RAISE(tc);
    json_t *cfg = T2C("hermes_cli.config.load_config", tmt2_a0()); T2_RET_IF_RAISE(cfg);
    json_t *valid = T2C("hermes_cli.tools_config._get_plugin_toolset_keys", tmt2_a0()); T2_RET_IF_RAISE(valid);
    json_t *mcp = json_array(), *unknown = json_array(), *ts = json_array();
    for (size_t i=0;i<json_len(targets);i++) {
        const char *s = tmt2_sv(json_get(targets,i));
        if (strchr(s, ':')) json_append(mcp, json_copy(json_get(targets,i)));
        else {
            bool inv = false;
            if (valid && valid->type == JSON_ARRAY) for (size_t j=0;j<json_len(valid);j++){const char*v=tmt2_sv(json_get(valid,j));if(v&&!strcmp(v,s)){inv=true;break;}}
            if (inv) json_append(ts, json_copy(json_get(targets,i))); else json_append(unknown, json_copy(json_get(targets,i)));
        }
    }
    if (json_len(ts) > 0) {
        json_t *a = json_object(); json_set(a,"cfg",json_copy(cfg)); json_set_hdr(a,"scope","cli");
        json_set(a,"targets",json_copy(ts)); json_set_hdr(a,"action",act);
        json_t *r = T2C("hermes_cli.tools_config._apply_toolset_change", a); T2_RET_IF_RAISE(r);
    }
    json_t *msr = T2C("mcp_server_rows", tmt2_a0()); T2_RET_IF_RAISE(msr);
    const json_t *plugins = (msr && msr->type == JSON_ARRAY && json_len(msr)>1) ? json_get(msr,1) : NULL;
    for (size_t i=0;i<json_len(mcp);i++) {
        const char *srv = tmt2_sv(json_get(mcp,i)); const char *col = strchr(srv ? srv : "", ':');
        char nm[128]; if (col) snprintf(nm,sizeof nm,"%.*s",(int)(col-srv),srv); else snprintf(nm,sizeof nm,"%s",srv?srv:"");
        json_t *mwa = json_object();
        json_set(mwa, "rid", json_copy(rid));
        json_set_hdr(mwa, "server", nm);
        json_set(mwa, "plugins", plugins ? json_copy(plugins) : json_null());
        json_t *er = T2C("mcp_plugin_write_error", mwa);
        T2_RET_IF_RAISE(er); if (tmt2_truthy(er)) { free(act); return er; }
    }
    json_t *missing = json_array();
    if (json_len(mcp) > 0) {
        json_t *a = json_object(); json_set(a,"cfg",json_copy(cfg)); json_set(a,"targets",json_copy(mcp)); json_set_hdr(a,"action",act);
        json_t *r = T2C("hermes_cli.tools_config._apply_mcp_change", a); T2_RET_IF_RAISE(r);
        if (r && r->type == JSON_ARRAY) for (size_t i=0;i<json_len(r);i++) json_append(missing, json_copy(json_get(r,i)));
    }
    json_t *sv = T2C("hermes_cli.config.save_config", tmt2_a1("cfg", cfg)); T2_RET_IF_RAISE(sv);
    json_t *info = NULL;
    if (tmt2_truthy(session)) { info = T2C("reset_session_agent", tmt2_a2("sid", sid, "session", session)); T2_RET_IF_RAISE(info); }
    json_t *cfg2 = T2C("hermes_cli.config.load_config", tmt2_a0()); T2_RET_IF_RAISE(cfg2);
    json_t *en = T2C("hermes_cli.tools_config._get_platform_tools", tmt2_a1("cfg", cfg2)); T2_RET_IF_RAISE(en);
    json_t *enabled = json_array();
    if (en && en->type == JSON_ARRAY) {
        for (size_t i=0;i<json_len(en);i++) json_append(enabled, json_copy(json_get(en,i)));
        for (size_t i=0;i<json_len(enabled);i++) for (size_t j=i+1;j<json_len(enabled);j++)
            if (strcmp(tmt2_sv(json_get(enabled,i))?tmt2_sv(json_get(enabled,i)):"",tmt2_sv(json_get(enabled,j))?tmt2_sv(json_get(enabled,j)):"")>0) {
                json_t*t=enabled->c.items[i]; enabled->c.items[i]=enabled->c.items[j]; enabled->c.items[j]=t; }
    }
    json_t *changed = json_array();
    for (size_t i=0;i<json_len(targets);i++) {
        const char *s = tmt2_sv(json_get(targets,i));
        bool unk=false; for(size_t j=0;j<json_len(unknown);j++){const char*u=tmt2_sv(json_get(unknown,j));if(u&&!strcmp(u,s)){unk=true;break;}}
        bool miss=false; const char *col=strchr(s? s:"", ':');
        if (col) { char nm[128]; snprintf(nm,sizeof nm,"%.*s",(int)(col-s),s);
            for(size_t j=0;j<json_len(missing);j++){const char*u=tmt2_sv(json_get(missing,j));if(u&&!strcmp(u,nm)){miss=true;break;}} }
        if (!unk && !miss) json_append(changed, json_copy(json_get(targets,i)));
    }
    json_t *p = json_object(); json_set(p,"changed",changed); json_set(p,"enabled_toolsets",enabled);
    json_set(p,"info",info?json_copy(info):json_null()); json_set(p,"missing_servers",missing);
    json_set(p,"reset",json_bool(tmt2_truthy(session))); json_set(p,"unknown",unknown);
    free(act);
    return T2C("ok", tmt2_ok_args(rid, p));
}

/* PoP: tmt2_skills_install @ tui_gateway/methods_tools.py:_skills_install */
static json_t *tmt2_skills_install(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *query = json_obj_get(args, "query");
    json_t *va = json_object(); json_set(va,"query",json_copy(query)); json_set(va,"skip_confirm",json_bool(true));
    json_t *verdict = T2C("hermes_cli.skills_hub.do_install", va); T2_RET_IF_RAISE(verdict);
    bool t = (verdict && verdict->type == JSON_BOOL && verdict->bool_val);
    bool is_none = (!verdict || verdict->type == JSON_NULL);
    bool installed = t;
    if (!installed && is_none) {
        json_t *b = T2C("tools.skills_sync_bundled_ops.bundled_skill_for_install", tmt2_a1("query", query)); T2_RET_IF_RAISE(b);
        installed = tmt2_truthy(b);
    }
    if (!installed) {
        const json_t *log = json_obj_get(args, "capture_log");
        const char *ls = tmt2_sv(log) ? tmt2_sv(log) : "";
        /* last line of "\n".join(lines[-12:]).strip() */
        const char *last = strrchr(ls, '\n'); const char *tail = last ? last + 1 : ls;
        json_t *data = json_object(); json_set(data,"installed",json_bool(false));
        json_set(data,"name",json_copy(query)); json_set(data,"log",*ls?json_string(ls):json_null());
        return T2C("err", tmt2_err_args(rid, 5031, tail, data));
    }
    json_t *p = json_object(); json_set(p,"installed",json_bool(true)); json_set(p,"name",json_copy(query));
    return T2C("ok", tmt2_ok_args(rid, p));
}

/* PoP: tmt2_register @ tui_gateway/methods_tools.py:register */
static json_t *tmt2_register(tmt2_ctx_t *ctx, const json_t *args) {
    const json_t *server = json_obj_get(args, "server");
    json_t *a = json_object(); json_set(a,"server",server?json_copy(server):json_null());
    json_t *r = T2C("bind_module", a); T2_RET_IF_RAISE(r);
    return T2C("plugin_commands.register", tmt2_a1("server", server));
}

/* __PORT_END__ */
