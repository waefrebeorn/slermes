/*
 * plugin_events.c — faithful C11 port of the pure naming/validation helpers in
 * upstream Hermes hermes_cli/plugin_events.py (gate w10_plugineven_e257fc).
 *
 * PoP: w10_plugineven_e257fc_event_name        @ hermes_cli/plugin_events.py:plugin_event_name
 * PoP: w10_plugineven_e257fc_session_event_body @ hermes_cli/plugin_events.py:emit_plugin_session_event
 *
 * The broadcast/forwarder tails are effectful and out of scope; only the
 * deterministic name composition and validation are ported here.
 */
#include "noc_w10_plugineven_e257fc.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

static json_t *err_result(const char *which) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(0));
    json_set(o, "error", json_string(which));
    return o;
}

static int id_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}
static int seg_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-';
}

/* ^[a-z0-9_-]{1,64}$ */
static int valid_plugin_id(const char *s) {
    if (!s) return 0;
    size_t n = strlen(s);
    if (n < 1 || n > 64) return 0;
    for (size_t i = 0; i < n; i++) if (!id_char(s[i])) return 0;
    return 1;
}

/* ^[A-Za-z0-9_-]+(?:\.[A-Za-z0-9_-]+)*$  and  len(event) <= 128 */
static int valid_event(const char *s) {
    if (!s) return 0;
    size_t n = strlen(s);
    if (n < 1 || n > 128) return 0;
    size_t i = 0;
    while (i < n) {
        size_t start = i;
        while (i < n && seg_char(s[i])) i++;
        if (i == start) return 0;              /* empty segment */
        if (i < n && s[i] != '.') return 0;    /* illegal character */
        if (i < n) i++;                        /* skip the dot */
    }
    return 1;
}

json_t *w10_plugineven_e257fc_event_name(const char *plugin_id, const char *event) {
    if (!valid_plugin_id(plugin_id)) return err_result("ValueError");
    if (!valid_event(event)) return err_result("ValueError");
    size_t a = strlen(plugin_id), b = strlen(event);
    char buf[8 + 64 + 1 + 128 + 2];
    memcpy(buf, "plugin.", 7);
    memcpy(buf + 7, plugin_id, a);
    buf[7 + a] = '.';
    memcpy(buf + 8 + a, event, b);
    buf[8 + a + b] = '\0';
    json_t *o = json_object();
    json_set(o, "ok", json_bool(1));
    json_set(o, "value", json_string(buf));
    return o;
}

json_t *w10_plugineven_e257fc_session_event_body(const char *session_id,
                                                 const char *plugin_id,
                                                 const char *event,
                                                 const json_t *payload) {
    if (!session_id || !session_id[0]) return err_result("ValueError");
    if (payload && payload->type != JSON_OBJECT && payload->type != JSON_NULL)
        return err_result("TypeError");
    json_t *named = w10_plugineven_e257fc_event_name(plugin_id, event);
    json_t *v = json_obj_get(named, "ok");
    if (!v || !v->bool_val) { json_free(named); return err_result("ValueError"); }
    json_t *body = (payload && payload->type == JSON_OBJECT) ? json_copy(payload) : json_object();
    json_t *o = json_object();
    json_set(o, "ok", json_bool(1));
    json_set(o, "name", json_copy(json_obj_get(named, "value")));
    json_set(o, "body", body);
    json_free(named);
    return o;
}
