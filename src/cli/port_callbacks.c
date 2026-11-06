/*
 * port_callbacks.c — Port of hermes_cli/callbacks.py::_skipped: the "value was
 * stored under a user-typed variable and skipped validation" result dict.
 */

#include "port_callbacks.h"

#include <stdlib.h>
#include <string.h>

/* PoP: _skipped @ hermes_cli/callbacks.py:_skipped */
json_t *cb_skipped(const char *var_name, const char *reason, const char *message)
{
    json_t *obj = json_object();
    if (!obj) return NULL;
    json_set(obj, "success", json_bool(true));
    json_set(obj, "reason", json_string(reason ? reason : ""));
    json_set(obj, "stored_as", json_string(var_name ? var_name : ""));
    json_set(obj, "validated", json_bool(false));
    json_set(obj, "skipped", json_bool(true));
    json_set(obj, "message", json_string(message ? message : ""));
    return obj;
}
