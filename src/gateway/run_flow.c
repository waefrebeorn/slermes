/*
 * run_flow.c — message-handling FLOW cores of gateway/run.py (LIVE authority).
 *
 * Ports decision sequences: call order, branch conditions, early returns,
 * exception->fallback paths and mutation order. Every live effect (rendezvous
 * store reads/writes, profile/config reads, cache access, executor lifecycle,
 * i18n, logging, print, process exit) enters through the seam structs declared
 * in gateway_run_flow.h; the cores below perform no I/O.
 *
 * Python semantics honored: truthiness of dict.get results, `or` fallbacks,
 * short-circuit evaluation order (seam calls happen exactly when live would
 * call them), str() of scalars, code-point slicing, dict insertion order.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "gateway_run_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── internal helpers (no Python counterpart; semantics documented inline) ── */

/* Python truthiness for a JSON value (NULL pointer == Python None). */
static bool jtruth(const json_t *v) {
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

/* Python == over JSON values (None == None, True == 1, deep containers). */
static bool jeq(const json_t *a, const json_t *b) {
    json_type_t ta = a ? a->type : JSON_NULL;
    json_type_t tb = b ? b->type : JSON_NULL;
    if (ta == JSON_NULL || tb == JSON_NULL) return ta == tb;
    if (ta == JSON_BOOL && tb == JSON_BOOL) return a->bool_val == b->bool_val;
    if (ta == JSON_NUMBER && tb == JSON_NUMBER) return a->num_val == b->num_val;
    if (ta == JSON_BOOL && tb == JSON_NUMBER) return (double)a->bool_val == b->num_val;
    if (ta == JSON_NUMBER && tb == JSON_BOOL) return a->num_val == (double)b->bool_val;
    if (ta == JSON_STRING && tb == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (ta == JSON_ARRAY && tb == JSON_ARRAY) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!jeq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (ta == JSON_OBJECT && tb == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !jeq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* Shortest round-trip decimal (Python repr/str of float). */
static void py_float_repr(double v, char *out, size_t cap) {
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(out, cap, "%.*g", prec, v);
        if (strtod(out, NULL) == v) return;
    }
    snprintf(out, cap, "%.17g", v);
}

/* Python str() for scalars. Containers are serialized as JSON (fixtures keep
 * str()-converted fields scalar, matching the live call sites). */
static char *jpystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && fabs(d) < 9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                py_float_repr(d, buf, sizeof(buf));
            return strdup(buf);
        }
        default: return json_serialize(v);
    }
}

/* First n Unicode code points of a UTF-8 string (Python s[:n]). malloc'd. */
static char *utf8_slice(const char *s, size_t n) {
    if (!s) return strdup("");
    size_t bytes = 0, cps = 0;
    while (s[bytes] && cps < n) {
        unsigned char c = (unsigned char)s[bytes];
        bytes++;
        if ((c & 0xC0) != 0x80) cps++;   /* count lead bytes only */
    }
    char *out = malloc(bytes + 1);
    if (!out) return NULL;
    memcpy(out, s, bytes);
    out[bytes] = '\0';
    return out;
}

static bool str_startswith(const char *s, const char *p) {
    return s && p && strncmp(s, p, strlen(p)) == 0;
}

static bool str_endswith(const char *s, const char *p) {
    if (!s || !p) return false;
    size_t ls = strlen(s), lp = strlen(p);
    return ls >= lp && strcmp(s + ls - lp, p) == 0;
}

static bool str_contains(const char *s, const char *p) {
    return s && p && strstr(s, p) != NULL;
}

/* ASCII lowercase (live call sites compare ASCII error text). malloc'd. */
static char *str_lower(const char *s) {
    char *out = strdup(s ? s : "");
    if (!out) return NULL;
    for (char *p = out; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p += 32;
    return out;
}

/* Python str.strip() for ASCII whitespace. malloc'd. */
static char *str_strip(const char *s) {
    if (!s) return strdup("");
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\v' || *s == '\f') s++;
    size_t n = strlen(s);
    while (n > 0) {
        char c = s[n - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') n--;
        else break;
    }
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* agent/turn_failure_copy.py SITE_FAILURE_CODES (frozenset membership). */
static bool site_failure_code(const char *code) {
    static const char *const codes[] = {
        "context_overflow", "truncated", "invalid_response", "empty_response", "loop_error",
        "interpreter_shutdown", "session_busy", "gateway_connection", "host_error",
    };
    if (!code) return false;
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
        if (strcmp(code, codes[i]) == 0) return true;
    return false;
}

/* ── trace recorder (harness infrastructure) ── */

void grun_trace_add(grun_trace_t *t, json_t *ev) {
    if (!t) { json_free(ev); return; }
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}

void grun_trace_free(grun_trace_t *t) {
    if (!t) return;
    json_free(t->events);
    t->events = NULL;
}

/* ── 1. delivery routing ── */

/* PoP: grun_flow_normalize_empty_agent_response @ gateway/run.py:_normalize_empty_agent_response */
char *grun_flow_normalize_empty_agent_response(const json_t *agent_result,
                                               const char *response,
                                               int history_len,
                                               const grun_normalize_seams_t *s) {
    const char *resp = response ? response : "";
    /* is_overflow computed FIRST, unconditionally (live line order). */
    bool is_overflow = s->is_context_overflow(s->ctx, agent_result, history_len);
    /* `response and not (is_overflow and looks_like(response))` — the provider
     * probe runs only for a non-empty response AND a positive overflow verdict. */
    if (*resp != '\0' && !(is_overflow && s->looks_like_provider_error(s->ctx, resp)))
        return strdup(resp);

    if (jtruth(json_obj_get(agent_result, "failed"))) {
        /* error can be an EXPLICIT None -> `or` falls through to the i18n seam. */
        const json_t *errv = json_obj_get(agent_result, "error");
        char *error_detail = jtruth(errv) ? jpystr(errv)
                                          : s->t(s->ctx, "gateway.shared.unknown_error", NULL);
        char *error_str = str_lower(error_detail ? error_detail : "");
        const json_t *frv = json_obj_get(agent_result, "failure_reason");
        char *failure_reason = jtruth(frv) ? jpystr(frv) : strdup("");
        char *ret = NULL;
        if (str_startswith(failure_reason, "session_persistence_failed")
                || str_contains(error_str, "session storage")) {
            if (str_endswith(failure_reason, ":disk") || str_contains(error_str, "disk"))
                ret = s->t(s->ctx, "gateway.errors.session_storage_unavailable_disk", NULL);
            else
                ret = s->t(s->ctx, "gateway.errors.session_storage_unavailable", NULL);
        } else if (is_overflow) {
            /* _context_overflow_reply() == t("gateway.errors.context_overflow") */
            ret = s->t(s->ctx, "gateway.errors.context_overflow", NULL);
        } else {
            char *detail500 = utf8_slice(error_detail, 500);
            size_t cap = strlen(detail500 ? detail500 : "") + 96;
            char *msg = malloc(cap);
            if (msg) {
                snprintf(msg, cap, "Agent turn failed; reply sanitized for chat. Detail: %s",
                         detail500 ? detail500 : "");
                s->log_warning(s->ctx, msg);
                free(msg);
            }
            free(detail500);
            ret = s->t(s->ctx, "gateway.errors.generic_failed", NULL);
        }
        free(error_detail);
        free(error_str);
        free(failure_reason);
        return ret;
    }

    /* int(agent_result.get("api_calls", 0) or 0) */
    const json_t *acv = json_obj_get(agent_result, "api_calls");
    long long api_calls = 0;
    if (jtruth(acv)) {
        if (acv->type == JSON_NUMBER) api_calls = (long long)acv->num_val;
        else if (acv->type == JSON_STRING) api_calls = atoll(acv->str_val);
        else if (acv->type == JSON_BOOL) api_calls = acv->bool_val ? 1 : 0;
    }

    if (jtruth(json_obj_get(agent_result, "interrupted"))) {
        if (api_calls == 0)
            return s->t(s->ctx, "gateway.errors.interrupted_before_start", NULL);
        return strdup(resp);
    }

    if (api_calls > 0) {
        if (s->hidden_reasoning_incomplete(s->ctx, agent_result))
            return strdup("");
        if (jtruth(json_obj_get(agent_result, "partial"))) {
            const json_t *errv = json_obj_get(agent_result, "error");
            char *err = jtruth(errv) ? jpystr(errv)
                                     : s->t(s->ctx, "gateway.errors.processing_incomplete", NULL);
            const json_t *frv = json_obj_get(agent_result, "failure_reason");
            char *fr = jtruth(frv) ? jpystr(frv) : strdup("");
            char *err_strip = str_strip(err);
            /* site code AND non-empty stripped err AND not a provider envelope:
             * the provider probe is short-circuited exactly as live. */
            if (site_failure_code(fr) && err_strip && *err_strip != '\0'
                    && !s->looks_like_provider_error(s->ctx, err ? err : "")) {
                char *ret;
                if (str_startswith(err, "\xe2\x9a\xa0\xef\xb8\x8f")) {
                    ret = strdup(err ? err : "");
                } else {
                    size_t cap = strlen(err ? err : "") + 8;
                    ret = malloc(cap);
                    if (ret) snprintf(ret, cap, "\xe2\x9a\xa0\xef\xb8\x8f %s", err ? err : "");
                }
                free(err); free(fr); free(err_strip);
                return ret;
            }
            free(err_strip);
            char *reason = NULL;
            if (s->looks_like_provider_error(s->ctx, err ? err : "")) {
                char *e500 = utf8_slice(err, 500);
                size_t cap = strlen(e500 ? e500 : "") + 96;
                char *msg = malloc(cap);
                if (msg) {
                    snprintf(msg, cap, "Agent turn ended partially; reply sanitized for chat. Detail: %s",
                             e500 ? e500 : "");
                    s->log_warning(s->ctx, msg);
                    free(msg);
                }
                free(e500);
                reason = strdup("");
            } else {
                char *e200 = utf8_slice(err, 200);
                size_t cap = strlen(e200 ? e200 : "") + 4;
                reason = malloc(cap);
                if (reason) snprintf(reason, cap, ": %s", e200 ? e200 : "");
                free(e200);
            }
            json_t *kw = json_object();
            json_set(kw, "reason", json_string(reason ? reason : ""));
            char *ret = s->t(s->ctx, "gateway.errors.stopped_before_finishing", kw);
            json_free(kw);
            free(err); free(fr); free(reason);
            return ret;
        }
        return s->t(s->ctx, "gateway.errors.no_response", NULL);
    }

    if (api_calls == 0 && !jtruth(json_obj_get(agent_result, "partial")))
        return s->t(s->ctx, "gateway.errors.previous_turn_cleanup", NULL);
    return strdup(resp);
}

/* ── 2. replay intake ── */

/* run.py _ASSISTANT_REPLAY_FIELDS (ordered tuple; order is the copy order). */
static const char *const ASSISTANT_REPLAY_FIELDS[] = {
    "reasoning", "reasoning_content", "reasoning_details",
    "codex_reasoning_items", "codex_message_items", "finish_reason",
};

/* PoP: grun_flow_build_replay_entry @ gateway/run.py:_build_replay_entry */
json_t *grun_flow_build_replay_entry(const char *role, const json_t *content,
                                     const json_t *msg, bool preserve_timestamp,
                                     const grun_replay_seams_t *s) {
    json_t *entry = json_object();
    json_set(entry, "role", json_string(role ? role : ""));
    json_set(entry, "content", content ? json_copy(content) : json_null());

    /* api_content sidecar: only when this pipeline did not rewrite content. */
    const json_t *sidecar = json_obj_get(msg, "api_content");
    if ((strcmp(role ? role : "", "user") == 0 || strcmp(role ? role : "", "assistant") == 0)
            && sidecar && sidecar->type == JSON_STRING && jtruth(sidecar)
            && jeq(content, json_obj_get(msg, "content"))) {
        json_set(entry, "api_content", json_string(sidecar->str_val));
    }

    if (strcmp(role ? role : "", "assistant") == 0) {
        for (size_t i = 0; i < sizeof(ASSISTANT_REPLAY_FIELDS) / sizeof(ASSISTANT_REPLAY_FIELDS[0]); i++) {
            const char *rkey = ASSISTANT_REPLAY_FIELDS[i];
            const json_t *rval = json_obj_get(msg, rkey);
            if (!rval) continue;                     /* `if _rkey not in msg` */
            /* reasoning_content drops ONLY None ("" is a provider sentinel);
             * every other field drops all falsy values. */
            bool drop = (strcmp(rkey, "reasoning_content") == 0)
                            ? (rval->type == JSON_NULL)
                            : !jtruth(rval);
            if (drop) continue;
            json_set(entry, rkey, json_copy(rval));
        }
    }

    if (preserve_timestamp && jtruth(json_obj_get(msg, "timestamp")))
        json_set(entry, "timestamp", json_copy(json_obj_get(msg, "timestamp")));

    /* entry.update(replay_metadata_fields(msg)) — seam result merged in order;
     * json_set keeps existing key positions, appends new keys (dict.update). */
    json_t *meta = s->replay_metadata_fields(s->ctx, msg);
    if (meta && meta->type == JSON_OBJECT) {
        for (size_t i = 0; i < meta->c.count; i++)
            json_set(entry, meta->c.keys[i], json_copy(meta->c.items[i]));
    }

    /* copy_identity_fields({key: msg[key] for key in (MESSAGE_UID,
     * ABSORBED_MESSAGE_UIDS) if key in msg}, entry) — subset built in tuple
     * order, then the seam applies the scripted identity update onto entry. */
    json_t *idsrc = json_object();
    const json_t *uid = json_obj_get(msg, "message_uid");
    if (uid) json_set(idsrc, "message_uid", json_copy(uid));
    const json_t *absorbed = json_obj_get(msg, "_absorbed_message_uids");
    if (absorbed) json_set(idsrc, "_absorbed_message_uids", json_copy(absorbed));
    s->copy_identity_fields(s->ctx, idsrc, entry);
    json_free(idsrc);

    if (jtruth(json_obj_get(msg, "_db_persisted")))
        json_set(entry, "_db_persisted", json_bool(true));

    /* durable-row stamps, in live tuple order */
    static const char *const stamps[] = {
        "_merged_durable_rows", "_unnamed_durable_rows", "_retired_durable_rows",
    };
    for (size_t i = 0; i < sizeof(stamps) / sizeof(stamps[0]); i++) {
        const json_t *v = json_obj_get(msg, stamps[i]);
        if (jtruth(v)) json_set(entry, stamps[i], json_copy(v));
    }
    return entry;
}

/* PoP: grun_flow_has_replayable_sidecar @ gateway/run.py:_has_replayable_sidecar */
bool grun_flow_has_replayable_sidecar(const json_t *role, const json_t *content,
                                      const json_t *msg) {
    if (!role || role->type != JSON_STRING || strcmp(role->str_val, "assistant") != 0)
        return false;
    if (jtruth(content)) return false;
    const json_t *ac = json_obj_get(msg, "api_content");
    return ac && ac->type == JSON_STRING && jtruth(ac);
}

/* ── 3. intake normalization ── */

/* PoP: grun_flow_command_origin_for_source @ gateway/run.py:_command_origin_for_source */
json_t *grun_flow_command_origin_for_source(const json_t *source, bool platform_raises) {
    if (platform_raises) return NULL;   /* injected exception -> except -> None */
    const json_t *plat = json_obj_get(source, "platform");
    /* getattr(source.platform, "value", None): only object platforms carry it */
    const json_t *pv = (plat && plat->type == JSON_OBJECT) ? json_obj_get(plat, "value") : NULL;
    char *platform;
    if (jtruth(pv)) {
        platform = jpystr(pv);
    } else {
        /* str(getattr(source, "platform", "") or "") */
        platform = jtruth(plat) ? jpystr(plat) : strdup("");
    }
    const json_t *cid = json_obj_get(source, "chat_id");
    json_t *out = NULL;
    if (platform && *platform != '\0' && jtruth(cid)) {
        out = json_object();
        json_set(out, "platform", json_string(platform));
        char *cid_str = jpystr(cid);
        json_set(out, "chat_id", json_string(cid_str ? cid_str : ""));
        free(cid_str);
        const json_t *cn = json_obj_get(source, "chat_name");
        json_set(out, "chat_name", cn ? json_copy(cn) : json_null());
        const json_t *ti = json_obj_get(source, "thread_id");
        json_set(out, "thread_id", ti ? json_copy(ti) : json_null());
    }
    free(platform);
    return out;
}

/* ── 4. agent-cache lifecycle ── */

/* PoP: grun_flow_cached_agent_for_hygiene @ gateway/run.py:_cached_agent_for_hygiene */
json_t *grun_flow_cached_agent_for_hygiene(const json_t *gateway, const char *session_key,
                                           const grun_cache_seams_t *s) {
    const json_t *cache = json_obj_get(gateway, "_agent_cache");
    if (!cache || cache->type == JSON_NULL) return NULL;
    /* `with (lock or suppress())` — a falsy lock is the no-op context manager */
    bool have_lock = jtruth(json_obj_get(gateway, "_agent_cache_lock"));
    if (have_lock) s->lock_acquire(s->ctx);
    bool is_tuple = false, raised = false;
    json_t *entry = s->cache_get(s->ctx, session_key ? session_key : "", &is_tuple, &raised);
    if (have_lock) s->lock_release(s->ctx);   /* __exit__ runs before except */
    if (raised) entry = NULL;
    /* entry[0] if isinstance(entry, tuple) and entry else entry */
    if (is_tuple && entry && entry->type == JSON_ARRAY && entry->c.count > 0)
        return json_copy(entry->c.items[0]);
    return entry ? json_copy(entry) : NULL;
}

/* ── 5. executor lifecycle ── */

/* PoP: grun_flow_get_or_create_pool @ gateway/run.py:GatewayRunner._get_or_create_pool */
json_t *grun_flow_get_or_create_pool(json_t *self_state, const char *attr,
                                     const grun_pool_seams_t *s,
                                     bool *raised_runtime_error) {
    if (raised_runtime_error) *raised_runtime_error = false;
    /* lock = getattr(self, "_executor_lock", None); if None: create + publish */
    const json_t *lock = json_obj_get(self_state, "_executor_lock");
    if (!lock || lock->type == JSON_NULL)
        json_set(self_state, "_executor_lock", json_bool(true));
    /* with lock: closing flag fences creation */
    if (jtruth(json_obj_get(self_state, "_executor_closing"))) {
        if (raised_runtime_error) *raised_runtime_error = true;
        return NULL;
    }
    json_t *executor = json_obj_get(self_state, attr);
    if (!executor || executor->type == JSON_NULL
            || jtruth(json_obj_get(executor, "_shutdown"))) {
        json_t *made = s->make_pool(s->ctx);
        executor = made ? json_copy(made) : json_null();
        json_set(self_state, attr, executor);
    }
    return executor;
}

/* ── 6. shutdown sequence ── */

/* PoP: grun_flow_stop_pool @ gateway/run.py:GatewayRunner._stop_pool */
json_t *grun_flow_stop_pool(const json_t *executor, const grun_stop_pool_seams_t *s) {
    if (!executor || executor->type == JSON_NULL) return json_array();
    /* try shutdown(wait=False, cancel_futures=True) except TypeError:
     * shutdown(wait=False) */
    bool typeerror = s->shutdown(s->ctx, false, true);
    if (typeerror) s->shutdown(s->ctx, false, false);
    /* list(getattr(executor, "_threads", None) or ()) */
    const json_t *th = json_obj_get(executor, "_threads");
    if (jtruth(th) && th->type == JSON_ARRAY) return json_copy(th);
    return json_array();
}

/* ── 7. session-key routing ── */

/* PoP: grun_flow_resolve_session_key_or_none @ gateway/run.py:GatewayRunner._resolve_session_key_or_none */
char *grun_flow_resolve_session_key_or_none(const json_t *source, const char *session_key,
                                            const grun_session_key_seams_t *s) {
    /* `if session_key or source is None: return session_key` */
    if ((session_key && *session_key != '\0') || !source || source->type == JSON_NULL)
        return session_key ? strdup(session_key) : NULL;
    bool raised = false;
    char *key = s->session_key_for_source(s->ctx, source, &raised);
    if (raised) { free(key); return NULL; }
    return key;
}

/* ── 8. cron tick gating ── */

/* PoP: grun_flow_cron_profile_gate @ gateway/run.py:_cron_profile_gate */
bool grun_flow_cron_profile_gate(const char *name, const char *home,
                                 const grun_cron_gate_seams_t *s) {
    bool raised = false;
    /* LIVE passes Path(home); Path("") normalizes to ".", so forward "." rather
     * than a raw empty string (which would change the seam's trace argument). */
    const char *liveness_home = (home && *home != '\0') ? home : ".";
    json_t *liveness = s->resolve_liveness(s->ctx, liveness_home, &raised);
    if (raised) {
        /* logger.debug("Cron profile gate probe failed for %s (ticking it): %s", name, exc) */
        const char *exc = s->probe_error ? s->probe_error : "";
        size_t cap = strlen(name ? name : "") + strlen(exc) + 96;
        char *msg = malloc(cap);
        if (msg) {
            snprintf(msg, cap, "Cron profile gate probe failed for %s (ticking it): %s",
                     name ? name : "", exc);
            s->log_debug(s->ctx, msg);
            free(msg);
        }
        return true;
    }
    bool running = jtruth(json_obj_get(liveness, "running"));
    const json_t *pid = json_obj_get(liveness, "pid");
    bool pid_present = pid && pid->type != JSON_NULL;
    bool same_pid = pid_present && pid->type == JSON_NUMBER
                        && (long long)pid->num_val == s->self_pid;
    /* not (liveness.running and liveness.pid is not None and liveness.pid != os.getpid()) */
    return !(running && pid_present && !same_pid);
}

/* ── 9. cron tick scope ── */

/* PoP: grun_flow_cron_tick_profile_homes @ gateway/run.py:_cron_tick_profile_homes */
json_t *grun_flow_cron_tick_profile_homes(const json_t *config, const grun_cron_tick_seams_t *s) {
    json_t *homes = json_array();
    json_t *mh = s->multiplex_homes(s->ctx, config);
    if (mh && mh->type == JSON_ARRAY)
        for (size_t i = 0; i < mh->c.count; i++)
            json_append(homes, json_copy(mh->c.items[i]));
    /* active = get_active_profile_name() or "default" */
    char *active_raw = s->active_profile(s->ctx);
    const char *active = (active_raw && *active_raw != '\0') ? active_raw : "default";
    bool found = false;
    for (size_t i = 0; i < homes->c.count; i++) {
        json_t *pair = homes->c.items[i];
        if (pair && pair->type == JSON_ARRAY && pair->c.count > 0) {
            json_t *nm = pair->c.items[0];
            if (nm && nm->type == JSON_STRING && strcmp(nm->str_val, active) == 0) {
                found = true;
                break;
            }
        }
    }
    if (!found) {
        bool raised = false;
        char *dir = s->profile_dir(s->ctx, active, &raised);
        if (!raised) {
            json_t *pair = json_array();
            json_append(pair, json_string(active));
            json_append(pair, dir ? json_string(dir) : json_null());
            json_append(homes, pair);
        }
        free(dir);
    }
    free(active_raw);
    return homes;
}

/* ── 10. handoff watcher scopes ── */

/* PoP: grun_flow_handoff_watch_scopes @ gateway/run.py:_handoff_watch_scopes */
json_t *grun_flow_handoff_watch_scopes(const json_t *runner, const grun_handoff_seams_t *s) {
    json_t *scopes = json_array();
    json_t *root = json_array();
    json_append(root, json_null());
    json_append(root, json_null());
    json_append(scopes, root);                 /* (None, None) always first */
    const json_t *config = json_obj_get(runner, "config");
    if (config && config->type != JSON_NULL
            && jtruth(json_obj_get(config, "multiplex_profiles"))) {
        bool raised = false;
        json_t *homes = s->multiplex_homes(s->ctx, config, &raised);
        if (raised) {
            s->log_debug(s->ctx, "Could not resolve multiplex homes for handoff watcher");
            return scopes;
        }
        if (homes && homes->type == JSON_ARRAY) {
            for (size_t i = 0; i < homes->c.count; i++) {
                json_t *pair = homes->c.items[i];
                if (!pair || pair->type != JSON_ARRAY || pair->c.count < 2) continue;
                json_t *nm = pair->c.items[0];
                json_t *home = pair->c.items[1];
                /* home is None or not name or name == "default" */
                if (!home || home->type == JSON_NULL) continue;
                if (!jtruth(nm)) continue;
                if (nm->type == JSON_STRING && strcmp(nm->str_val, "default") == 0) continue;
                json_append(scopes, json_copy(pair));
            }
        }
    }
    return scopes;
}

/* ── 11. host-gateway claim ── */

/* PoP: grun_flow_claim_host_gateway_role @ gateway/run.py:_claim_host_gateway_role */
int grun_flow_claim_host_gateway_role(bool force, const grun_claim_seams_t *s) {
    /* Pre-lock opt-out check (skipped entirely under --force). */
    if (!force) {
        char *home = s->hermes_home(s->ctx);
        json_t *decision = s->attach_decision(s->ctx, home ? home : "", NULL);
        bool refuse = decision && decision->type != JSON_NULL
                          && strcmp(json_get_str(decision, "outcome", ""), "refuse") == 0;
        if (refuse) {
            s->print_line(s->ctx, json_get_str(decision, "message", ""));
            free(home);
            return 75;   /* SystemExit(GATEWAY_SERVICE_RESTART_EXIT_CODE) */
        }
        free(home);
    }

    char *outcome = NULL, *error = NULL;
    bool raised = s->claim_host_lock(s->ctx, "gateway", &outcome, &error);
    if (raised) {
        s->log(s->ctx, "debug", "host gateway rendezvous failed");
        free(outcome); free(error);
        return 0;
    }
    if (outcome && strcmp(outcome, "acquired") == 0) {
        /* PROVISIONAL publish: empty served set, refreshed later. */
        char *home = s->hermes_home(s->ctx);
        json_t *profiles = json_array();
        s->publish_record(s->ctx, "gateway", profiles, home ? home : "");
        json_free(profiles);
        s->cleanup_on_exit(s->ctx, "gateway");
        free(home); free(outcome); free(error);
        return 0;
    }
    if (outcome && strcmp(outcome, "could-not-open") == 0) {
        /* NOT contention: unusable lock dir never implies a second gateway. */
        const char *err = error ? error : "";
        size_t cap = strlen(err) + 224;
        char *msg = malloc(cap);
        if (msg) {
            snprintf(msg, cap,
                     "Host gateway lock could not be opened (%s); this gateway is not discoverable. "
                     "No second gateway is implied — the lock directory itself is unusable.", err);
            s->log(s->ctx, "warning", msg);
            free(msg);
        }
        free(outcome); free(error);
        return 0;
    }
    free(outcome); free(error);

    /* Lost the lock: read the owner's record; failure changes WHO, not the verdict. */
    json_t *owner = NULL;
    bool read_raised = false;
    owner = s->read_record(s->ctx, "gateway", true, &read_raised);
    if (read_raised) {
        owner = NULL;
        s->log(s->ctx, "debug", "host gateway record unreadable");
    }

    if (force) {
        char *who = jtruth(owner) ? s->describe(s->ctx, owner) : strdup("another process");
        size_t cap = strlen(who ? who : "") + 96;
        char *msg = malloc(cap);
        if (msg) {
            snprintf(msg, cap, "--force: starting a second gateway although %s owns this host.",
                     who ? who : "");
            s->log(s->ctx, "warning", msg);
            free(msg);
        }
        free(who);
        return 0;
    }

    /* Another tenant's home can never win the per-OS-user lock: start beside it. */
    if (owner) {
        const char *owner_home = json_get_str(owner, "home", "");
        char *home = s->hermes_home(s->ctx);
        bool tenant = s->launched_by_other_tenant(s->ctx, owner_home, home ? home : "");
        if (tenant) {
            char *desc = s->describe(s->ctx, owner);
            size_t cap = strlen(desc ? desc : "") + 128;
            char *msg = malloc(cap);
            if (msg) {
                snprintf(msg, cap,
                         "Another Hermes home's gateway owns this host (%s); starting this home's gateway beside it.",
                         desc ? desc : "");
                s->log(s->ctx, "warning", msg);
                free(msg);
            }
            free(desc); free(home);
            return 0;
        }
        free(home);
    }

    /* Standalone recheck after losing the atomic lock (served set may be stale). */
    {
        char *home = s->hermes_home(s->ctx);
        bool standalone = s->profile_is_standalone(s->ctx, home ? home : "");
        if (standalone) {
            json_t *live_owner = s->host_gateway(s->ctx, 5.0);   /* ATTACH_CHANNEL_WAIT_S */
            /* live RE-CALLS get_hermes_home() for the recheck decision
             * (run.py: standalone_attach_decision(get_hermes_home(), live_owner));
             * it does not reuse the home read for profile_is_standalone. */
            char *home2 = s->hermes_home(s->ctx);
            json_t *decision = s->attach_decision(s->ctx, home2 ? home2 : "", live_owner);
            free(home2);
            if (decision && decision->type != JSON_NULL) {
                if (strcmp(json_get_str(decision, "outcome", ""), "start") == 0) {
                    free(home);
                    return 0;
                }
                s->print_line(s->ctx, json_get_str(decision, "message", ""));
                free(home);
                return 75;
            }
        }
        free(home);
    }

    /* Composition with the attach START verdict: an unmigrated standalone owner
     * can never be beaten in the lock race — start beside it. */
    if (s->owner_is_standalone(s->ctx)) {
        char *who = jtruth(owner) ? s->describe(s->ctx, owner) : strdup("owner unknown");
        char *mig = s->migrate_command(s->ctx);
        size_t cap = strlen(who ? who : "") + strlen(mig ? mig : "") + 256;
        char *msg = malloc(cap);
        if (msg) {
            snprintf(msg, cap,
                     "Another profile's standalone gateway owns this host (%s); starting beside it rather "
                     "than retrying a race no second profile can win. Fold every profile onto one gateway "
                     "with: %s", who ? who : "", mig ? mig : "");
            s->log(s->ctx, "warning", msg);
            free(msg);
        }
        free(who); free(mig);
        return 0;
    }

    s->refuse_second(s->ctx, owner);
    return 75;
}

/* ── 12. standalone-owner probe ── */

/* PoP: grun_flow_owner_is_standalone @ gateway/run.py:_owner_is_standalone */
bool grun_flow_owner_is_standalone(const grun_owner_seams_t *s) {
    bool raised = false;
    json_t *owner = s->host_gateway(s->ctx, &raised);
    if (raised) goto except_path;
    if (!owner || owner->type == JSON_NULL) return false;
    const json_t *pid = json_obj_get(owner, "pid");
    if (!pid) goto except_path;   /* owner.pid AttributeError -> except */
    if (pid->type == JSON_NUMBER && (long long)pid->num_val == s->self_pid) return false;

    char *home = s->hermes_home(s->ctx, &raised);
    if (raised) { free(home); goto except_path; }
    char *name = s->profile_name_for_home(s->ctx, home ? home : "", &raised);
    free(home);
    if (raised) { free(name); goto except_path; }
    json_t *answered = s->request_serve_profile(s->ctx, name ? name : "", owner, &raised);
    free(name);
    if (raised) goto except_path;
    if (!answered || answered->type == JSON_NULL) return false;
    const json_t *standalone = json_obj_get(answered, "standalone");
    if (!standalone) goto except_path;   /* answered.standalone AttributeError */
    return jtruth(standalone);

except_path:
    s->log_debug(s->ctx, "standalone-owner probe failed; keeping the second-gateway refusal");
    return false;
}

/* ── 13. second-gateway refusal ── */

/* PoP: grun_flow_refuse_second_host_gateway @ gateway/run.py:_refuse_second_host_gateway */
int grun_flow_refuse_second_host_gateway(const json_t *owner, const grun_refuse_seams_t *s) {
    char *who = jtruth(owner)
                    ? s->describe(s->ctx, owner)
                    : strdup("owner unknown (its record is gone)");
    char *mig = s->migrate_command(s->ctx);
    size_t cap = strlen(who ? who : "") + strlen(mig ? mig : "") + 640;
    char *message = malloc(cap);
    if (message) {
        snprintf(message, cap,
                 "\xe2\x9d\x8c Another gateway already owns this host: %s\n"
                 "   Exactly one gateway per host serves every profile, so this process will not start a\n"
                 "   second one (it would double-bind this profile's platforms).\n"
                 "   Fold every profile onto the owner:  %s\n"
                 "   Or stop the other gateway first, then start this one.\n"
                 "   Or start one anyway (skips the host-lock check):  hermes gateway run --force\n"
                 "   (--replace does not skip this check; it only replaces an owner that serves this profile.)",
                 who ? who : "", mig ? mig : "");
    }
    size_t lcap = strlen(who ? who : "") + 96;
    char *logmsg = malloc(lcap);
    if (logmsg) {
        snprintf(logmsg, lcap, "Refusing to start a second gateway on this host: %s", who ? who : "");
        s->log_error(s->ctx, logmsg);
        free(logmsg);
    }
    if (message) {
        s->print_line(s->ctx, message);
        free(message);
    }
    free(who); free(mig);
    return 75;   /* SystemExit(GATEWAY_SERVICE_RESTART_EXIT_CODE) */
}

/* ── 14. host record refresh ── */

/* PoP: grun_flow_refresh_host_gateway_record @ gateway/run.py:_refresh_host_gateway_record */
void grun_flow_refresh_host_gateway_record(const json_t *runner, const grun_refresh_seams_t *s) {
    bool raised = false;
    bool owns = s->owns_host_lock(s->ctx, "gateway", &raised);
    if (raised) goto except_path;
    if (!owns) return;

    char *home = s->hermes_home(s->ctx, &raised);
    if (raised) { free(home); goto except_path; }

    /* getattr(runner.config, ...): an ABSENT config attribute raises
     * AttributeError (except path); config=None reads the default False. */
    if (!json_has(runner, "config")) { free(home); goto except_path; }
    const json_t *config = json_obj_get(runner, "config");
    bool multiplex = config && config->type != JSON_NULL
                         && jtruth(json_obj_get(config, "multiplex_profiles"));

    json_t *served = json_array();
    if (multiplex) {
        json_t *names = s->served_profile_names(s->ctx, &raised);
        if (raised) { free(home); json_free(served); goto except_path; }
        if (names && names->type == JSON_ARRAY)
            for (size_t i = 0; i < names->c.count; i++)
                json_append(served, json_copy(names->c.items[i]));
    } else {
        char *name = s->profile_name_for_home(s->ctx, home ? home : "", &raised);
        if (raised) { free(name); free(home); json_free(served); goto except_path; }
        json_append(served, json_string(name ? name : ""));
        free(name);
    }
    s->publish_record(s->ctx, "gateway", served, home ? home : "");
    json_free(served);
    free(home);
    return;

except_path:
    s->log_debug(s->ctx, "host gateway record refresh failed");
}
