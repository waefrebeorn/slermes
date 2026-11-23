/*
 * cli_init_mixin.c — C port of the pure helpers in upstream Hermes
 * hermes_cli/cli_init_mixin.py.
 *
 * PoP: w12_cliinitmi_95d44d_explicit_launch_skills @ hermes_cli/cli_init_mixin.py:_explicit_launch_skills
 * PoP: w12_cliinitmi_95d44d_gateway_launch @ hermes_cli/cli_init_mixin.py:_gateway_launch
 * PoP: w12_cliinitmi_95d44d_adopt_gateway_toolsets @ hermes_cli/cli_init_mixin.py:_adopt_gateway_toolsets
 *
 * CLI attributes, resolved auto-load skills and the store snapshot are
 * injected from the fixture; t() is the identity key.
 */
#include "noc_w12_cliinitmi_95d44d.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINEBUF 8192

static json_t *jget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *jstr(const json_t *o, const char *k, const char *d) {
    return (o && o->type == JSON_OBJECT) ? json_get_str(o, k, d) : d;
}
static int jtruthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return 0;
}
/* getattr(cli, key, default) then `or None` — preserves a truthy value's type. */
static json_t *attr_or_null(const json_t *cli, const char *key) {
    json_t *v = jget(cli, key);
    return jtruthy(v) ? json_copy(v) : json_null();
}
static int in_array(const json_t *arr, const char *needle) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *s = json_string_value(json_get(arr, i));
        if (s && !strcmp(s, needle)) return 1;
    }
    return 0;
}

struct json_t *w12_cliinitmi_95d44d_explicit_launch_skills(const struct json_t *cli,
                                                           const struct json_t *auto_skills) {
    json_t *requested = jget(cli, "_preload_skills_requested");
    if (!jtruthy(requested) || requested->type != JSON_ARRAY) return json_null();
    json_t *out = json_array();
    int ignore = jtruthy(jget(cli, "ignore_rules"));
    for (size_t i = 0; i < json_len(requested); i++) {
        const char *nm = json_string_value(json_get(requested, i));
        if (!ignore && nm && in_array(auto_skills, nm)) continue;
        json_append(out, json_string(nm ? nm : ""));
    }
    if (json_len(out) == 0) { json_free(out); return json_null(); }
    return out;
}

struct json_t *w12_cliinitmi_95d44d_gateway_launch(const struct json_t *cli,
                                                   const struct json_t *auto_skills) {
    json_t *o = json_object();
    int explicit_model = jtruthy(jget(cli, "_explicit_model_override"));
    const char *provider = jstr(cli, "_explicit_provider", "");
    json_set(o, "model", explicit_model ? json_string(jstr(cli, "model", "")) : json_null());
    if (explicit_model && *provider && strcmp(provider, "auto") != 0)
        json_set(o, "provider", json_string(provider));
    else
        json_set(o, "provider", json_null());
    json_set(o, "api_key", attr_or_null(cli, "_explicit_api_key"));
    json_set(o, "base_url", attr_or_null(cli, "_explicit_base_url"));
    json_set(o, "toolsets", jtruthy(jget(cli, "_explicit_toolsets"))
             ? attr_or_null(cli, "enabled_toolsets") : json_null());
    json_set(o, "reasoning", attr_or_null(cli, "_explicit_reasoning"));
    json_set(o, "max_turns", attr_or_null(cli, "_explicit_max_turns"));
    json_set(o, "checkpoints", attr_or_null(cli, "_explicit_checkpoints"));
    json_set(o, "pass_session_id", attr_or_null(cli, "pass_session_id"));
    json_set(o, "ignore_rules", attr_or_null(cli, "ignore_rules"));
    json_set(o, "run_budget", attr_or_null(cli, "_explicit_run_budget"));
    json_set(o, "verbose", attr_or_null(cli, "verbose"));
    json_set(o, "skills", w12_cliinitmi_95d44d_explicit_launch_skills(cli, auto_skills));
    return o;
}

struct json_t *w12_cliinitmi_95d44d_adopt_gateway_toolsets(const struct json_t *cli,
                                                           const struct json_t *store) {
    json_t *out = json_object();
    json_t *current = jget(store, "current");
    json_t *info = jget(current, "info");
    if (!jtruthy(info)) info = NULL;

    json_t *cur_ts = jget(cli, "enabled_toolsets");
    if ((!cur_ts || cur_ts->type == JSON_NULL)) {
        json_t *from_info = jget(info, "enabled_toolsets");
        if (from_info && from_info->type == JSON_ARRAY)
            json_set(out, "enabled_toolsets", json_copy(from_info));
        else
            json_set(out, "enabled_toolsets", json_null());
    } else {
        json_set(out, "enabled_toolsets", json_copy(cur_ts));
    }

    json_t *lines = json_array();
    json_t *unknown = jget(info, "unknown_toolsets");
    if (jtruthy(unknown)) {
        char joined[LINEBUF];
        joined[0] = '\0';
        if (unknown->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(unknown); i++) {
                const char *u = json_string_value(json_get(unknown, i));
                if (i) strncat(joined, ", ", sizeof(joined) - strlen(joined) - 1);
                strncat(joined, u ? u : "", sizeof(joined) - strlen(joined) - 1);
            }
        }
        (void)joined;  /* _t is the identity key; names do not reach the line */
        json_append(lines, json_string("[bold red]cli.startup.unknown_toolsets[/]"));
    }
    json_set(out, "stdout", lines);
    return out;
}
