/*
 * onboarding_session.c — C home for upstream Hermes agent/onboarding_session.py.
 *
 * PoP: w10_onboarding_ec6652_builds_onboarding @ agent/onboarding_session.py:builds_onboarding
 * PoP: w10_onboarding_ec6652_keeps_toolsets @ agent/onboarding_session.py:keeps_toolsets
 * PoP: w10_onboarding_ec6652_with_onboarding @ agent/onboarding_session.py:with_onboarding
 * PoP: w10_onboarding_ec6652_carry_onboarding_toolset @ agent/onboarding_session.py:carry_onboarding_toolset
 * PoP: w10_onboarding_ec6652_append_turn_note @ agent/onboarding_session.py:append_turn_note
 *
 * The pure per-session toolset fold-in and note accumulation. DB/record-driven
 * gates (onboarding_toolsets / onboarding_chat_toolsets / prelude / turn_note)
 * need live session/DB collaborators and stay in Python.
 */
#include "noc_w10_onboarding_ec6652.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_BOOL) return v->bool_val ? 0 : 1;
    if (v->type == JSON_NUMBER) return v->num_val == 0.0;
    if (v->type == JSON_STRING) return v->str_val[0] == '\0';
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count == 0;
    return 1;
}

/* Python `x in collection` for a JSON array (element strings) or string (substring). */
static int contains(const json_t *coll, const char *needle) {
    if (!coll) return 0;
    if (coll->type == JSON_STRING) return coll->str_val && strstr(coll->str_val, needle) != NULL;
    if (coll->type == JSON_ARRAY) {
        for (size_t i = 0; i < coll->c.count; i++) {
            const json_t *v = coll->c.items[i];
            if (v && v->type == JSON_STRING && !strcmp(v->str_val, needle)) return 1;
        }
    }
    return 0;
}

int w10_onboarding_ec6652_builds_onboarding(const json_t *valid_tool_names) {
    return contains(valid_tool_names, "setup_beat");
}

int w10_onboarding_ec6652_keeps_toolsets(const json_t *enabled) {
    if (falsy(enabled)) return enabled != NULL && enabled->type != JSON_NULL;
    return contains(enabled, "onboarding");
}

json_t *w10_onboarding_ec6652_with_onboarding(const json_t *enabled,
                                              const json_t *all_toolsets,
                                              const json_t *session_platforms) {
    json_t *out = json_array();
    if (enabled && enabled->type == JSON_NULL) {
        size_t n = (all_toolsets && all_toolsets->type == JSON_ARRAY) ? all_toolsets->c.count : 0;
        for (size_t i = 0; i < n; i++) {
            const json_t *v = all_toolsets->c.items[i];
            if (!v || v->type != JSON_STRING) continue;
            if (contains(session_platforms, v->str_val)) continue;
            json_append(out, json_copy(v));
        }
    } else if (enabled && enabled->type == JSON_STRING) {
        for (const char *p = enabled->str_val; p && *p; p++) {
            char one[2] = {*p, '\0'};
            json_append(out, json_string(one));
        }
    } else if (enabled && enabled->type == JSON_ARRAY) {
        for (size_t i = 0; i < enabled->c.count; i++)
            json_append(out, json_copy(enabled->c.items[i]));
    }
    json_append(out, json_string("onboarding"));
    return out;
}

json_t *w10_onboarding_ec6652_carry_onboarding_toolset(const json_t *enabled,
                                                       const json_t *agent_toolsets) {
    if (w10_onboarding_ec6652_keeps_toolsets(enabled) || !contains(agent_toolsets, "onboarding")) {
        if (!enabled || enabled->type == JSON_NULL) return json_null();
        return json_copy(enabled);
    }
    return w10_onboarding_ec6652_with_onboarding(enabled, NULL, NULL);
}

char *w10_onboarding_ec6652_append_turn_note(const char *prior, const char *note) {
    if (!prior) prior = "";
    if (!note) note = "";
    if (!note[0]) return xstrdup(prior);
    if (!prior[0]) return xstrdup(note);
    size_t n = strlen(prior) + 2 + strlen(note) + 1;
    char *out = (char *)malloc(n);
    if (!out) return xstrdup(prior);
    snprintf(out, n, "%s\n\n%s", prior, note);
    return out;
}

