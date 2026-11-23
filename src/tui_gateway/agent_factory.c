/*
 * agent_factory.c — C port of upstream Hermes tui_gateway/agent_factory.py.
 *
 * PoP: w13_agentfacto_542e88_parse_model_config @ tui_gateway/agent_factory.py:_parse_model_config
 * PoP: w13_agentfacto_542e88_row_follows_profile @ tui_gateway/agent_factory.py:_row_follows_profile
 * PoP: w13_agentfacto_542e88_display_mouse_tracking @ tui_gateway/agent_factory.py:_display_mouse_tracking
 * PoP: w13_agentfacto_542e88_coerce_statusbar @ tui_gateway/agent_factory.py:_coerce_statusbar
 *
 * Server-bound names used by these helpers (_STATUSBAR_MODES) are injected by
 * the oracle from tui_gateway/server.py's definition.
 */
#include "noc_w13_agentfacto_542e88.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *trim_copy(const char *s, char *buf, size_t n) {
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    size_t len = (size_t)(e - s);
    if (len >= n) len = n - 1;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

static const char *lower_copy(const char *s, char *buf, size_t n) {
    const char *t = trim_copy(s, buf, n);
    size_t len = strlen(t);
    for (size_t i = 0; i < len; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
    return buf;
}

static int py_bool(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}

struct json_t *w13_agentfacto_542e88_parse_model_config(const struct json_t *raw, int quiet) {
    (void)quiet; /* error path always yields {} in the harness */
    if (json_is_object(raw)) return json_copy(raw);
    if (raw && raw->type == JSON_STRING) {
        char buf[32768];
        if (!trim_copy(raw->str_val, buf, sizeof buf)[0]) return json_object();
        struct json_t *parsed = json_parse(buf, NULL);
        if (parsed) {
            if (parsed->type == JSON_OBJECT) return parsed;
            json_free(parsed);
        }
    }
    return json_object();
}

int w13_agentfacto_542e88_row_follows_profile(const struct json_t *row) {
    if (!json_is_object(row)) return 0;
    struct json_t *mc = w13_agentfacto_542e88_parse_model_config(json_obj_get(row, "model_config"), 1);
    int follow = py_bool(json_obj_get(mc, "follow_profile_config"));
    json_free(mc);
    if (follow) return 1;
    char tbuf[512];
    const char *title = trim_copy(json_get_str(row, "title", ""), tbuf, sizeof tbuf);
    return strcmp(title, "Bot Chat") == 0 ? 1 : 0;
}

const char *w13_agentfacto_542e88_display_mouse_tracking(const struct json_t *display) {
    static const char *keys[] = {
        "0", "1", "all", "any", "button", "buttons", "click", "false", "full", "no",
        "off", "on", "scroll", "true", "wheel", "yes", NULL};
    static const char *vals[] = {
        "off", "all", "all", "all", "buttons", "buttons", "buttons", "off", "all", "off",
        "off", "all", "wheel", "all", "wheel", "all", NULL};
    if (!json_is_object(display)) return "all";
    const struct json_t *raw = json_obj_get(display, "mouse_tracking");
    if (!json_has(display, "mouse_tracking")) {
        /* display.get("tui_mouse", True) */
        const struct json_t *tm = json_obj_get(display, "tui_mouse");
        if (!tm) return "all"; /* default True -> "all" */
        raw = tm;
    }
    if (raw && raw->type == JSON_STRING) {
        char buf[256];
        const char *s = lower_copy(raw->str_val, buf, sizeof buf);
        for (int i = 0; keys[i]; i++)
            if (!strcmp(s, keys[i])) return vals[i];
        return "all";
    }
    /* raw is False or raw == 0 -> "off", else "all" */
    if ((raw && raw->type == JSON_BOOL && !raw->bool_val) ||
        (raw && raw->type == JSON_NUMBER && raw->num_val == 0.0)) return "off";
    return "all";
}

const char *w13_agentfacto_542e88_coerce_statusbar(const struct json_t *raw) {
    if (raw && raw->type == JSON_BOOL && !raw->bool_val) return "off"; /* raw is False */
    if (raw && raw->type == JSON_STRING) {
        char buf[256];
        const char *s = lower_copy(raw->str_val, buf, sizeof buf);
        if (!strcmp(s, "off")) return "off";
        if (!strcmp(s, "top")) return "top";
        if (!strcmp(s, "bottom")) return "bottom";
    }
    return "top";
}
