/*
 * process_registry_env_scope.c -- real C11 port of upstream Hermes
 *   tools/process_registry_env_scope.py.
 *
 * PoP: w21_processreg_cf2ea3_in_env @ tools/process_registry_env_scope.py:_in_env
 * PoP: w21_processreg_cf2ea3_has_running @ tools/process_registry_env_scope.py:ProcessEnvScopeMixin.has_running_in_env
 * PoP: w21_processreg_cf2ea3_settle @ tools/process_registry_env_scope.py:ProcessEnvScopeMixin.settle_env_closed
 * PoP: w21_processreg_cf2ea3_env_probe @ tools/process_registry_env_scope.py:ProcessEnvScopeMixin.settle_env_closed
 *
 * The registry's _lock/_running/_any_running/_finish_exited are the object
 * seams and arrive as `world`; the identity rules the module itself owns (an
 * env_ref never falls back to task_id, a handoff's task_id does not move the
 * process, only not-exited sessions of THIS env are settled) are here.
 */
#include "hermes_json.h"
#include "noc_w21_processreg_cf2ea3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python truthiness of a decoded JSON value; absent (NULL) reads as None. */
static int prsc_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static int prsc_streq(const char *a, const char *b) {
    if (!a || !b) return 0;
    return strcmp(a, b) == 0;
}

/* session.env_ref as a tag: NULL when env_ref is None. */
static const char *prsc_env_ref(const json_t *session) {
    json_t *v;
    if (!session || session->type != JSON_OBJECT) return NULL;
    v = json_obj_get(session, "env_ref");
    if (!v || v->type != JSON_STRING || !v->str_val || !v->str_val[0]) return NULL;
    return v->str_val;
}

static const char *prsc_task_id(const json_t *session) {
    json_t *v;
    if (!session || session->type != JSON_OBJECT) return "";
    v = json_obj_get(session, "task_id");
    if (!v || v->type != JSON_STRING || !v->str_val) return "";
    return v->str_val;
}

/* _in_env(session, key, env): an env_ref session belongs only through identity;
 * sessions without one (local, host-recovered) fall back to their task_id. */
static int prsc_in_env(const json_t *session, const char *key, const char *env_tag) {
    const char *ref = prsc_env_ref(session);
    if (ref != NULL) return env_tag != NULL && strcmp(ref, env_tag) == 0;
    return prsc_streq(prsc_task_id(session), key ? key : "");
}

int w21_processreg_cf2ea3_in_env(const json_t *session, const char *key,
                                 const char *env_tag) {
    return prsc_in_env(session, key, env_tag);
}

static json_t *prsc_sessions(const json_t *world) {
    json_t *s = world ? json_obj_get(world, "sessions") : NULL;
    return (s && s->type == JSON_ARRAY) ? s : NULL;
}

json_t *w21_processreg_cf2ea3_has_running(const json_t *world) {
    json_t *sessions;
    size_t i;
    const char *key = world ? json_get_str(world, "key", "") : "";
    const char *tag = world ? json_get_str(world, "env_tag", NULL) : NULL;

    sessions = prsc_sessions(world);
    if (!sessions) return json_bool(0);
    for (i = 0; i < sessions->c.count; i++) {
        if (prsc_in_env(sessions->c.items[i], key, tag)) return json_bool(1);
    }
    return json_bool(0);
}

json_t *w21_processreg_cf2ea3_settle(const json_t *world) {
    json_t *sessions;
    size_t i;
    long settled = 0;
    const char *tag;
    int cleanup_stops = 1;

    if (!world || world->type != JSON_OBJECT) return json_number(0);
    tag = json_get_str(world, "env_tag", NULL);
    /* if env is None or not getattr(env, "cleanup_stops_processes", True): return 0
     * (None would match every local session, so it is never settled.) */
    if (!tag || !tag[0]) return json_number(0);
    {
        json_t *cf = json_obj_get(world, "cleanup_stops_processes");
        if (cf) cleanup_stops = prsc_truthy(cf);   /* getattr(..., True), None is falsy */
    }
    if (!cleanup_stops) return json_number(0);

    /* sessions = [s for s in self._running.values()
     *             if s.env_ref is env and not s.exited] */
    sessions = prsc_sessions(world);
    if (!sessions) return json_number(0);
    for (i = 0; i < sessions->c.count; i++) {
        json_t *s = sessions->c.items[i];
        const char *ref = prsc_env_ref(s);
        if (!ref || strcmp(ref, tag) != 0) continue;
        if (prsc_truthy(json_obj_get(s, "exited"))) continue;
        settled++;                       /* _finish_exited(s, -1, reason="lost", ...) */
    }
    return json_number((double)settled);
}

json_t *w21_processreg_cf2ea3_env_probe(void) {
    const char *sler = getenv("SLERMES_HOME");
    const char *home = getenv("HOME");
    json_t *out = json_object();
    json_set(out, "sler", json_bool(sler && sler[0]));
    json_set(out, "home", json_bool(home && home[0]));
    json_set(out, "same", json_bool(sler && home && !strcmp(sler, home)));
    return out;
}
