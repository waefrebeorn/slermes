/*
 * windows_store_update.c - C11 port of upstream hermes_cli/windows_store_update.py
 * (gate w25_windowssto_d94391).
 *
 * PoP: w25_windowssto_d94391_main_result @ hermes_cli/windows_store_update.py:perform
 * PoP: w25_windowssto_d94391_main_result @ hermes_cli/windows_store_update.py:_execute
 * PoP: w25_windowssto_d94391_main_result @ hermes_cli/windows_store_update.py:main
 * PoP: w25_windowssto_d94391_constants @ hermes_cli/windows_store_update.py:_wait
 *
 * The Store call itself is a seam: a single-threaded apartment, StoreContext's
 * default object, ``initialize_with_window`` for consent UI, and an async
 * operation pumped until it leaves STARTED.  What is ported is the decision
 * layer above it -- which document is printed and which exit code the desktop
 * reads -- including the ordering that matters: consent is required before any
 * download/install request, an empty update list short-circuits the request,
 * and a completed_state mismatch is a failure even when the call returned.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_windowssto_d94391.h"

static int ws_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static const char *ws_text(const json_t *v, const char *def) {
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return def;
}

/* main()'s failure document: {"available": None, "ok": False, "error": ...}.
 * ``prefix`` is the "Microsoft Store request failed: " wrapper, absent for the
 * apartment-init failure whose error is the bare exception text. */
static json_t *ws_failure(const char *prefix, const char *text) {
    json_t *o = json_object();
    json_set(o, "available", json_null());
    json_set(o, "ok", json_bool(0));
    if (prefix) {
        char *buf;
        size_t need = strlen(prefix) + strlen(text) + 1;
        buf = (char *)malloc(need);
        if (!buf) return o;
        snprintf(buf, need, "%s%s", prefix, text);
        json_set(o, "error", json_string(buf));
        free(buf);
    } else {
        json_set(o, "error", json_string(text));
    }
    return o;
}

static json_t *ws_envelope(json_t *result, int exit_code) {
    json_t *o = json_object();
    json_set(o, "result", result);
    json_set(o, "exit", json_number((double)exit_code));
    return o;
}

json_t *w25_windowssto_d94391_main_result(const json_t *plan) {
    const json_t *packages, *hwnd, *init_error, *get_error, *request_error;
    const char *mode;
    int available, ok, exit_code;
    json_t *result, *names;
    size_t i, n;

    if (!plan || plan->type != JSON_OBJECT) return json_null();
    mode = ws_text(json_obj_get(plan, "mode"), "check");
    packages = json_obj_get(plan, "packages");
    if (packages && packages->type != JSON_NULL && packages->type != JSON_ARRAY) return json_null();
    hwnd = json_obj_get(plan, "hwnd");
    init_error = json_obj_get(plan, "init_error");
    get_error = json_obj_get(plan, "get_error");
    request_error = json_obj_get(plan, "request_error");
    n = packages ? json_len(packages) : 0;
    available = n > 0;

    /* main(): the apartment never came up -> one failure document, exit 1. */
    if (ws_truthy(init_error)) return ws_envelope(ws_failure(NULL, ws_text(init_error, "")), 1);

    /* _execute(): consent needs a desktop window before anything else. */
    if (strcmp(mode, "check") != 0 && !ws_truthy(hwnd))
        return ws_envelope(ws_failure("Microsoft Store request failed: ",
                                      "A desktop window is required for Microsoft Store consent"), 1);

    /* perform(): the query itself raised -> main()'s wrapper. */
    if (ws_truthy(get_error))
        return ws_envelope(ws_failure("Microsoft Store request failed: ", ws_text(get_error, "")), 1);

    names = json_array();
    for (i = 0; i < n; i++) {
        const json_t *p = json_get(packages, i);
        if (p && p->type == JSON_STRING) json_append(names, json_copy(p));
        else json_append(names, json_string(ws_text(p, "")));
    }
    result = json_object();
    json_set(result, "available", json_bool(available));
    json_set(result, "packages", names);
    json_set(result, "ok", json_bool(1));

    /* ``if mode == "check" or not updates: return result`` */
    if (strcmp(mode, "check") == 0 || !available) {
        exit_code = (strcmp(mode, "check") == 0 && available) ? 2 : 0;
        return ws_envelope(result, exit_code);
    }

    if (ws_truthy(request_error)) {
        json_free(result);
        return ws_envelope(ws_failure("Microsoft Store request failed: ", ws_text(request_error, "")), 1);
    }

    ok = 1;
    {
        const char *overall = ws_text(json_obj_get(plan, "overall_state"), "");
        const char *completed = ws_text(json_obj_get(plan, "completed_state"), "COMPLETED");
        if (strcmp(overall, completed) != 0) {
            char *buf;
            size_t need = strlen("Microsoft Store : ") + strlen(mode) + strlen(overall) + 1;
            ok = 0;
            buf = (char *)malloc(need);
            if (buf) {
                snprintf(buf, need, "Microsoft Store %s: %s", mode, overall);
                json_set(result, "error", json_string(buf));
                free(buf);
            }
        }
    }
    json_set(result, "ok", json_bool(ok));
    exit_code = ok ? 0 : 1;
    return ws_envelope(result, exit_code);
}

json_t *w25_windowssto_d94391_constants(void) {
    json_t *o = json_object();
    json_t *modes = json_array();
    json_append(modes, json_string("check"));
    json_append(modes, json_string("download"));
    json_append(modes, json_string("install"));
    json_set(o, "modes", modes);
    json_set(o, "default_mode", json_string("check"));
    json_set(o, "hwnd_flag", json_string("--hwnd"));
    json_set(o, "hwnd_type", json_string("int"));
    json_set(o, "timeout_s", json_number(1800.0));
    json_set(o, "poll_ms", json_number(50.0));
    return o;
}
