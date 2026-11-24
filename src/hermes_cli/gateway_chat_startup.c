/*
 * gateway_chat_startup.c -- C11 port of hermes_cli/gateway_chat_startup.py.
 *
 * Both entry points are control flow around seams: bypass_launch, the TUI
 * gateway URL, the provider probe, the bootstrap/guard side effects, and the
 * import block / launch call. The flow itself (which branch wins, what is
 * skipped, whether consent runs) is real and asserted per branch.
 *
 * PoP: w22_gatewaycha_382a68_ensure_launch_provider @ hermes_cli/gateway_chat_startup.py:ensure_launch_provider
 * PoP: w22_gatewaycha_382a68_launch_gateway_chat @ hermes_cli/gateway_chat_startup.py:launch_gateway_chat
 */
#include "hermes_json.h"
#include "noc_w22_gatewaycha_382a68.h"

#include <stddef.h>
#include <string.h>

static int gc_truthy(const json_t *v) {
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

/* `(HERMES_TUI_GATEWAY_URL or "").strip()` has content. */
static int gc_url_present(const char *url) {
    if (!url) return 0;
    while (*url == ' ' || *url == '\t' || *url == '\n' || *url == '\r' || *url == '\f' || *url == '\v') url++;
    while (*url) {
        if (*url != ' ' && *url != '\t' && *url != '\n' && *url != '\r' &&
            *url != '\f' && *url != '\v') return 1;
        url++;
    }
    return 0;
}

json_t *w22_gatewaycha_382a68_ensure_launch_provider(int bypass, const json_t *args,
                                                     const char *gateway_url,
                                                     int provider_configured) {
    json_t *out = json_object();
    int bootstrap_called = 0, provider_checked = 0, guard_called = 0, ready;

    if (bypass) {
        ready = 1;
    } else if (gc_truthy(json_obj_get(args, "resume")) ||
               gc_url_present(gateway_url) ||
               gc_truthy(json_obj_get(args, "base_url")) ||
               gc_truthy(json_obj_get(args, "api_key"))) {
        ready = 1;
    } else {
        bootstrap_called = 1;
        provider_checked = 1;
        if (provider_configured) {
            ready = 1;
        } else {
            guard_called = 1;
            ready = 0;
        }
    }

    json_set(out, "ready", json_bool(ready));
    json_set(out, "bootstrap_called", json_bool(bootstrap_called));
    json_set(out, "provider_checked", json_bool(provider_checked));
    json_set(out, "guard_called", json_bool(guard_called));
    return out;
}

json_t *w22_gatewaycha_382a68_launch_gateway_chat(int bypass, int import_ok,
                                                  int hint_handled, long code) {
    json_t *out = json_object();

    if (!import_ok) {
        json_set(out, "hint_called", json_bool(1));
        json_set(out, "confirmed", json_bool(0));
        if (hint_handled) {
            json_set(out, "outcome", json_string("returned"));
            json_set(out, "code", json_number(1));
        } else {
            json_set(out, "outcome", json_string("raised"));
        }
        return out;
    }
    json_set(out, "hint_called", json_bool(0));
    json_set(out, "confirmed", json_bool(bypass ? 0 : 1));
    json_set(out, "outcome", json_string("returned"));
    json_set(out, "code", json_number((double)code));
    return out;
}
