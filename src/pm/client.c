/*
 * client.c — C port of upstream Hermes pm/client.py (gate w5_client_039ee5).
 *
 * PoP: w5_client_039ee5_prepare_tools @ pm/client.py:prepare_tools
 * PoP: w5_client_039ee5_stage_tools @ pm/client.py:stage_tools
 * PoP: w5_client_039ee5_ensure_environment @ pm/client.py:ensure_environment
 * PoP: w5_client_039ee5_ensure_python_tool @ pm/client.py:ensure_python_tool
 * PoP: w5_client_039ee5_build_requirements_environment
 *   @ pm/client.py:build_requirements_environment
 *
 * _python_operation dispatches to a worker process; the gate injects it, so
 * what is mirrored here is the guard each public function applies and the
 * exact payload it builds. str(Path(...)) is represented by path_str() for
 * the simple relative names the fixtures use.
 */
#include "noc_w5_client_039ee5.h"

#include <string.h>

static int is_string(json_t *v) { return v && v->type == JSON_STRING; }

static json_t *val(json_t *v) { return v ? json_copy(v) : json_null(); }

static json_t *path_str(const char *s) {
    return json_string(!s || !*s ? "." : s);
}

static json_t *result(const char *op, json_t *payload) {
    json_t *o = json_object();
    json_set(o, "op", json_string(op));
    json_set(o, "payload", payload);
    return o;
}

/* PoP: w5_client_039ee5_prepare_tools @ pm/client.py:prepare_tools */
json_t *w5_client_039ee5_prepare_tools(json_t *names, const char *out,
                                       const char *target, json_t *cache) {
    if (is_string(names)) return json_null();   /* TypeError: names must be a sequence */
    json_t *p = json_object();
    json_set(p, "names", val(names));
    json_set(p, "out", path_str(out));
    json_set(p, "target", json_string(target ? target : ""));
    json_set(p, "cache", val(cache));
    return result("prepare_tools", p);
}

/* PoP: w5_client_039ee5_stage_tools @ pm/client.py:stage_tools */
json_t *w5_client_039ee5_stage_tools(json_t *names, const char *source_store,
                                     const char *out, const char *target) {
    if (is_string(names)) return json_null();
    json_t *p = json_object();
    json_set(p, "names", val(names));
    json_set(p, "source_store", path_str(source_store));
    json_set(p, "out", path_str(out));
    json_set(p, "target", json_string(target ? target : ""));
    return result("stage_tools", p);
}

/* PoP: w5_client_039ee5_ensure_environment @ pm/client.py:ensure_environment */
json_t *w5_client_039ee5_ensure_environment(const char *name, json_t *requirements,
                                            json_t *root, int explicit,
                                            double timeout) {
    if (is_string(requirements)) return json_null();
    json_t *p = json_object();
    json_set(p, "name", json_string(name ? name : ""));
    json_set(p, "requirements", val(requirements));
    json_set(p, "root", val(root));
    json_set(p, "explicit", json_bool(explicit != 0));
    json_set(p, "timeout", json_number(timeout));
    return result("ensure_environment", p);
}

/* PoP: w5_client_039ee5_ensure_python_tool @ pm/client.py:ensure_python_tool */
json_t *w5_client_039ee5_ensure_python_tool(const char *name, json_t *requirements,
                                            const char *executable, json_t *root,
                                            int explicit, double timeout) {
    if (is_string(requirements)) return json_null();
    json_t *p = json_object();
    json_set(p, "name", json_string(name ? name : ""));
    json_set(p, "requirements", val(requirements));
    json_set(p, "executable", json_string(executable ? executable : ""));
    json_set(p, "root", val(root));
    json_set(p, "explicit", json_bool(explicit != 0));
    json_set(p, "timeout", json_number(timeout));
    return result("ensure_python_tool", p);
}

/* PoP: w5_client_039ee5_build_requirements_environment
 *   @ pm/client.py:build_requirements_environment */
json_t *w5_client_039ee5_build_requirements_environment(
    json_t *requirements, const char *out, json_t *python, json_t *cache,
    json_t *env, json_t *wheelhouse, int offline, int sealed, int explicit,
    json_t *lock)
{
    if (is_string(requirements)) return json_null();
    json_t *p = json_object();
    json_set(p, "requirements", val(requirements));
    json_set(p, "out", path_str(out));
    json_set(p, "python", val(python));
    json_set(p, "cache", val(cache));
    json_set(p, "env", val(env));
    json_set(p, "wheelhouse", val(wheelhouse));
    json_set(p, "offline", json_bool(offline != 0));
    json_set(p, "sealed", json_bool(sealed != 0));
    json_set(p, "explicit", json_bool(explicit != 0));
    json_set(p, "lock", val(lock));
    return result("build_requirements_environment", p);
}
