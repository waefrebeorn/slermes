/*
 * gateway_gaps.c — Consolidated N/A annotations for gateway modules.
 *
 * Documents the status of all Python gateway modules not yet annotated.
 * Most are already ported to C (helpers.c, server.c, etc.) or are
 * inherently Python-only (async, ABC, SDK wrappers).
 *
 * See THIRD_PARTY.md §2h for full N/A module catalog with install guide.
 *
 * Already ported to C:
 *   delivery.py → helpers.c (send_result_failed, is_silence_narration, etc.)
 *   display_config.py → helpers.c (normalise_display_value)
 *   session_context.py → PORTED in api_server.c (set_session_vars via
 *     agent_state_t fields platform/session_id/chat_id/session_key)
 *     + hermes.h agent_state_t struct fields.
 *     clear via memset after LLM dispatch.
 *   whatsapp_identity.py → helpers.c (normalize_whatsapp_identifier, etc.)
 *   channel_directory.py → helpers.c (normalize_channel_query, session_entry_id)
 *   mirror.py → mirror.c (gw_mirror_*)
 *   runtime_footer.py → helpers.c (home_relative_cwd, model_short)
 *   sticker_cache.py → sticker_cache.c (gw_sticker_cache_*)
 *   slash_access.py → slash_access.c (gw_slash_access_*)
 *   shutdown_forensics.py → shutdown_forensics.c (gw_shutdown_*)
 *   restart.py → helpers.c (parse_restart_drain_timeout)
 *   memory_monitor.py → helpers.c (get_rss_mb, start/stop_memory_monitoring)
 *   session.py → server.c (build_session_context_prompt, is_shared_multi_user_session, etc.)
 *   run.py → server.c (process_update, gateway lifecycle) + helpers.c
 *
 * Python-only (async/SDK/ABC — NOT PORTABLE, SYNC EQUIVALENTS EXIST):
 *   stream_consumer.py → PORTED in server.c (gateway_stream_cb, stream_consumer_push/drain/flush)
 *   stream_dispatch.py → PORTED in stream_events.c (stream_event_dispatch())
 *   stream_events.py → PORTED in stream_events.c (MessageChunk, MessageStop, etc. event types)
 *   kanban_watchers.py → PORTED via cron/scheduler.c (synchronous kanban lifecycle)
 *   pairing.py → PORTED via pairing.c (synchronous code-exchange approval protocol)
 *   hooks.py → N/A, Python importlib plugin hooks registry
 *   status.py → N/A, Python gateway status aggregator
 *   config.py → N/A, Python gateway config loader (PyYAML + plugin discovery)
 *
 * Revived gap ports below use the universal-seam convention: every live I/O
 * enters through gwg_seams_t.call(ctx, name, args); args is owned by
 * gwg_call (freed after the borrowed call); results are NEW json_t with
 * NULL == Python None and {"__raise__":K,"msg":M} == a raised exception.
 * Attribute-bearing doubles (policy, store, connection, skin, cli) are json
 * objects; a MISSING key models AttributeError with the oracle's
 * AttrDouble text "missing attribute '<name>'". Path joins use as_posix
 * semantics (backslashes normalized to '/', trailing '/' stripped).
 */

#include "hermes_core_types.h"
#include "hermes_gateway_core.h"
#include "hermes_json.h"
#include "gateway_lifecycle_flow.h"

typedef struct gwg_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gwg_seams_t;

/* ── universal-seam helpers ── */

static json_t *gwg_call(const gwg_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool gwg_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *gwg_raise_kind(const json_t *r) {
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
/* BaseException kinds that `except Exception` does NOT catch. */
static bool gwg_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
static json_t *gwg_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static json_t *gwg_args(void) { return json_object(); }
static void gwg_aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void gwg_aset_b(json_t *a, const char *k, bool v) {
    json_set(a, k, json_bool(v));
}
static void gwg_aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const json_t *gwg_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static bool gwg_json_equal(const json_t *a, const json_t *b) {
    bool an = !a || a->type == JSON_NULL, bn = !b || b->type == JSON_NULL;
    if (an || bn) return an && bn;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "",
                                        b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!gwg_json_equal(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !gwg_json_equal(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}
/* str.strip() whitespace (ASCII subset + \x1c-\x1f + U+0085). */
static bool gwg_strip_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'
        || (c >= 0x1c && c <= 0x1f);
}
static char *gwg_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && gwg_strip_ws((unsigned char)s[a])) a++;
    while (b > a && gwg_strip_ws((unsigned char)s[b - 1])) b--;
    char *r = malloc(b - a + 1);
    if (!r) return NULL;
    memcpy(r, s + a, b - a);
    r[b - a] = '\0';
    return r;
}
/* int(str) digit run (see gateway_gaps2.c for the full semantics). */
static bool gwg_parse_int_str(const char *s, long long *out) {
    if (!s) return false;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\v' || *s == '\f' || *s == '\r') s++;
    bool neg = false;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    long long acc = 0;
    bool any = false, last_digit = false;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c >= '0' && c <= '9') {
            if (acc < 9000000000000000000LL) acc = acc * 10 + (c - '0');
            any = true; last_digit = true; s++;
        } else if (c == '_' && last_digit && s[1] >= '0' && s[1] <= '9') {
            last_digit = false; s++;
        } else break;
    }
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\v' || *s == '\f' || *s == '\r') s++;
    if (*s != '\0' || !any) return false;
    *out = neg ? -acc : acc;
    return true;
}
/* Path(home) / child as_posix: normalize separators, strip trailing '/'. */
static char *gwg_path_join(const char *home, const char *child) {
    if (!home) home = "";
    size_t hn = strlen(home);
    char *h = malloc(hn + 1);
    if (!h) return NULL;
    for (size_t i = 0; i < hn; i++) h[i] = (home[i] == '\\') ? '/' : home[i];
    h[hn] = '\0';
    while (hn > 0 && h[hn - 1] == '/') { h[--hn] = '\0'; }
    size_t cn = strlen(child);
    char *r = malloc(hn + cn + 2);
    if (!r) { free(h); return NULL; }
    if (hn == 0) {
        memcpy(r, child, cn + 1);
    } else {
        memcpy(r, h, hn);
        r[hn] = '/';
        memcpy(r + hn + 1, child, cn + 1);
    }
    free(h);
    return r;
}

/* PoP: gateway_plugin_keyless_inject_launched_by_classic_cli @ gateway/plugin_keyless_inject.py:_launched_by_classic_cli */
json_t *gateway_plugin_keyless_inject_launched_by_classic_cli(const json_t *runner,
        const json_t *live, const gwg_seams_t *s) {
    /* live.source is evaluated INSIDE the try, but only AuthorityRefusal is
     * caught: an AttributeError on a missing source propagates. */
    if (!live || live->type == JSON_NULL)
        return gwg_raise("AttributeError", "'NoneType' object has no attribute 'source'");
    const json_t *src = gwg_oget(live, "source");
    if (!src) return gwg_raise("AttributeError", "missing attribute 'source'");
    json_t *a = gwg_args();
    gwg_aset_j(a, "runner", runner);
    gwg_aset_j(a, "source", src);
    json_t *pol = gwg_call(s, "policy_for_source", a);
    if (gwg_raised(pol)) {
        if (strcmp(gwg_raise_kind(pol), "AuthorityRefusal") == 0) {
            json_free(pol);
            return json_bool(false);
        }
        return pol;
    }
    if (gflow_is_null(pol)) { json_free(pol); return json_bool(false); }
    const json_t *ps = gwg_oget(pol, "source");
    if (!ps) { json_free(pol); return gwg_raise("AttributeError", "missing attribute 'source'"); }
    bool is_cli = ps->type == JSON_STRING && ps->str_val && strcmp(ps->str_val, "cli") == 0;
    json_free(pol);
    return json_bool(is_cli);  /* policy is not None and policy.source == "cli" */
}

/* PoP: gateway_session_command_sink_active_sink @ gateway/session_command_sink.py:active_sink */
json_t *gateway_session_command_sink_active_sink(const gwg_seams_t *s) {
    /* _SINK.get() — the ContextVar (default None) is the seam. */
    return gwg_call(s, "_SINK.get", gwg_args());
}

/* _start_times_conflict — INTERNAL, real on both sides (gateway/status.py):
 * True only when BOTH start times are known and differ. */
static bool gwg_start_times_conflict(const json_t *recorded, const json_t *current) {
    return !gflow_is_null(recorded) && !gflow_is_null(current)
        && !gwg_json_equal(recorded, current);
}

/* PoP: gateway_status_scoped_locks_scoped_lock_record_is_stale @ gateway/status_scoped_locks.py:_scoped_lock_record_is_stale */
json_t *gateway_status_scoped_locks_scoped_lock_record_is_stale(const json_t *existing,
        const json_t *existing_pid, const gwg_seams_t *s) {
    json_t *cur_start = NULL;
    json_t *ret = NULL;
    if (!gflow_is_null(existing_pid)) {
        json_t *a = gwg_args();
        gwg_aset_j(a, "record", existing);
        json_t *ours = gwg_call(s, "scoped_lock_pid_is_ours", a);
        if (gwg_raised(ours)) return ours;
        bool is_ours = gflow_truth(ours);
        json_free(ours);
        if (!is_ours) {
            json_t *fa = gwg_args();
            gwg_aset_j(fa, "existing", existing);
            json_t *fl = gwg_call(s, "_foreign_owner_is_live", fa);
            if (gwg_raised(fl)) return fl;
            bool foreign_live = gflow_truth(fl);
            json_free(fl);
            if (!foreign_live) return json_bool(true);  /* its gateway.lock is free */
            json_t *la = gwg_args();
            gwg_aset_s(la, "fmt",
                "Scoped lock for %s is held by PID %s in another PID namespace (%s), whose gateway.lock "
                "in %s is held or cannot be checked; treating it as live.");
            json_t *args = json_array();
            const json_t *scope = gwg_oget(existing, "scope");
            json_append(args, gflow_truth(scope) ? json_copy(scope) : json_string("?"));
            json_append(args, json_copy(existing_pid));
            const json_t *pidns = gwg_oget(existing, "pidns");
            json_append(args, pidns ? json_copy(pidns) : json_null());
            const json_t *home = gwg_oget(existing, "hermes_home");
            json_append(args, gflow_truth(home) ? json_copy(home) : json_string("an unrecorded home"));
            json_set(la, "args", args);
            json_t *lr = gwg_call(s, "logger.warning", la);
            if (gwg_raised(lr)) return lr;
            json_free(lr);
            return json_bool(false);
        }
    }
    if (gflow_is_null(existing_pid)) return json_bool(true);
    {
        json_t *a = gwg_args();
        gwg_aset_j(a, "pid", existing_pid);
        json_t *ex = gwg_call(s, "_pid_exists", a);
        if (gwg_raised(ex)) return ex;
        bool exists = gflow_truth(ex);
        json_free(ex);
        if (!exists) return json_bool(true);
    }
    const json_t *recorded_start = gwg_oget(existing, "start_time");
    {
        json_t *a = gwg_args();
        gwg_aset_j(a, "pid", existing_pid);
        cur_start = gwg_call(s, "_get_process_start_time", a);
    }
    if (gwg_raised(cur_start)) return cur_start;
    if (gwg_start_times_conflict(recorded_start, cur_start)) { ret = json_bool(true); goto done; }
    {
        const json_t *metadata = gwg_oget(existing, "metadata");
        const json_t *ok = gwg_oget(metadata, "owner_kind");
        bool process_owner = gflow_is_obj(metadata) && ok && ok->type == JSON_STRING
            && ok->str_val && strcmp(ok->str_val, "process") == 0;
        bool looks = false;
        if (!process_owner) {
            json_t *a = gwg_args();
            gwg_aset_j(a, "pid", existing_pid);
            json_t *lk = gwg_call(s, "_looks_like_gateway_process", a);
            if (gwg_raised(lk)) { ret = lk; goto done; }
            looks = gflow_truth(lk);
            json_free(lk);
        }
        if (!process_owner && !looks) {
            json_t *a = gwg_args();
            gwg_aset_j(a, "pid", existing_pid);
            json_t *cmd = gwg_call(s, "_read_process_cmdline", a);
            if (gwg_raised(cmd)) { ret = cmd; goto done; }
            bool has_cmd = !gflow_is_null(cmd);  /* is not None */
            json_free(cmd);
            if (has_cmd) { ret = json_bool(true); goto done; }
            if (gflow_is_null(recorded_start) || gflow_is_null(cur_start)) {
                json_t *ra = gwg_args();
                gwg_aset_j(ra, "record", existing);
                json_t *rl = gwg_call(s, "_record_looks_like_gateway", ra);
                if (gwg_raised(rl)) { ret = rl; goto done; }
                bool rec_looks = gflow_truth(rl);
                json_free(rl);
                if (!rec_looks) { ret = json_bool(true); goto done; }
            }
        }
    }
    {
        json_t *a = gwg_args();
        gwg_aset_j(a, "pid", existing_pid);
        ret = gwg_call(s, "_process_is_stopped", a);
    }
done:
    json_free(cur_start);
    return ret;
}

/* PoP: gateway_systemd_stop_mark_resolve_target_pid @ gateway/systemd_stop_mark.py:_resolve_target_pid */
json_t *gateway_systemd_stop_mark_resolve_target_pid(const json_t *argv, const gwg_seams_t *s) {
    /* Explicit argv[1] (truthy), else $MAINPID; int(str(raw).strip()) > 0. */
    if (!argv || argv->type != JSON_ARRAY)
        return gwg_raise("TypeError", "object of type 'NoneType' has no len()");
    const json_t *raw = NULL;
    bool raw_owned = false;
    if (json_len(argv) > 1) {
        const json_t *a1 = json_get(argv, 1);
        if (gflow_truth(a1)) raw = a1;
    }
    if (!raw) {
        json_t *a = gwg_args();
        gwg_aset_s(a, "key", "MAINPID");
        raw = gwg_call(s, "os.environ.get", a);
        raw_owned = true;
        if (gwg_raised(raw)) return (json_t *)raw;
    }
    char *rs = gflow_pystr(raw);  /* str(raw): None -> "None", ints int-like */
    char *st = gwg_strip(rs);
    free(rs);
    long long pid = 0;
    bool ok = gwg_parse_int_str(st, &pid);
    free(st);
    if (raw_owned) json_free((json_t *)raw);
    if (!ok) return NULL;  /* TypeError/ValueError -> None */
    return pid > 0 ? json_number((double)pid) : NULL;
}

/* PoP: gateway_session_skin_resolved_skin @ gateway/session_skin.py:resolved_skin */
json_t *gateway_session_skin_resolved_skin(const gwg_seams_t *s) {
    json_t *home = NULL, *cfg = NULL, *skin = NULL, *ret = NULL;
    home = gwg_call(s, "get_hermes_home", gwg_args());
    if (gwg_raised(home)) {
        if (gwg_is_base_exc(gwg_raise_kind(home))) return home;
        goto fail;
    }
    {
        const char *hs = gflow_str(home);
        char *path = gwg_path_join(hs ? hs : "", "config.yaml");
        json_free(home); home = NULL;
        if (!path) goto fail;
        json_t *ca = gwg_args();
        gwg_aset_s(ca, "path", path);
        free(path);
        cfg = gwg_call(s, "load_user_config_effective", ca);
    }
    if (gwg_raised(cfg)) {
        if (gwg_is_base_exc(gwg_raise_kind(cfg))) return cfg;
        goto fail;
    }
    {
        json_t *ia = gwg_args();
        gwg_aset_j(ia, "config", cfg);
        json_t *ir = gwg_call(s, "init_skin_from_config", ia);
        json_free(cfg); cfg = NULL;
        if (gwg_raised(ir)) {
            if (gwg_is_base_exc(gwg_raise_kind(ir))) return ir;
            json_free(ir);
            goto fail;
        }
        json_free(ir);
    }
    skin = gwg_call(s, "get_active_skin", gwg_args());
    if (gwg_raised(skin)) {
        if (gwg_is_base_exc(gwg_raise_kind(skin))) return skin;
        goto fail;
    }
    {
        /* Every direct attribute must exist; a missing one is AttributeError
         * and boots the default theme ({}). Null VALUES are fine. */
        static const char *fields[] = {
            "name", "colors", "light_colors", "dark_colors", "branding",
            "banner_logo", "banner_hero", "tool_prefix", "custom_css"
        };
        for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
            if (!gwg_oget(skin, fields[i])) goto fail;
        }
        ret = json_object();
        json_set(ret, "name", json_copy(json_obj_get(skin, "name")));
        json_set(ret, "colors", json_copy(json_obj_get(skin, "colors")));
        json_set(ret, "light_colors", json_copy(json_obj_get(skin, "light_colors")));
        json_set(ret, "dark_colors", json_copy(json_obj_get(skin, "dark_colors")));
        json_set(ret, "branding", json_copy(json_obj_get(skin, "branding")));
        json_set(ret, "banner_logo", json_copy(json_obj_get(skin, "banner_logo")));
        json_set(ret, "banner_hero", json_copy(json_obj_get(skin, "banner_hero")));
        json_set(ret, "tool_prefix", json_copy(json_obj_get(skin, "tool_prefix")));
        /* help_header = (skin.branding or {}).get("help_header", "") */
        const json_t *br = json_obj_get(skin, "branding");
        if (gflow_truth(br)) {
            if (!gflow_is_obj(br)) { json_free(ret); ret = NULL; goto fail; }
            const json_t *hh = json_obj_get(br, "help_header");
            json_set(ret, "help_header", hh ? json_copy(hh) : json_string(""));
        } else {
            json_set(ret, "help_header", json_string(""));
        }
        json_set(ret, "customCSS", json_copy(json_obj_get(skin, "custom_css")));
    }
    json_free(skin);
    return ret;
fail:
    json_free(home);
    json_free(cfg);
    json_free(skin);
    json_free(ret);
    {
        json_t *la = gwg_args();
        gwg_aset_s(la, "fmt", "runtime.describe: skin resolution failed");
        gwg_aset_b(la, "exc_info", true);
        json_t *lr = gwg_call(s, "logger.debug", la);
        if (gwg_raised(lr)) return lr;
        json_free(lr);
    }
    return json_object();  /* {} — the client treats it as the default theme */
}

/* PoP: hermes_cli_gateway_side_connection_for @ hermes_cli/gateway_side.py:connection_for */
json_t *hermes_cli_gateway_side_connection_for(const json_t *cli, const gwg_seams_t *s) {
    json_t *a = gwg_args();
    gwg_aset_j(a, "cli", cli);
    json_t *store = gwg_call(s, "gateway_store", a);
    if (gwg_raised(store)) return store;
    if (gflow_is_null(store)) { json_free(store); return NULL; }
    /* getattr(cli, "_gateway_session_open", False) — missing attr is False. */
    const json_t *open = gwg_oget(cli, "_gateway_session_open");
    if (!gflow_truth(open)) { json_free(store); return NULL; }
    const json_t *conn = gwg_oget(store, "connection");
    if (!conn) { json_free(store); return gwg_raise("AttributeError", "missing attribute 'connection'"); }
    const json_t *fin = gwg_oget(conn, "final");
    if (!fin) { json_free(store); return gwg_raise("AttributeError", "missing attribute 'final'"); }
    if (gflow_truth(fin)) { json_free(store); return NULL; }  /* final -> None */
    json_t *r = json_copy(conn);  /* hung-up connections are returned */
    json_free(store);
    return r;
}
