/*
 * placement.c - C11 port of upstream tools/bot_desktop/placement.py
 * (gate w24_placement_282518).
 *
 * PoP: w24_placement_282518_resolve @ tools/bot_desktop/placement.py:resolve
 * PoP: w24_placement_282518_constants @ tools/bot_desktop/placement.py:GATEWAY|TERMINAL|REFUSED|_STREAM_BACKENDS
 *
 * Ported: the whole decision table of ``resolve()`` -- the local-terminal
 * shortcut (taken before any config load upstream, and so kept first here),
 * the ``auto``/``terminal``/``gateway`` normalisation, the docker/ssh stream
 * backends, the plugin-backend stream flag with its plugin-host isolation
 * refusal, and both refusal sentences, which name the backend the user chose.
 *
 * Seams: ``_terminal_backend``, ``_setting``, ``_plugin_streams`` and
 * ``_plugin_host_isolated`` are arguments.  Nothing here loads config, starts a
 * sandbox or touches a terminal environment; ``terminal_environment()`` (which
 * would create the container) is deliberately not ported.
 *
 * ``_plugin_streams`` compares with ``is True``, so a JSON 1 or "yes" does NOT
 * count -- only a real boolean true does.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_placement_282518.h"

#define PL_GATEWAY  "gateway"
#define PL_TERMINAL "terminal"
#define PL_REFUSED  "refused"

#define REASON_LOCAL \
    "terminal.backend is local, so the terminal IS the gateway host"

#define REFUSED_ISOLATED_FMT \
    "terminal.backend is %s, a plugin that runs in the plugin host " \
    "(plugins.isolation: host), and the host cannot hand its sandbox's exec " \
    "channel to Hermes, so the screen cannot follow it. Set plugins.isolation: " \
    "in_process, or bot_desktop.placement: gateway to run the desktop on the " \
    "gateway host."

#define REFUSED_NO_SCREEN_FMT \
    "terminal.backend is %s, which cannot host a screen yet, and running the " \
    "desktop on the gateway host would put the agent's screen, browser and " \
    "computer_use outside the sandbox you chose for it. Set " \
    "bot_desktop.placement: gateway to allow that explicitly."

static char *xdup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    if (n) memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static int truthy(const json_t *v) {
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

/* str(v) for the shapes a config value can hold (containers are a seam). */
static char *py_str(const json_t *v) {
    char buf[64];
    switch (v ? v->type : JSON_NULL) {
    case JSON_STRING: return xdup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return xdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return xdup(buf);
    default: return xdup("");
    }
}

/* str(v or "local"): the terminal backend name with its default. */
static char *backend_name(const json_t *v) {
    if (!truthy(v)) return xdup("local");
    return py_str(v);
}

/* str(v or "auto").strip().lower(), then the membership fallback to "auto". */
static char *placement_setting(const json_t *v) {
    char *s = truthy(v) ? py_str(v) : xdup("auto");
    size_t n, a = 0, b;
    if (!s) return NULL;
    n = strlen(s);
    b = n;
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r' ||
                     s[a] == '\v' || s[a] == '\f')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' ||
                     s[b - 1] == '\r' || s[b - 1] == '\v' || s[b - 1] == '\f')) b--;
    memmove(s, s + a, b - a);
    s[b - a] = '\0';
    for (n = 0; s[n]; n++)
        if (s[n] >= 'A' && s[n] <= 'Z') s[n] = (char)(s[n] - 'A' + 'a');
    if (strcmp(s, "auto") && strcmp(s, PL_TERMINAL) && strcmp(s, PL_GATEWAY)) {
        free(s);
        return xdup("auto");
    }
    return s;
}

static json_t *placement(const char *where, const char *backend, const char *reason, char *owned_backend) {
    json_t *o = json_object();
    json_set(o, "where", json_string(where));
    json_set(o, "backend", json_string(backend ? backend : ""));
    json_set(o, "reason", json_string(reason ? reason : ""));
    if (owned_backend) free(owned_backend);
    return o;
}

static char *fmt_reason(const char *fmt, const char *backend) {
    size_t need = strlen(fmt) + strlen(backend ? backend : "") + 1;
    char *out = (char *)malloc(need);
    if (!out) return NULL;
    snprintf(out, need, fmt, backend ? backend : "");
    return out;
}

json_t *w24_placement_282518_resolve(const json_t *backend, const json_t *setting,
                                     const json_t *plugin_streams,
                                     const json_t *host_isolated) {
    char *name = backend_name(backend);
    char *cfg = NULL, *reason;

    if (!name) return json_null();

    /* A local terminal IS the gateway host, whatever the setting says. */
    if (!strcmp(name, "local")) return placement(PL_GATEWAY, name, REASON_LOCAL, name);

    cfg = placement_setting(setting);
    if (!cfg) { free(name); return json_null(); }
    if (!strcmp(cfg, PL_GATEWAY)) {
        free(cfg);
        return placement(PL_GATEWAY, name, "", name);
    }
    free(cfg);

    if (!strcmp(name, "docker") || !strcmp(name, "ssh"))
        return placement(PL_TERMINAL, name, "", name);

    /* _plugin_streams(...) is True */
    if (plugin_streams && plugin_streams->type == JSON_BOOL && plugin_streams->bool_val) {
        if (!truthy(host_isolated)) return placement(PL_TERMINAL, name, "", name);
        reason = fmt_reason(REFUSED_ISOLATED_FMT, name);
        {
            json_t *out = placement(PL_REFUSED, name, reason ? reason : "", name);
            free(reason);
            return out;
        }
    }
    reason = fmt_reason(REFUSED_NO_SCREEN_FMT, name);
    {
        json_t *out = placement(PL_REFUSED, name, reason ? reason : "", name);
        free(reason);
        return out;
    }
}

json_t *w24_placement_282518_constants(void) {
    json_t *o = json_object(), *streams = json_array();
    json_set(o, "gateway", json_string(PL_GATEWAY));
    json_set(o, "terminal", json_string(PL_TERMINAL));
    json_set(o, "refused", json_string(PL_REFUSED));
    json_append(streams, json_string("docker"));
    json_append(streams, json_string("ssh"));
    json_set(o, "stream_backends", streams);
    return o;
}
