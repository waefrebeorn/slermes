/*
 * session_busy_controls.c — faithful C11 port of upstream Hermes
 * gateway/session_busy_controls.py (gate w17_sessionbus_30eb49).
 *
 * PoP: w17_sessionbus_30eb49_handlers @ gateway/session_busy_controls.py:handlers
 * PoP: w17_sessionbus_30eb49_authorize @ gateway/session_busy_controls.py:authorize
 * PoP: w17_sessionbus_30eb49_verbose_config @ gateway/session_busy_controls.py:verbose_config
 * PoP: w17_sessionbus_30eb49_show_reasoning_config @ gateway/session_busy_controls.py:show_reasoning_config
 * PoP: w17_sessionbus_30eb49_service_tier_config @ gateway/session_busy_controls.py:service_tier_config
 * PoP: w17_sessionbus_30eb49_yolo_config @ gateway/session_busy_controls.py:yolo_config
 * PoP: w17_sessionbus_30eb49_adopts_owner_agent @ gateway/session_busy_controls.py:adopts_owner_agent
 * PoP: w17_sessionbus_30eb49_correct @ gateway/session_busy_controls.py:correct
 * PoP: w17_sessionbus_30eb49_busy_config @ gateway/session_busy_controls.py:busy_config
 *
 * Collaborators are scripted via `io`; observable seam arguments and mutated
 * live-session fields are recorded into `state`.
 */
#include "noc_w17_sessionbus_30eb49.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

static bool sbus_null(json_t *v) { return !v || v->type == JSON_NULL; }
static bool sbus_truthy(json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count != 0;
        case JSON_OBJECT: return v->c.count != 0;
    }
    return false;
}
static json_t *sbus_copy(json_t *v) { return v ? json_copy(v) : json_null(); }
static void sbus_record(json_t *state, const char *key, json_t *ev) {
    json_t *a = json_obj_get(state, key);
    if (!a) { a = json_array(); json_set(state, key, a); }
    json_append(a, ev);
}
static json_t *sbus_refusal(const char *reason) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string("AuthorityRefusal"));
    json_set(r, "reason", json_string(reason ? reason : ""));
    return r;
}
static const char *sbus_str(json_t *v) { return (v && v->type == JSON_STRING) ? v->str_val : NULL; }

/* Python str(value) for the fixture domain, then .strip().lower(); caller frees. */
static char *sbus_word(json_t *v) {
    char buf[128];
    if (!v || v->type == JSON_NULL) snprintf(buf, sizeof(buf), "None");
    else if (v->type == JSON_BOOL) snprintf(buf, sizeof(buf), "%s", v->bool_val ? "True" : "False");
    else if (v->type == JSON_NUMBER) {
        if (v->num_val == floor(v->num_val)) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%g", v->num_val);
    } else if (v->type == JSON_STRING) snprintf(buf, sizeof(buf), "%s", v->str_val ? v->str_val : "");
    else snprintf(buf, sizeof(buf), "");
    char *s = buf;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    for (size_t i = 0; i < n; i++) s[i] = (char)tolower((unsigned char)s[i]);
    return _strdup(s);
}

static bool sbus_in_strs(const char *s, const char *const *vals, size_t n) {
    if (!s) return false;
    for (size_t i = 0; i < n; i++) if (strcmp(s, vals[i]) == 0) return true;
    return false;
}
static const char *const VERBOSE_CYCLE[4] = {"off", "new", "all", "verbose"};
static const char *const BUSY_MODES[3] = {"interrupt", "steer", "queue"};

static json_t *sbus_live(json_t *conn, const char *sid) {
    return json_obj_get(json_obj_get(json_obj_get(conn, "authority"), "sessions"), sid);
}
static void sbus_live_state(json_t *state, json_t *live) {
    json_t *o = json_object();
    json_set(o, "tool_progress_mode", sbus_copy(json_obj_get(live, "tool_progress_mode")));
    json_set(o, "show_reasoning", sbus_copy(json_obj_get(live, "show_reasoning")));
    json_set(o, "busy_input_mode", sbus_copy(json_obj_get(live, "busy_input_mode")));
    json_set(state, "live", o);
}

/* PoP: handlers */
json_t *w17_sessionbus_30eb49_handlers(void) {
    json_t *a = json_array();
    json_append(a, json_string("config.get"));
    json_append(a, json_string("config.set"));
    json_append(a, json_string("session.redirect"));
    json_append(a, json_string("session.steer"));
    return a;
}

/* PoP: authorize */
json_t *w17_sessionbus_30eb49_authorize(json_t *conn, const char *sid, json_t *params,
        const char *capability, json_t *io, json_t *state) {
    json_t *ev = json_object();
    json_set(ev, "actor", sbus_copy(json_obj_get(conn, "actor")));
    json_set(ev, "ref", json_string(sid));
    json_set(ev, "capability", json_string(capability ? capability : ""));
    sbus_record(state, "authorize_calls", ev);
    json_t *a = json_obj_get(io, "authorize");
    if (json_is_object(a) && json_obj_get(a, "$raise"))
        return sbus_refusal(json_get_str(a, "reason", "refused"));
    json_t *profile = params ? json_obj_get(params, "profile") : NULL;
    if (!sbus_null(profile)) {
        if (profile->type != JSON_STRING) return sbus_refusal("invalid_params");
        if (sbus_truthy(profile)) {
            bool ok = json_get_bool(io, "profile_matches_home", true);
            if (!ok) return sbus_refusal("profile_mismatch");
        }
    }
    return NULL;
}

/* PoP: verbose_config */
json_t *w17_sessionbus_30eb49_verbose_config(json_t *conn, const char *sid, json_t *params,
        bool write, json_t *io, json_t *state) {
    json_t *value = params ? json_obj_get(params, "value") : NULL;
    const char *vs = sbus_str(value);
    if (write && !sbus_in_strs(vs, VERBOSE_CYCLE, 4) && !(vs && strcmp(vs, "cycle") == 0))
        return sbus_refusal("invalid_params");
    json_t *r = w17_sessionbus_30eb49_authorize(conn, sid, params, write ? "session:control" : "session:read", io, state);
    if (r) return r;
    json_t *live = sbus_live(conn, sid);
    json_t *cur = json_obj_get(live, "tool_progress_mode");
    const char *current = sbus_truthy(cur) ? sbus_str(cur) : "all";
    if (!current) current = "all";
    char *cc = _strdup(current);
    if (write) {
        if (vs && strcmp(vs, "cycle") == 0) {
            int idx = 0;
            for (int i = 0; i < 4; i++) if (strcmp(cc, VERBOSE_CYCLE[i]) == 0) { idx = i; break; }
            idx = (idx + 1) % 4;
            free(cc); cc = _strdup(VERBOSE_CYCLE[idx]);
        } else {
            free(cc); cc = _strdup(vs ? vs : "");
        }
        json_set(live, "tool_progress_mode", json_string(cc));
    }
    sbus_live_state(state, live);
    json_t *out = json_object();
    json_set(out, "key", json_string("verbose"));
    json_set(out, "value", json_string(cc));
    json_set(out, "scope", json_string("session"));
    free(cc);
    return out;
}

/* PoP: show_reasoning_config */
json_t *w17_sessionbus_30eb49_show_reasoning_config(json_t *conn, const char *sid, json_t *params,
        bool write, json_t *io, json_t *state) {
    json_t *value = params ? json_obj_get(params, "value") : NULL;
    char *word = sbus_word(value);
    bool in_words = sbus_in_strs(word, (const char *const[]){"1","on","true","0","off","false"}, 6);
    if (write && !in_words) { free(word); return sbus_refusal("invalid_params"); }
    json_t *r = w17_sessionbus_30eb49_authorize(conn, sid, params, write ? "session:control" : "session:read", io, state);
    free(word);
    if (r) return r;
    json_t *live = sbus_live(conn, sid);
    if (write) {
        char *w2 = sbus_word(value);
        bool truth = sbus_in_strs(w2, (const char *const[]){"1","on","true"}, 3);
        free(w2);
        json_set(live, "show_reasoning", json_bool(truth));
    }
    json_t *shown = json_obj_get(live, "show_reasoning");
    if (sbus_null(shown)) shown = json_obj_get(io, "launch_shows_reasoning");
    bool one = shown && shown->type == JSON_BOOL && shown->bool_val;
    sbus_live_state(state, live);
    json_t *out = json_object();
    json_set(out, "key", json_string("show_reasoning"));
    json_set(out, "value", json_string(one ? "1" : "0"));
    json_set(out, "scope", json_string("session"));
    return out;
}

/* PoP: service_tier_config */
json_t *w17_sessionbus_30eb49_service_tier_config(json_t *conn, const char *sid, json_t *params,
        json_t *io, json_t *state) {
    json_t *raw = params ? json_obj_get(params, "value") : NULL;
    const char *tier = NULL;
    if (raw && raw->type == JSON_STRING) {
        json_t *spec = json_obj_get(json_obj_get(io, "service_tier"), raw->str_val);
        if (json_is_object(spec) && json_obj_get(spec, "$raise")) tier = NULL;
        else tier = sbus_str(spec);
    }
    if (!tier) return sbus_refusal("invalid_params");
    json_t *r = w17_sessionbus_30eb49_authorize(conn, sid, params, "session:control", io, state);
    if (r) return r;
    json_t *live = sbus_live(conn, sid);
    const char *route = sbus_str(json_obj_get(live, "route"));
    json_t *ev = json_object();
    json_set(ev, "route", json_string(route ? route : ""));
    json_set(ev, "tier", json_string(tier));
    sbus_record(state, "set_tier_calls", ev);
    json_t *ev2 = json_object();
    json_set(ev2, "session_id", json_string(sid));
    json_t *payload = json_object();
    json_set(payload, "service_tier", json_string(tier));
    json_set(ev2, "payload", payload);
    json_set(ev2, "event_type", json_string("session.info"));
    sbus_record(state, "publish_calls", ev2);
    json_t *out = json_object();
    json_set(out, "key", json_string("service_tier"));
    json_set(out, "value", json_string(tier));
    json_set(out, "scope", json_string("session"));
    return out;
}

/* PoP: yolo_config */
json_t *w17_sessionbus_30eb49_yolo_config(json_t *conn, const char *sid, json_t *params,
        bool write, json_t *io, json_t *state) {
    json_t *value = params ? json_obj_get(params, "value") : NULL;
    char *word = sbus_word(value);
    bool in_words = sbus_in_strs(word, (const char *const[]){"1","on","true","0","off","false"}, 6);
    if (write && !sbus_null(value) && !in_words) { free(word); return sbus_refusal("invalid_params"); }
    json_t *r = w17_sessionbus_30eb49_authorize(conn, sid, params, write ? "session:control" : "session:read", io, state);
    if (r) { free(word); return r; }
    json_t *live = sbus_live(conn, sid);
    const char *route = sbus_str(json_obj_get(live, "route"));
    json_t *policy = json_obj_get(io, "policy");
    bool policy_yolo = policy && json_is_true(json_obj_get(policy, "yolo"));
    json_t *entry_yolo = json_obj_get(io, "entry_yolo");
    json_t *ev = json_object();
    json_set(ev, "route", json_string(route ? route : ""));
    json_set(ev, "entry_yolo", sbus_copy(entry_yolo));
    json_set(ev, "launch", json_bool(policy_yolo));
    sbus_record(state, "yolo_seams", ev);
    bool persisted = entry_yolo && entry_yolo->type == JSON_BOOL && entry_yolo->bool_val;
    bool enabled = json_get_bool(io, "yolo_enabled", false) || persisted;
    if (write) {
        json_t *ev2 = json_object();
        json_set(ev2, "route", json_string(route ? route : ""));
        json_set(ev2, "value", (sbus_null(value) || !in_words) ? json_null() : json_bool(
            sbus_in_strs(word, (const char *const[]){"1","on","true"}, 3)));
        sbus_record(state, "yolo_seams", ev2);
        enabled = json_get_bool(io, "toggle", false);
    }
    free(word);
    json_t *out = json_object();
    json_set(out, "key", json_string("yolo"));
    json_set(out, "value", json_string(enabled ? "1" : "0"));
    json_set(out, "scope", json_string("session"));
    return out;
}

/* PoP: adopts_owner_agent */
json_t *w17_sessionbus_30eb49_adopts_owner_agent(json_t *authority, const char *sid,
        json_t *io, json_t *state) {
    (void)state;
    json_t *policy = json_obj_get(io, "policy");
    const char *src = policy ? sbus_str(json_obj_get(policy, "source")) : NULL;
    if (src && strcmp(src, "cron") == 0) return json_bool(false);
    json_t *mp = json_obj_get(io, "managed_policy");
    if (json_is_object(mp) && json_obj_get(mp, "$raise")) return json_bool(false);
    return json_bool(sbus_null(mp));
}

/* PoP: correct */
json_t *w17_sessionbus_30eb49_correct(json_t *conn, const char *sid, json_t *params,
        const char *verb, json_t *io, json_t *state) {
    static const char *const ALLOWED[4] = {"session_id","profile","text","execution_generation"};
    if (!params || params->type != JSON_OBJECT) return sbus_refusal("invalid_params");
    for (size_t i = 0; i < json_object_size(params); i++) {
        const char *k = json_object_get_key_at(params, i);
        if (!sbus_in_strs(k, ALLOWED, 4)) return sbus_refusal("invalid_params");
    }
    json_t *text = json_obj_get(params, "text");
    if (!text || text->type != JSON_STRING) return sbus_refusal("invalid_params");
    char *t = _strdup(text->str_val ? text->str_val : "");
    char *p = t; while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\0') { free(t); return sbus_refusal("invalid_params"); }
    free(t);
    json_t *gen = json_obj_get(params, "execution_generation");
    if (!gen || gen->type != JSON_NUMBER || gen->num_val != floor(gen->num_val)) return sbus_refusal("invalid_params");

    json_t *r = w17_sessionbus_30eb49_authorize(conn, sid, params, "session:control", io, state);
    if (r) return r;
    if (json_get_bool(io, "voice_stop", false)) {
        json_t *ev = json_object(); json_set(ev, "session_id", json_string(sid));
        sbus_record(state, "voice_stop_calls", ev);
        json_t *o = json_object(); json_set(o, "voice_stopped", json_bool(true)); return o;
    }
    json_t *authority = json_obj_get(conn, "authority");
    json_t *epoch = json_obj_get(authority, "epoch");
    json_t *ev = json_object();
    json_set(ev, "session_id", json_string(sid));
    json_set(ev, "generation", sbus_copy(gen));
    sbus_record(state, "check_generation_calls", ev);
    json_t *cg = json_obj_get(io, "check_generation");
    if (json_is_object(cg) && json_obj_get(cg, "$raise"))
        return sbus_refusal(json_get_str(cg, "reason", "generation_mismatch"));
    json_t *workers = json_obj_get(authority, "_managed_workers");
    if (workers && workers->type == JSON_ARRAY) {
        for (size_t i = 0; i < workers->c.count; i++) {
            const char *w = sbus_str(workers->c.items[i]);
            if (w && strcmp(w, sid) == 0) return sbus_refusal("unsupported_control");
        }
    }
    json_t *sa = json_obj_get(io, "steer_agent");
    json_t *ev2 = json_object();
    json_set(ev2, "session_id", json_string(sid));
    json_set(ev2, "generation", sbus_copy(gen));
    json_set(ev2, "text", (verb && strcmp(verb, "steer") == 0) ? sbus_copy(text) : json_null());
    sbus_record(state, "steer_calls", ev2);
    bool latched = json_is_true(json_obj_get(sa, "latched"));
    bool has_agent = json_is_true(json_obj_get(sa, "agent"));
    if (latched) {
        json_t *o = json_object();
        json_set(o, "status", json_string("queued"));
        json_set(o, "text", sbus_copy(text));
        json_set(o, "execution_generation", sbus_copy(gen));
        json_set(o, "authority_epoch", sbus_copy(epoch));
        return o;
    }
    if (!has_agent) {
        json_t *o = json_object();
        json_set(o, "status", json_string("rejected"));
        json_set(o, "text", sbus_copy(text));
        json_set(o, "execution_generation", sbus_copy(gen));
        json_set(o, "authority_epoch", sbus_copy(epoch));
        json_set(o, "delivered", json_bool(false));
        return o;
    }
    json_t *ev3 = json_object();
    json_set(ev3, "text", sbus_copy(text));
    sbus_record(state, "agent_method_calls", ev3);
    bool accepted = json_get_bool(io, "agent_accept", false);
    const char *status = accepted ? (strcmp(verb, "steer") == 0 ? "queued" : "redirected") : "rejected";
    json_t *o = json_object();
    json_set(o, "status", json_string(status));
    json_set(o, "text", sbus_copy(text));
    json_set(o, "execution_generation", sbus_copy(gen));
    json_set(o, "authority_epoch", sbus_copy(epoch));
    return o;
}

/* PoP: busy_config */
json_t *w17_sessionbus_30eb49_busy_config(json_t *conn, const char *sid, json_t *params,
        bool write, json_t *io, json_t *state) {
    const char *key = sbus_str(params ? json_obj_get(params, "key") : NULL);
    if (write && key && strcmp(key, "model") == 0) return sbus_refusal("use_session_mutation_model");
    bool extra = false;
    if (params && params->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(params); i++) {
            const char *k = json_object_get_key_at(params, i);
            bool ok = !strcmp(k, "session_id") || !strcmp(k, "profile") || !strcmp(k, "key") ||
                      (write && !strcmp(k, "value"));
            if (!ok) { extra = true; break; }
        }
    }
    bool sid_ok = sid && sid[0] != '\0';
    if (key && sid_ok && !extra) {
        if (!strcmp(key, "verbose")) return w17_sessionbus_30eb49_verbose_config(conn, sid, params, write, io, state);
        if (!strcmp(key, "show_reasoning")) return w17_sessionbus_30eb49_show_reasoning_config(conn, sid, params, write, io, state);
        if (!strcmp(key, "service_tier") && write) return w17_sessionbus_30eb49_service_tier_config(conn, sid, params, io, state);
        if (!strcmp(key, "yolo")) return w17_sessionbus_30eb49_yolo_config(conn, sid, params, write, io, state);
    }
    json_t *value = params ? json_obj_get(params, "value") : NULL;
    const char *vs = sbus_str(value);
    if (extra || !sid_ok || !key || strcmp(key, "busy") != 0 ||
        (write && !sbus_in_strs(vs, BUSY_MODES, 3)))
        return sbus_refusal("invalid_params");
    json_t *r = w17_sessionbus_30eb49_authorize(conn, sid, params, write ? "session:control" : "session:read", io, state);
    if (r) return r;
    json_t *live = sbus_live(conn, sid);
    if (write) json_set(live, "busy_input_mode", sbus_copy(value));
    const char *v = NULL;
    json_t *cur = json_obj_get(live, "busy_input_mode");
    if (!sbus_null(cur)) v = sbus_str(cur);
    if (!v) {
        json_t *policy = json_obj_get(io, "policy");
        const char *src = policy ? sbus_str(json_obj_get(policy, "source")) : NULL;
        const char *fallback = (src && !strcmp(src, "tui")) ? "queue" : "interrupt";
        if (policy) {
            json_t *d = json_obj_get(json_obj_get(policy, "config"), "display");
            const char *dv = sbus_str(json_obj_get(d, "busy_input_mode"));
            v = (dv && dv[0]) ? dv : fallback;
        } else {
            v = sbus_str(json_obj_get(io, "effective_mode"));
            if (!v) v = fallback;
        }
        if (!sbus_in_strs(v, BUSY_MODES, 3)) v = fallback;
    }
    sbus_live_state(state, live);
    json_t *out = json_object();
    json_set(out, "key", json_string("busy"));
    json_set(out, "value", json_string(v));
    json_set(out, "scope", json_string("session"));
    return out;
}
