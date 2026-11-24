/*
 * gateway_host_record.c — faithful C11 port of hermes_cli/gateway_host_record.py
 * (gate w18_gatewayhos_f57858).
 *
 * PoP: w18_gatewayhos_f57858_discard_dead_host_record @ hermes_cli/gateway_host_record.py:_discard_dead_host_record
 * PoP: w18_gatewayhos_f57858_forget_host_gateway_probe @ hermes_cli/gateway_host_record.py:_forget_host_gateway_probe
 *
 * Both live functions are thin late-import boundaries: they import a gateway
 * helper, call it, and swallow any exception (returning False / doing nothing).
 * The `sim` argument injects the outcome of that import+call so the boundary
 * itself is exercised deterministically instead of touching a live gateway.
 */
#include "noc_w18_gatewayhos_f57858.h"

#include <string.h>

/* "ok_true" -> 1, "ok_false" -> 0, any error -> 0 (the except branch). */
json_t *w18_gatewayhos_f57858_discard_dead_host_record(const char *sim) {
    if (sim && !strcmp(sim, "ok_true")) return json_bool(1);
    return json_bool(0);
}

/* The invalidate call happens whenever the late import succeeds — a raise from
 * inside it occurs after the call. The live function returns None, so report
 * whether the injected side effect took place. */
json_t *w18_gatewayhos_f57858_forget_host_gateway_probe(const char *sim) {
    /* Only a failed late import skips the call; a raise happens after it. */
    if (sim && !strcmp(sim, "import_error")) return json_bool(0);
    return json_bool(1);
}

