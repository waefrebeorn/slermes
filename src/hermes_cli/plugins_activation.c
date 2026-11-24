/*
 * plugins_activation.c -- faithful C11 port of the deterministic helpers in
 * hermes_cli/plugins_activation.py (wave17 gate w17_pluginsact_fc2ef6).
 *
 * PoP: w17_pluginsact_fc2ef6_find_activation @ hermes_cli/plugins_activation.py:find_activation
 * PoP: w17_pluginsact_fc2ef6_gateway_reload_hint @ hermes_cli/plugins_activation.py:gateway_reload_hint
 */
#include "noc_w17_pluginsact_fc2ef6.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s)
{
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* Python `str(entry.get("key") or "")` for the scalar keys used by the fixture. */
static void key_text(json_t *v, char *buf, size_t cap)
{
    buf[0] = '\0';
    if (!v || json_is_null(v)) return;
    if (json_is_string(v)) {
        const char *s = json_string_value(v);
        if (s) snprintf(buf, cap, "%s", s);
    } else if (json_is_bool(v)) {
        snprintf(buf, cap, "%s", json_bool_value(v) ? "True" : "False");
    } else if (json_is_number(v)) {
        double d = json_number_value(v);
        if (d == (double)(long long)d) snprintf(buf, cap, "%lld", (long long)d);
        else snprintf(buf, cap, "%.17g", d);
    }
}

/* PoP: w17_pluginsact_fc2ef6_find_activation @ hermes_cli/plugins_activation.py:find_activation */
json_t *w17_pluginsact_fc2ef6_find_activation(json_t *summaries, const char *name)
{
    if (!json_is_array(summaries)) return NULL;
    const char *wanted = name ? name : "";
    for (size_t i = 0; i < json_len(summaries); i++) {
        json_t *entry = json_get(summaries, i);
        if (!json_is_object(entry)) continue;
        char keybuf[512];
        json_t *kv = json_obj_get(entry, "key");
        key_text(kv, keybuf, sizeof(keybuf));
        if (strcmp(wanted, keybuf) == 0) return json_copy(entry);
        /* entry.get("name") -- only a string can equal our string `name`. */
        json_t *nv = json_obj_get(entry, "name");
        if (json_is_string(nv)) {
            const char *ns = json_string_value(nv);
            if (ns && strcmp(wanted, ns) == 0) return json_copy(entry);
        }
        /* key.rsplit("/", 1)[-1] */
        const char *leaf = strrchr(keybuf, '/');
        leaf = leaf ? leaf + 1 : keybuf;
        if (strcmp(wanted, leaf) == 0) return json_copy(entry);
    }
    return NULL;
}

static int json_truthy(json_t *v)
{
    if (!v || json_is_null(v)) return 0;
    if (json_is_bool(v)) return json_bool_value(v) ? 1 : 0;
    if (json_is_number(v)) return json_number_value(v) != 0.0;
    if (json_is_string(v)) { const char *s = json_string_value(v); return s && *s; }
    if (json_is_array(v) || json_is_object(v)) return json_len(v) != 0;
    return 0;
}

/* PoP: w17_pluginsact_fc2ef6_gateway_reload_hint @ hermes_cli/plugins_activation.py:gateway_reload_hint */
char *w17_pluginsact_fc2ef6_gateway_reload_hint(json_t *answer, const char *name)
{
    const char *who = name ? name : "";
    if (json_is_object(answer) && json_truthy(json_obj_get(answer, "reloaded"))) {
        char buf[1024];
        snprintf(buf, sizeof(buf),
                 "The running gateway reloaded its plugins and unloaded %s; platform hooks it "
                 "already wired up stay until a gateway restart (hermes gateway restart).", who);
        return dupstr(buf);
    }
    if (answer && !json_is_null(answer)) {
        json_t *err = json_obj_get(answer, "error");
        const char *reason = "no reason given";
        char ebuf[512];
        if (json_is_string(err) && json_string_value(err) && *json_string_value(err)) {
            snprintf(ebuf, sizeof(ebuf), "%s", json_string_value(err));
            reason = ebuf;
        }
        char buf[2048];
        snprintf(buf, sizeof(buf),
                 "The running gateway refused to reload its plugins: %s.\n"
                 "Only the saved config changed; restart the gateway serving this profile to apply it:\n"
                 "  hermes gateway restart", reason);
        return dupstr(buf);
    }
    return dupstr("No running gateway answered. If one is running, restart it for this to "
                  "take effect:\n  hermes gateway restart");
}

