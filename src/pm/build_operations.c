/*
 * build_operations.c — faithful C11 port of pm/build_operations.py
 * (gate w7_buildoperat_e2e0da), caller-owned validation subset.
 *
 * PoP: w7_buildoperat_prepare_tools @ pm/build_operations.py:prepare_tools
 * PoP: w7_buildoperat_verified_tools @ pm/build_operations.py:verified_tools
 * PoP: w7_buildoperat_stage_tools @ pm/build_operations.py:stage_tools
 * PoP: w7_buildoperat_check_project_lock @ pm/build_operations.py:check_project_lock
 * PoP: w7_buildoperat_build_requirements_environment @ pm/build_operations.py:build_requirements_environment
 *
 * The injected booleans stand in for the results of pm.registry.walk and the
 * filesystem checks; the ordering of guards mirrors the Python source exactly.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

static json_t *w7_ok(void) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    return o;
}
static json_t *w7_err(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "error", json_string(cls));
    json_set(o, "message", json_string(msg));
    return o;
}

static int w7_is_string(const json_t *v) {
    return v && v->type == JSON_STRING;
}

json_t *w7_buildoperat_prepare_tools(const json_t *names, int state_present) {
    if (w7_is_string(names))
        return w7_err("TypeError", "names must be a sequence, not a string");
    if (state_present)
        return w7_err("ValueError", "prepare_tools accepts tools, not application state");
    return w7_ok();
}

json_t *w7_buildoperat_verified_tools(const json_t *names, int state_present, int facts_present) {
    if (w7_is_string(names))
        return w7_err("TypeError", "names must be a sequence, not a string");
    if (state_present)
        return w7_err("ValueError", "verified_tools accepts tool packages, not application state");
    if (!facts_present)
        return w7_err("InstallError", "tools source facts missing");
    return w7_ok();
}

json_t *w7_buildoperat_stage_tools(const json_t *names, int overlap, int state_present, int facts_present) {
    if (w7_is_string(names))
        return w7_err("TypeError", "names must be a sequence, not a string");
    if (overlap)
        return w7_err("ValueError", "tool stores must be disjoint");
    if (!facts_present)
        return w7_err("InstallError", "tools source facts missing");
    if (state_present)
        return w7_err("ValueError", "stage_tools accepts tool packages, not application state");
    return w7_ok();
}

json_t *w7_buildoperat_check_project_lock(int manifest_present, int offline, int explicit) {
    if (!offline && !explicit)
        return w7_err("RuntimeError", "install not allowed");
    if (!manifest_present)
        return w7_err("InstallError", "venv project manifest is missing");
    return w7_ok();
}

json_t *w7_buildoperat_build_requirements_environment(int explicit) {
    if (!explicit)
        return w7_err("RuntimeError", "install not allowed");
    return w7_ok();
}
