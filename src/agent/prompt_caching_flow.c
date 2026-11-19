/* prompt_caching_flow.c — agent/prompt_caching.py FLOW + leaves.
 *
 * Decision-sequence port of the Anthropic prompt-caching strategy module:
 * the one-shot / part-marker predicates (is_one_shot_request, _has_part_marker,
 * _text_part), the scaffold split / re-canonicalisation flows (_scaffold_parts,
 * _split_unmarked_scaffolds, _strip_prior_decoration), the auto-TTL resolution
 * chain (auto_cache_ttl_for_source, auto_cache_ttl_for_agent, auto_start_cache_ttl,
 * refresh_auto_cache_ttl), the route-name / catalog / ceiling resolution
 * (_route_names, _catalog_route, catalog_cache_markers, learned_or_catalog_markers,
 * route_ttl_ceiling), the _tool_run_end nested closure of the claimed
 * _completed_transaction_endpoint_indexes, and the PromptCachePlan frozen
 * dataclass methods (__init__, __eq__, __setattr__, __delattr__, __hash__).
 *
 * ALL external collaborators ride the injected universal seam (pc_call); the
 * harness scripts results and records the ordered trace the oracle diffs against
 * LIVE agent/prompt_caching.py under the same scripts. Ported siblings called by
 * a target (_has_part_marker, _text_part, _scaffold_parts, _route_names,
 * auto_cache_ttl_for_source, auto_cache_ttl_for_agent, auto_start_cache_ttl,
 * catalog_cache_markers) are transcribed real (internal pc_* calls), matching the
 * oracle's AST-extracted live defs. The CLAIMED sibling strip_anthropic_cache_control
 * is SEAMED. No target catches exceptions: every seam raise propagates
 * (BaseException passthrough) — no exception-parent table is needed.
 *
 * Representation contract (shared with tests/oracle_prompt_caching_flow.py):
 *   - scripts: MISSING key -> fail-closed RuntimeError("no script for <name>");
 *     null -> None; {"$seq":[...]} per-call queue (exhaustion raises
 *     RuntimeError("script exhausted: <name>")); {"__raise__":cls,...} raises
 *     (trace ret is the WHOLE marker); any other value reused (a bare ARRAY is a
 *     LIST RESULT, not a queue). None-valued seam arg keys dropped on BOTH sides.
 *   - get_provider_profile / active_policy results are attribute dicts (the oracle
 *     materializes SimpleNamespace doubles; C reads the JSON fields). fetch /
 *     load_config_readonly / strip results stay raw dicts/lists (.get / [0]).
 *   - agent doubles are attribute dicts; writes land in the echoed state. The
 *     mutation window (agent / messages) is echoed on RAISE too (partial history).
 *   - PromptCachePlan instances project as {messages, tools} (field order);
 *     NotImplemented -> {"__notimplemented__":true}; integral floats collapse to
 *     ints (libjson %.0f); uncaught exception -> {"__raise__":"<ClassName>"}.
 *   - bool<->int cross-equality holds in pc_json_eq (Python True == 1, 0 == False).
 *
 * Fixture domain (ASCII scope):
 *   - strings ASCII (str.strip/lower/slice transcribed byte-wise; strip set is
 *     White_Space incl \x1c-\x1f).
 *   - request_json is strict json.dumps-produced JSON (no JSONDecodeError paths).
 *   - profile.name / model / api_key / source / infer result are str-or-null.
 *   - _tool_run_end start is a non-negative integer (live call domain).
 *   - PromptCachePlan.__hash__ fields carry >=1 list/dict (unhashable); the
 *     all-scalar hash domain (randomized str hash) is excluded.
 *   - strip results: list / null / str / dict (subscript outcomes); num/bool out
 *     of domain.
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "agent/prompt_caching_flow.c".
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct pc_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} pc_ctx;

static bool pc_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool pc_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool pc_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool pc_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool pc_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool pc_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }

static bool pc_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
/* String value of a VALUE node (never call json_get_str on a value node). */
static const char *pc_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* Deep Python-style equality; NULL pointer and JSON_NULL both read None.
 * Object key order irrelevant (dict ==); array order matters. bool<->number
 * cross-equality (True == 1, 0 == False). */
static bool pc_json_eq(const json_t *a, const json_t *b) {
    if (pc_is_none(a) && pc_is_none(b)) return true;
    if (pc_is_none(a) || pc_is_none(b)) return false;
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER)
        return (a->bool_val ? 1.0 : 0.0) == b->num_val;
    if (a->type == JSON_NUMBER && b->type == JSON_BOOL)
        return a->num_val == (b->bool_val ? 1.0 : 0.0);
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return pc_str_eq(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!pc_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !pc_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

/* ── exception plumbing (no catches: every raise propagates) ─────────────── */

static bool pc_exc_pending(const pc_ctx *ctx) { return ctx && ctx->exc != NULL; }

static void pc_raise(pc_ctx *ctx, const char *cls) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
}
static void pc_raise_msg(pc_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg));
}

/* ── universal seam (fail-closed) ────────────────────────────────────────── */

/* args ownership is TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *pc_call(pc_ctx *ctx, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *script = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    json_t *chosen = NULL;
    json_t *fc_marker = NULL;   /* owned fail-closed / exhaustion marker */
    if (!script) {              /* MISSING key -> fail-closed */
        fc_marker = json_object();
        json_set(fc_marker, "__raise__", json_string("RuntimeError"));
        char msg[256];
        snprintf(msg, sizeof(msg), "no script for %s", name);
        json_set(fc_marker, "msg", json_string(msg));
        chosen = fc_marker;
    } else if (script->type == JSON_OBJECT) {
        json_t *seq = json_obj_get(script, "$seq");
        if (seq && seq->type == JSON_ARRAY) {
            double cur = json_get_num(ctx->counters, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= seq->c.count) {
                fc_marker = json_object();
                json_set(fc_marker, "__raise__", json_string("RuntimeError"));
                char msg[256];
                snprintf(msg, sizeof(msg), "script exhausted: %s", name);
                json_set(fc_marker, "msg", json_string(msg));
                chosen = fc_marker;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        } else {
            chosen = script;
        }
    } else {
        chosen = script;
    }
    json_t *ret = NULL;
    bool raises = false;
    if (fc_marker) {
        if (ctx->exc) json_free(ctx->exc);
        ctx->exc = json_copy(fc_marker);
        raises = true;
    } else if (chosen && chosen->type == JSON_OBJECT) {
        json_t *r = json_obj_get(chosen, "__raise__");
        if (r && r->type == JSON_STRING) {
            if (ctx->exc) json_free(ctx->exc);
            ctx->exc = json_copy(chosen);
            raises = true;
        } else {
            ret = json_copy(chosen);
        }
    } else if (chosen && chosen->type != JSON_NULL) {
        ret = json_copy(chosen);
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args);
    if (raises) json_set(ev, "ret", json_copy(ctx->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    if (fc_marker) json_free(fc_marker);
    return ret;
}

/* Arg builders: None-valued keys dropped on BOTH sides ("" is kept). */
static void pc_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void pc_arg_node(json_t *args, const char *key, const json_t *val) {
    if (!pc_is_none(val)) json_set(args, key, json_copy(val));
}

/* ── attribute-double accessors ──────────────────────────────────────────── */

/* getattr(obj, key, default) — non-object / absent reads the default (NULL). */
static json_t *pc_getattr_def(json_t *obj, const char *key) {
    if (!pc_is_obj(obj)) return NULL;
    return json_obj_get(obj, key);
}
/* Direct obj.key read — non-object / absent raises AttributeError. Borrowed. */
static json_t *pc_dattr(pc_ctx *ctx, json_t *obj, const char *key) {
    if (!pc_is_obj(obj) || !json_has(obj, key)) {
        pc_raise(ctx, "AttributeError");
        return NULL;
    }
    return json_obj_get(obj, key);
}
/* obj.key = val (val owned, transferred). Non-object raises AttributeError. */
static bool pc_setattr(pc_ctx *ctx, json_t *obj, const char *key, json_t *val) {
    if (!pc_is_obj(obj)) {
        pc_raise(ctx, "AttributeError");
        json_free(val);
        return false;
    }
    json_set(obj, key, val);
    return true;
}

/* ── ASCII string helpers (Python str.strip/lower/slice fixture domain) ──── */

static char *pc_dup_span(const char *s, size_t len) {
    char *r = (char *)malloc(len + 1);
    if (!r) return NULL;
    memcpy(r, s, len);
    r[len] = '\0';
    return r;
}
static bool pc_is_ws(char c) {
    unsigned char u = (unsigned char)c;
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'
        || (u >= 0x1c && u <= 0x1f);
}
static char *pc_py_strip(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && pc_is_ws(s[a])) a++;
    while (b > a && pc_is_ws(s[b - 1])) b--;
    return pc_dup_span(s + a, b - a);
}
static char *pc_py_lower(const char *s) {
    char *r = pc_dup_span(s, strlen(s));
    if (!r) return NULL;
    for (char *p = r; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return r;
}
static char *pc_str_slice(const char *s, size_t start) {
    size_t n = strlen(s);
    if (start >= n) return pc_dup_span("", 0);
    return pc_dup_span(s + start, n - start);
}

/* (provider or "").strip().lower() — truthy non-str raises AttributeError. */
static char *pc_provider_norm(pc_ctx *ctx, const json_t *provider) {
    const char *s = "";
    if (pc_truthy(provider)) {
        if (!pc_is_str(provider)) { pc_raise(ctx, "AttributeError"); return NULL; }
        s = provider->str_val ? provider->str_val : "";
    }
    char *st = pc_py_strip(s);
    char *lo = pc_py_lower(st);
    free(st);
    return lo;
}

/* ── live constant tables ────────────────────────────────────────────────── */

static bool pc_in_machine_set(const char *s) {
    static const char *T[] = {
        "subagent", "cron", "oneshot", "webhook", "kanban", "api", "api_server",
        "tool", "batch", "curator", "a2a", "msgraph_webhook", "gateway_hygiene",
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (pc_str_eq(T[i], s)) return true;
    return false;
}
static bool pc_in_ceiling_tiers(const json_t *v) {
    return pc_is_str(v) && (pc_str_eq(pc_jstr(v), "5m") || pc_str_eq(pc_jstr(v), "1h"));
}

/* ── _text_part / _has_part_marker (pure leaves) ─────────────────────────── */

/* PoP: pc_text_part @ agent/prompt_caching.py:_text_part */
static json_t *pc_text_part(const json_t *text, const json_t *cache_marker) {
    json_t *part = json_object();
    json_set(part, "type", json_string("text"));
    json_set(part, "text", text ? json_copy(text) : json_null());
    if (!pc_is_none(cache_marker))
        json_set(part, "cache_control", json_copy(cache_marker));
    return part;
}

/* PoP: pc_has_part_marker @ agent/prompt_caching.py:_has_part_marker */
static bool pc_has_part_marker(const json_t *content) {
    if (!pc_is_arr(content)) return false;
    for (size_t i = 0; i < content->c.count; i++) {
        json_t *p = content->c.items[i];
        if (pc_is_obj(p) && json_has(p, "cache_control")) return true;
    }
    return false;
}

/* ── is_one_shot_request ─────────────────────────────────────────────────── */

/* PoP: pc_is_one_shot @ agent/prompt_caching.py:is_one_shot_request */
static json_t *pc_is_one_shot(pc_ctx *ctx, json_t *messages, json_t *tools) {
    if (pc_truthy(tools)) return json_bool(false);
    /* msgs = [m for m in messages or [] if isinstance(m, dict)] */
    json_t *msgs = json_array();
    json_t *src = pc_truthy(messages) ? messages : NULL;
    if (src) {
        if (pc_is_arr(src)) {
            for (size_t i = 0; i < src->c.count; i++)
                if (pc_is_obj(src->c.items[i])) json_append(msgs, src->c.items[i]);
        } else if (pc_is_obj(src) || pc_is_str(src)) {
            /* dict keys / str chars are never dicts -> all filtered out */
        } else {
            pc_raise(ctx, "TypeError");
            json_free(msgs);
            return NULL;
        }
    }
    for (size_t i = 0; i < msgs->c.count; i++) {
        json_t *m = msgs->c.items[i];
        if (json_has(m, "cache_control") || pc_has_part_marker(json_obj_get(m, "content"))) {
            json_free(msgs);
            return json_bool(false);
        }
    }
    long users = 0;
    bool has_ast = false;
    for (size_t i = 0; i < msgs->c.count; i++) {
        const char *role = pc_jstr(json_obj_get(msgs->c.items[i], "role"));
        if (pc_str_eq(role, "user")) users++;
        if (pc_str_eq(role, "assistant") || pc_str_eq(role, "tool")) has_ast = true;
    }
    json_free(msgs);
    return json_bool(users <= 1 && !has_ast);
}

/* ── _scaffold_parts / _split_unmarked_scaffolds ─────────────────────────── */

/* PoP: pc_scaffold_parts @ agent/prompt_caching.py:_scaffold_parts */
static json_t *pc_scaffold_parts(pc_ctx *ctx, json_t *msg, const json_t *cache_marker) {
    json_t *content = pc_getattr_def(msg, "content");
    json_t *prefix = NULL;
    const char *role = pc_jstr(pc_getattr_def(msg, "role"));
    if (pc_str_eq(role, "user") && pc_is_str(content)) {
        json_t *a = json_object();
        pc_arg_node(a, "content", content);
        prefix = pc_call(ctx, "find_stable_prefix", a);
        if (pc_exc_pending(ctx)) { if (prefix) json_free(prefix); return NULL; }
    }
    if (pc_is_none(prefix)) { if (prefix) json_free(prefix); return NULL; }
    if (!pc_is_str(prefix)) {
        pc_raise(ctx, "TypeError");   /* len(prefix) on a non-str */
        json_free(prefix);
        return NULL;
    }
    char *tail = pc_str_slice(pc_jstr(content), strlen(pc_jstr(prefix)));
    json_t *arr = json_array();
    json_append(arr, pc_text_part(prefix, cache_marker));
    json_t *tn = json_string(tail ? tail : "");
    json_append(arr, pc_text_part(tn, NULL));
    json_free(tn);
    free(tail);
    json_free(prefix);
    return arr;
}

/* PoP: pc_split_unmarked_scaffolds @ agent/prompt_caching.py:_split_unmarked_scaffolds */
static void pc_split_unmarked_scaffolds(pc_ctx *ctx, json_t *messages) {
    if (!pc_is_arr(messages)) { pc_raise(ctx, "TypeError"); return; }
    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *msg = messages->c.items[i];
        if (!pc_is_obj(msg)) continue;
        json_t *parts = pc_scaffold_parts(ctx, msg, NULL);
        if (pc_exc_pending(ctx)) { if (parts) json_free(parts); return; }
        if (pc_truthy(parts)) {
            json_t *nm = json_copy(msg);
            json_set(nm, "content", parts);   /* parts owned, transferred */
            json_free(messages->c.items[i]);
            messages->c.items[i] = nm;
        } else if (parts) {
            json_free(parts);
        }
    }
}

/* ── _strip_prior_decoration (claimed sibling strip_anthropic_cache_control seamed) ── */

/* res[0] with Python subscript semantics across JSON types. Owned result. */
static json_t *pc_subscript0(pc_ctx *ctx, json_t *res) {
    if (pc_is_arr(res)) {
        if (res->c.count == 0) { pc_raise(ctx, "IndexError"); return NULL; }
        return json_copy(res->c.items[0]);
    }
    if (pc_is_none(res)) { pc_raise(ctx, "TypeError"); return NULL; }
    if (pc_is_str(res)) {
        const char *s = pc_jstr(res);
        if (s[0] == '\0') { pc_raise(ctx, "IndexError"); return NULL; }
        char buf[2] = { s[0], '\0' };
        return json_string(buf);
    }
    if (pc_is_obj(res)) { pc_raise(ctx, "KeyError"); return NULL; }
    pc_raise(ctx, "TypeError");
    return NULL;
}

/* PoP: pc_strip_prior_decoration @ agent/prompt_caching.py:_strip_prior_decoration */
static json_t *pc_strip_prior_decoration(pc_ctx *ctx, json_t *messages) {
    if (!pc_is_arr(messages)) { pc_raise(ctx, "TypeError"); return NULL; }
    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *msg = messages->c.items[i];
        if (pc_is_obj(msg) &&
            (json_has(msg, "cache_control") || pc_is_arr(json_obj_get(msg, "content")))) {
            json_t *lst = json_array();
            json_append(lst, json_copy(msg));
            json_t *a = json_object();
            pc_arg_node(a, "messages", lst);
            json_free(lst);
            json_t *res = pc_call(ctx, "strip_anthropic_cache_control", a);
            if (pc_exc_pending(ctx)) { if (res) json_free(res); return NULL; }
            json_t *elem = pc_subscript0(ctx, res);
            json_free(res);
            if (pc_exc_pending(ctx)) { if (elem) json_free(elem); return NULL; }
            json_free(messages->c.items[i]);
            messages->c.items[i] = elem;
        }
    }
    return messages;
}

/* ── auto TTL resolution chain ───────────────────────────────────────────── */

/* PoP: pc_auto_cache_ttl_for_source @ agent/prompt_caching.py:auto_cache_ttl_for_source */
static json_t *pc_auto_cache_ttl_for_source(pc_ctx *ctx, const json_t *source) {
    const char *s = "";
    if (pc_truthy(source)) {
        if (!pc_is_str(source)) { pc_raise(ctx, "AttributeError"); return NULL; }
        s = source->str_val ? source->str_val : "";
    }
    char *st = pc_py_strip(s);
    char *lo = pc_py_lower(st);
    bool machine = pc_in_machine_set(lo);
    free(st);
    free(lo);
    return json_string(machine ? "5m" : "1h");
}

/* json.loads on a strict-JSON string fixture (domain: json.dumps output). */
static json_t *pc_json_loads(pc_ctx *ctx, const json_t *s) {
    if (!pc_is_str(s)) { pc_raise(ctx, "TypeError"); return NULL; }
    json_t *parsed = json_parse(pc_jstr(s), NULL);
    if (!parsed) { pc_raise(ctx, "JSONDecodeError"); return NULL; }
    return parsed;
}

/* PoP: pc_auto_cache_ttl_for_agent @ agent/prompt_caching.py:auto_cache_ttl_for_agent */
static json_t *pc_auto_cache_ttl_for_agent(pc_ctx *ctx, json_t *agent) {
    if (!pc_setattr(ctx, agent, "_cache_ttl_auto", json_bool(true))) return NULL;
    json_t *finite = pc_call(ctx, "finite_turn_required", json_object());
    if (pc_exc_pending(ctx)) { if (finite) json_free(finite); return NULL; }
    json_t *policy = pc_call(ctx, "active_policy", json_object());
    if (pc_exc_pending(ctx)) { json_free(finite); if (policy) json_free(policy); return NULL; }
    if (pc_is_none(finite) && !pc_is_none(policy)) {
        json_t *rj = pc_dattr(ctx, policy, "request_json");
        if (pc_exc_pending(ctx)) { json_free(finite); json_free(policy); return NULL; }
        json_t *parsed = pc_json_loads(ctx, rj);
        if (pc_exc_pending(ctx)) { json_free(finite); json_free(policy); if (parsed) json_free(parsed); return NULL; }
        if (!pc_is_obj(parsed)) {
            pc_raise(ctx, "AttributeError");   /* parsed.get on a non-dict */
            json_free(finite); json_free(policy); json_free(parsed);
            return NULL;
        }
        json_t *tv = json_obj_get(parsed, "turn_v1");   /* .get("turn_v1", {}) */
        /* default arg policy.source == "oneshot" is evaluated EAGERLY */
        json_t *src = pc_dattr(ctx, policy, "source");
        if (pc_exc_pending(ctx)) { json_free(finite); json_free(policy); json_free(parsed); return NULL; }
        bool def_finite = pc_str_eq(pc_jstr(src), "oneshot");
        json_t *tvobj = tv ? tv : json_object();        /* the {} default */
        bool tvobj_owned = (tv == NULL);
        if (!pc_is_obj(tvobj)) {
            pc_raise(ctx, "AttributeError");   /* None.get / list.get */
            json_free(finite); json_free(policy); json_free(parsed);
            if (tvobj_owned) json_free(tvobj);
            return NULL;
        }
        json_t *fin = json_obj_get(tvobj, "finite");
        json_free(finite);
        finite = fin ? json_copy(fin) : json_bool(def_finite);
        if (tvobj_owned) json_free(tvobj);
        json_free(parsed);
    }
    bool is_true = finite && finite->type == JSON_BOOL && finite->bool_val;
    json_t *ttl;
    if (is_true) {
        ttl = json_string("5m");
    } else {
        json_t *platform = pc_getattr_def(agent, "platform");
        json_t *a = json_object();
        pc_arg_node(a, "platform", platform);
        json_t *srcres = pc_call(ctx, "session_source_for", a);
        if (pc_exc_pending(ctx)) {
            json_free(finite); json_free(policy); if (srcres) json_free(srcres);
            return NULL;
        }
        ttl = pc_auto_cache_ttl_for_source(ctx, srcres);
        json_free(srcres);
        if (pc_exc_pending(ctx)) { json_free(finite); json_free(policy); if (ttl) json_free(ttl); return NULL; }
    }
    json_free(finite);
    json_free(policy);
    return ttl;
}

/* PoP: pc_auto_start_cache_ttl @ agent/prompt_caching.py:auto_start_cache_ttl */
static json_t *pc_auto_start_cache_ttl(pc_ctx *ctx, json_t *agent) {
    json_t *resolved = pc_auto_cache_ttl_for_agent(ctx, agent);
    if (pc_exc_pending(ctx)) { if (resolved) json_free(resolved); return NULL; }
    json_t *a = json_object();
    pc_arg_node(a, "agent", agent);
    pc_arg_node(a, "resolved", resolved);
    json_t *r = pc_call(ctx, "adaptive_start_tier", a);
    json_free(resolved);
    if (pc_exc_pending(ctx)) { if (r) json_free(r); return NULL; }
    return r;
}

/* PoP: pc_refresh_auto_cache_ttl @ agent/prompt_caching.py:refresh_auto_cache_ttl */
static void pc_refresh_auto_cache_ttl(pc_ctx *ctx, json_t *agent) {
    json_t *auto_flag = pc_getattr_def(agent, "_cache_ttl_auto");
    json_t *side = pc_getattr_def(agent, "side_agent");
    if (pc_truthy(auto_flag) && !pc_truthy(side)) {
        json_t *adaptive = pc_getattr_def(agent, "_cache_ttl_adaptive");
        bool upgraded = false;
        if (pc_truthy(adaptive)) {
            json_t *ttl = pc_dattr(ctx, agent, "_cache_ttl");
            if (pc_exc_pending(ctx)) return;
            upgraded = pc_str_eq(pc_jstr(ttl), "1h");
        }
        json_t *newttl = pc_auto_start_cache_ttl(ctx, agent);
        if (pc_exc_pending(ctx)) { if (newttl) json_free(newttl); return; }
        if (!pc_setattr(ctx, agent, "_cache_ttl", newttl)) return;
        if (upgraded) {
            json_t *ad2 = pc_dattr(ctx, agent, "_cache_ttl_adaptive");
            if (pc_exc_pending(ctx)) return;
            if (pc_truthy(ad2)) {
                if (!pc_setattr(ctx, agent, "_cache_ttl", json_string("1h"))) return;
            }
        }
    }
}

/* ── route names / catalog / ceiling ─────────────────────────────────────── */

static bool pc_names_contains(json_t *names, const json_t *cand) {
    for (size_t i = 0; i < names->c.count; i++)
        if (pc_json_eq(names->c.items[i], cand)) return true;
    return false;
}

/* PoP: pc_route_names @ agent/prompt_caching.py:_route_names */
static json_t *pc_route_names(pc_ctx *ctx, json_t *provider, json_t *base_url) {
    json_t *names = json_array();
    char *first = pc_provider_norm(ctx, provider);
    if (pc_exc_pending(ctx)) { free(first); json_free(names); return NULL; }
    json_t *second = NULL;
    if (pc_truthy(base_url)) {
        json_t *a = json_object();
        pc_arg_node(a, "base_url", base_url);
        second = pc_call(ctx, "_infer_provider_from_url", a);
        if (pc_exc_pending(ctx)) { free(first); json_free(names); if (second) json_free(second); return NULL; }
    }
    for (int k = 0; k < 2; k++) {
        json_t *name_node = (k == 0) ? json_string(first ? first : "") : second;
        json_t *profile = NULL;
        if (pc_truthy(name_node)) {
            json_t *a = json_object();
            pc_arg_node(a, "name", name_node);
            profile = pc_call(ctx, "get_provider_profile", a);
            if (pc_exc_pending(ctx)) {
                if (k == 0) json_free(name_node);
                free(first); if (second) json_free(second);
                json_free(names); if (profile) json_free(profile);
                return NULL;
            }
        }
        json_t *pname = pc_getattr_def(profile, "name");
        json_t *cands[2] = { name_node, pname };
        for (int j = 0; j < 2; j++) {
            json_t *cand = cands[j];
            if (pc_truthy(cand) && !pc_names_contains(names, cand))
                json_append(names, json_copy(cand));
        }
        if (k == 0) json_free(name_node);
        if (profile) json_free(profile);
    }
    free(first);
    if (second) json_free(second);
    return names;
}

/* PoP: pc_catalog_route @ agent/prompt_caching.py:_catalog_route */
static json_t *pc_catalog_route(pc_ctx *ctx, json_t *provider, json_t *base_url) {
    json_t *names = pc_route_names(ctx, provider, base_url);
    if (pc_exc_pending(ctx)) { if (names) json_free(names); return NULL; }
    bool any = false;
    for (size_t i = 0; i < names->c.count; i++) {
        json_t *a = json_object();
        pc_arg_node(a, "name", names->c.items[i]);
        json_t *profile = pc_call(ctx, "get_provider_profile", a);
        if (pc_exc_pending(ctx)) { json_free(names); if (profile) json_free(profile); return NULL; }
        json_t *flag = pc_getattr_def(profile, "prompt_cache_markers_from_catalog");
        if (pc_truthy(flag)) { any = true; json_free(profile); break; }
        json_free(profile);
    }
    json_free(names);
    return json_bool(any);
}

/* PoP: pc_catalog_cache_markers @ agent/prompt_caching.py:catalog_cache_markers */
static json_t *pc_catalog_cache_markers(pc_ctx *ctx, json_t *provider, json_t *base_url,
                                        json_t *model, json_t *api_key) {
    json_t *names = pc_route_names(ctx, provider, base_url);
    if (pc_exc_pending(ctx)) { if (names) json_free(names); return NULL; }
    json_t *profile = NULL;
    for (size_t i = 0; i < names->c.count; i++) {
        json_t *a = json_object();
        pc_arg_node(a, "name", names->c.items[i]);
        json_t *p = pc_call(ctx, "get_provider_profile", a);
        if (pc_exc_pending(ctx)) { json_free(names); if (p) json_free(p); return NULL; }
        json_t *flag = pc_getattr_def(p, "prompt_cache_markers_from_catalog");
        if (pc_truthy(flag)) { profile = p; break; }
        json_free(p);
    }
    json_free(names);
    if (pc_is_none(profile) || !pc_truthy(model)) {
        if (profile) json_free(profile);
        return json_null();
    }
    json_t *burl;
    if (pc_truthy(base_url)) {
        burl = base_url;
    } else {
        burl = pc_dattr(ctx, profile, "base_url");
        if (pc_exc_pending(ctx)) { json_free(profile); return NULL; }
    }
    json_t *a = json_object();
    pc_arg_node(a, "base_url", burl);
    pc_arg_str(a, "api_key", pc_truthy(api_key) ? pc_jstr(api_key) : "");
    json_t *res = pc_call(ctx, "fetch_endpoint_model_metadata", a);
    if (pc_exc_pending(ctx)) { json_free(profile); if (res) json_free(res); return NULL; }
    if (!pc_is_obj(res)) {
        pc_raise(ctx, "AttributeError");   /* res.get on a non-dict */
        json_free(profile); json_free(res);
        return NULL;
    }
    json_t *row = json_obj_get(res, pc_jstr(model));
    if (!pc_is_obj(row)) { json_free(profile); json_free(res); return json_null(); }
    json_t *pricing = json_obj_get(row, "pricing");
    if (!pc_is_obj(pricing)) { json_free(profile); json_free(res); return json_null(); }
    json_t *cw = json_obj_get(pricing, "cache_write");
    bool in = pc_is_none(cw)
        || (pc_is_str(cw) && (pc_str_eq(pc_jstr(cw), "") || pc_str_eq(pc_jstr(cw), "0")));
    json_free(profile);
    json_free(res);
    return json_bool(!in);
}

/* PoP: pc_learned_or_catalog_markers @ agent/prompt_caching.py:learned_or_catalog_markers */
static json_t *pc_learned_or_catalog_markers(pc_ctx *ctx, json_t *provider, json_t *base_url,
                                             json_t *model, json_t *api_key) {
    json_t *a = json_object();
    pc_arg_node(a, "provider", provider);
    pc_arg_node(a, "base_url", base_url);
    pc_arg_node(a, "model", model);
    json_t *learned = pc_call(ctx, "marker_preference", a);
    if (pc_exc_pending(ctx)) { if (learned) json_free(learned); return NULL; }
    if (!pc_is_none(learned)) return learned;
    json_free(learned);
    return pc_catalog_cache_markers(ctx, provider, base_url, model, api_key);
}

/* PoP: pc_route_ttl_ceiling @ agent/prompt_caching.py:route_ttl_ceiling */
static json_t *pc_route_ttl_ceiling(pc_ctx *ctx, json_t *provider, json_t *base_url) {
    json_t *names = pc_route_names(ctx, provider, base_url);
    if (pc_exc_pending(ctx)) { if (names) json_free(names); return NULL; }
    json_t *cfg = pc_call(ctx, "load_config_readonly", json_object());
    if (pc_exc_pending(ctx)) { json_free(names); if (cfg) json_free(cfg); return NULL; }
    if (!pc_is_obj(cfg)) {
        pc_raise(ctx, "AttributeError");   /* cfg.get on a non-dict */
        json_free(names); json_free(cfg);
        return NULL;
    }
    json_t *section = json_obj_get(cfg, "prompt_caching");
    json_t *overrides = pc_is_obj(section) ? json_obj_get(section, "route_ttl_ceilings") : NULL;
    for (size_t i = 0; i < names->c.count; i++) {
        const char *name = pc_jstr(names->c.items[i]);
        if (pc_is_obj(overrides) && json_has(overrides, name)) {
            json_t *value = json_obj_get(overrides, name);
            json_t *ret = pc_in_ceiling_tiers(value) ? json_copy(value) : json_null();
            json_free(names); json_free(cfg);
            return ret;
        }
    }
    for (size_t i = 0; i < names->c.count; i++) {
        json_t *a = json_object();
        pc_arg_node(a, "name", names->c.items[i]);
        json_t *p = pc_call(ctx, "get_provider_profile", a);
        if (pc_exc_pending(ctx)) { json_free(names); json_free(cfg); if (p) json_free(p); return NULL; }
        json_t *ceiling = pc_getattr_def(p, "prompt_cache_max_ttl");
        if (pc_in_ceiling_tiers(ceiling)) {
            json_t *ret = json_copy(ceiling);
            json_free(p); json_free(names); json_free(cfg);
            return ret;
        }
        json_free(p);
    }
    json_free(names);
    json_free(cfg);
    return json_null();
}

/* ── _tool_run_end (nested closure; free var messages passed explicitly) ─── */

/* PoP: pc_tool_run_end @ agent/prompt_caching.py:_tool_run_end */
static json_t *pc_tool_run_end(json_t *messages, double start) {
    double end = start;
    double len = pc_is_arr(messages) ? (double)messages->c.count : 0.0;
    while (end < len) {
        json_t *m = messages->c.items[(size_t)end];
        if (!pc_is_obj(m)) break;
        if (!pc_str_eq(pc_jstr(json_obj_get(m, "role")), "tool")) break;
        end += 1.0;
    }
    return json_number(end);
}

/* ── PromptCachePlan frozen dataclass methods ────────────────────────────── */

static json_t *pc_notimplemented(void) {
    json_t *o = json_object();
    json_set(o, "__notimplemented__", json_bool(true));
    return o;
}

/* PoP: pc_plan_init @ agent/prompt_caching.py:PromptCachePlan.__init__ */
static json_t *pc_plan_init(pc_ctx *ctx, json_t *args, json_t *kwargs) {
    size_t nargs = pc_is_arr(args) ? args->c.count : 0;
    if (nargs > 2) { pc_raise(ctx, "TypeError"); return NULL; }
    bool has_msg = nargs >= 1, has_tools = nargs >= 2;
    json_t *msg_val = nargs >= 1 ? args->c.items[0] : NULL;
    json_t *tools_val = nargs >= 2 ? args->c.items[1] : NULL;
    if (pc_is_obj(kwargs)) {
        for (size_t i = 0; i < kwargs->c.count; i++) {
            const char *key = kwargs->c.keys[i];
            json_t *val = kwargs->c.items[i];
            if (pc_str_eq(key, "messages")) {
                if (has_msg) { pc_raise(ctx, "TypeError"); return NULL; }
                has_msg = true; msg_val = val;
            } else if (pc_str_eq(key, "tools")) {
                if (has_tools) { pc_raise(ctx, "TypeError"); return NULL; }
                has_tools = true; tools_val = val;
            } else {
                pc_raise(ctx, "TypeError"); return NULL;
            }
        }
    }
    if (!has_msg || !has_tools) { pc_raise(ctx, "TypeError"); return NULL; }
    json_t *o = json_object();
    json_set(o, "messages", msg_val ? json_copy(msg_val) : json_null());
    json_set(o, "tools", tools_val ? json_copy(tools_val) : json_null());
    return o;
}

/* PoP: pc_plan_eq @ agent/prompt_caching.py:PromptCachePlan.__eq__ */
static json_t *pc_plan_eq(pc_ctx *ctx, json_t *slf, json_t *other) {
    (void)ctx;
    const char *kind = pc_jstr(json_obj_get(other, "kind"));
    if (!pc_str_eq(kind, "plan")) return pc_notimplemented();
    json_t *of = json_obj_get(other, "fields");
    bool eq = pc_json_eq(json_obj_get(slf, "messages"), json_obj_get(of, "messages"))
           && pc_json_eq(json_obj_get(slf, "tools"), json_obj_get(of, "tools"));
    return json_bool(eq);
}

/* PoP: pc_plan_setattr @ agent/prompt_caching.py:PromptCachePlan.__setattr__ */
static json_t *pc_plan_setattr(pc_ctx *ctx, json_t *name) {
    (void)name;
    pc_raise(ctx, "FrozenInstanceError");   /* type(self) is cls: any name */
    return NULL;
}

/* PoP: pc_plan_delattr @ agent/prompt_caching.py:PromptCachePlan.__delattr__ */
static json_t *pc_plan_delattr(pc_ctx *ctx, json_t *name) {
    (void)name;
    pc_raise(ctx, "FrozenInstanceError");
    return NULL;
}

/* PoP: pc_plan_hash @ agent/prompt_caching.py:PromptCachePlan.__hash__ */
static json_t *pc_plan_hash(pc_ctx *ctx, json_t *fields) {
    json_t *m = json_obj_get(fields, "messages");
    json_t *t = json_obj_get(fields, "tools");
    if (pc_is_arr(m) || pc_is_obj(m) || pc_is_arr(t) || pc_is_obj(t)) {
        pc_raise(ctx, "TypeError");   /* unhashable type: 'list'/'dict' */
        return NULL;
    }
    pc_raise_msg(ctx, "RuntimeError", "hash fixture domain: scalar fields excluded");
    return NULL;
}
