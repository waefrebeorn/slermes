/*
 * onboarding_flow.c — C port of upstream Hermes session_rules/onboarding_flow.py.
 *
 * PoP: w17_onboarding_4983bc_next_beat @ session_rules/onboarding_flow.py:next_beat
 * PoP: w17_onboarding_4983bc_is_active @ session_rules/onboarding_flow.py:is_active
 * PoP: w17_onboarding_4983bc_applies_to @ session_rules/onboarding_flow.py:applies_to
 * PoP: w17_onboarding_4983bc_has_suggestions @ session_rules/onboarding_flow.py:_has_suggestions
 * PoP: w17_onboarding_4983bc_is_names @ session_rules/onboarding_flow.py:_is_names
 * PoP: w17_onboarding_4983bc_valid_answer @ session_rules/onboarding_flow.py:valid_answer
 * PoP: w17_onboarding_4983bc_build_values @ session_rules/onboarding_flow.py:build_values
 * PoP: w17_onboarding_4983bc_constants @ session_rules/onboarding_flow.py:PENDING/SHOWN/DONE/SKIPPED/ONBOARDING/COMPLETE/PHASE_SKIPPED/CARD_TIMEOUT_S/MAX_PLUGINS/BEAT_IDS/LAYOUT_OPTIONS/TOUR_OPTIONS
 */
#include "noc_w17_onboarding_4983bc.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* FIXED_ID / OWN_ID come from session_rules/onboarding_first_tasks.py. */
static const char *const FIXED_ID = "hermes";
static const char *const OWN_ID = "own";

static const char *const BEAT_IDS[] = {
    "name", "theme", "layout", "connectors", "plugins", "tour", "build"
};
static const size_t BEAT_COUNT = sizeof(BEAT_IDS) / sizeof(BEAT_IDS[0]);
static const char *const LAYOUT_IDS[] = { "sidebar-left", "terminal-deck" };
static const char *const TOUR_IDS[] = { "quick", "full", "skip" };

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}
static bool is_space_str(const char *s) {
    if (!s) return true;
    while (*s) { if (!isspace((unsigned char)*s)) return false; s++; }
    return true;
}
/* Python str() of a JSON value, enough for _has_suggestions' str(slug). */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof(b), "%.17g", v->num_val);
        return dupstr(b);
    }
    if (v->type == JSON_ARRAY) return dupstr("[]");
    return dupstr("{}");
}
static bool is_names(const json_t *v) {
    if (!v || v->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(v); i++) {
        json_t *e = json_get(v, i);
        if (!e || e->type != JSON_STRING) return false;
    }
    return true;
}
static const json_t *beats_of(const json_t *record) {
    if (!record || record->type != JSON_OBJECT) return NULL;
    return json_obj_get(record, "beats");
}
static const char *beat_state(const json_t *record, const char *beat) {
    const json_t *beats = beats_of(record);
    if (!beats || beats->type != JSON_OBJECT) return NULL;
    json_t *v = json_obj_get(beats, beat);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

json_t *w17_onboarding_4983bc_next_beat(const json_t *record) {
    for (size_t i = 0; i < BEAT_COUNT; i++) {
        const char *st = beat_state(record, BEAT_IDS[i]);
        if (st && (!strcmp(st, "pending") || !strcmp(st, "shown")))
            return json_string(BEAT_IDS[i]);
    }
    return json_null();
}

json_t *w17_onboarding_4983bc_is_active(const json_t *record) {
    if (!record || record->type != JSON_OBJECT || json_object_size(record) == 0) return json_bool(false);
    json_t *phase = json_obj_get(record, "phase");
    return json_bool(phase && phase->type == JSON_STRING && phase->str_val
                     && !strcmp(phase->str_val, "onboarding"));
}

json_t *w17_onboarding_4983bc_applies_to(const char *tool) {
    if (!tool) return json_bool(false);
    return json_bool(!strcmp(tool, "set_theme") || !strcmp(tool, "manage_connections")
                     || !strcmp(tool, "gui_tour"));
}

json_t *w17_onboarding_4983bc_has_suggestions(const json_t *suggest) {
    if (!suggest || suggest->type != JSON_ARRAY) return json_bool(false);
    for (size_t i = 0; i < json_len(suggest); i++) {
        char *s = py_str(json_get(suggest, i));
        bool truthy = s && !is_space_str(s);
        free(s);
        if (truthy) return json_bool(true);
    }
    return json_bool(false);
}

json_t *w17_onboarding_4983bc_is_names(const json_t *value) {
    return json_bool(is_names(value));
}

/* Which beat is this, and its accepted pick keys. */
static size_t beat_index(const char *beat) {
    if (!beat) return (size_t)-1;
    for (size_t i = 0; i < BEAT_COUNT; i++) if (!strcmp(beat, BEAT_IDS[i])) return i;
    return (size_t)-1;
}
static const char *const *picks_for(size_t idx, size_t *count) {
    static const char *const name[] = { "name" };
    static const char *const theme[] = { "accent", "mode" };
    static const char *const layout[] = { "layout" };
    static const char *const connectors[] = { "connected", "newly_connected" };
    static const char *const plugins[] = { "installed" };
    static const char *const tour[] = { "tour" };
    static const char *const build[] = { "build" };
    switch (idx) {
        case 0: *count = 1; return name;
        case 1: *count = 2; return theme;
        case 2: *count = 1; return layout;
        case 3: *count = 2; return connectors;
        case 4: *count = 1; return plugins;
        case 5: *count = 1; return tour;
        case 6: *count = 1; return build;
        default: *count = 0; return NULL;
    }
}
static bool key_in(const char *const *list, size_t n, const char *k) {
    for (size_t i = 0; i < n; i++) if (!strcmp(list[i], k)) return true;
    return false;
}
static bool str_in(const char *const *list, size_t n, const json_t *v) {
    if (!v || v->type != JSON_STRING || !v->str_val) return false;
    for (size_t i = 0; i < n; i++) if (!strcmp(list[i], v->str_val)) return true;
    return false;
}
static bool facts_plugin_has(const json_t *facts, const char *slug) {
    if (!facts || facts->type != JSON_OBJECT) return false;
    json_t *ps = json_obj_get(facts, "plugins");
    if (!ps || ps->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(ps); i++) {
        json_t *v = json_get(ps, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, slug)) return true;
    }
    return false;
}

json_t *w17_onboarding_4983bc_valid_answer(const char *beat, const json_t *answer,
                                           const json_t *facts, const json_t *picks) {
    if (!answer || answer->type != JSON_OBJECT) return json_bool(false);
    size_t idx = beat_index(beat);
    if (idx == (size_t)-1) return json_bool(false);
    /* isinstance(answer.get("label", ""), str) */
    json_t *lbl = json_obj_get(answer, "label");
    if (lbl && lbl->type != JSON_STRING) return json_bool(false);
    /* strip label; require non-empty and all keys within the beat's picks */
    size_t nkeys = 0, npicks = 0;
    const char *const *picks_list = picks_for(idx, &npicks);
    for (size_t i = 0; i < json_object_size(answer); i++) {
        const char *k = json_object_get_key_at(answer, i);
        if (!k || !strcmp(k, "label")) continue;
        nkeys++;
        if (!key_in(picks_list, npicks, k)) return json_bool(false);
    }
    if (nkeys == 0) return json_bool(false);

    if (idx == 2 /* layout */) {
        json_t *v = json_obj_get(answer, "layout");
        return json_bool(str_in(LAYOUT_IDS, sizeof(LAYOUT_IDS)/sizeof(LAYOUT_IDS[0]), v));
    }
    if (idx == 5 /* tour */) {
        json_t *v = json_obj_get(answer, "tour");
        return json_bool(str_in(TOUR_IDS, sizeof(TOUR_IDS)/sizeof(TOUR_IDS[0]), v));
    }
    if (idx == 6 /* build */) {
        json_t *vals = w17_onboarding_4983bc_build_values(facts, picks);
        json_t *v = json_obj_get(answer, "build");
        bool ok = false;
        if (v && v->type == JSON_STRING && v->str_val) {
            for (size_t i = 0; i < json_len(vals); i++) {
                json_t *e = json_get(vals, i);
                if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, v->str_val)) { ok = true; break; }
            }
        }
        json_free(vals);
        return json_bool(ok);
    }
    if (idx == 0 /* name */) {
        json_t *v = json_obj_get(answer, "name");
        return json_bool(v && v->type == JSON_STRING && v->str_val && !is_space_str(v->str_val));
    }
    if (idx == 1 /* theme */) {
        for (size_t i = 0; i < json_object_size(answer); i++) {
            const char *k = json_object_get_key_at(answer, i);
            if (!k || !strcmp(k, "label")) continue;
            json_t *v = json_object_get_at(answer, i);
            if (v && v->type != JSON_NULL && v->type != JSON_STRING) return json_bool(false);
        }
        return json_bool(true);
    }
    if (idx == 4 /* plugins */) {
        json_t *installed = json_obj_get(answer, "installed");
        if (!installed || !is_names(installed)) return json_bool(false);
        for (size_t i = 0; i < json_len(installed); i++) {
            json_t *e = json_get(installed, i);
            if (!e || !e->str_val || !facts_plugin_has(facts, e->str_val)) return json_bool(false);
        }
        return json_bool(true);
    }
    /* connectors (default branch) */
    if (!json_obj_get(answer, "connected")) return json_bool(false);
    for (size_t i = 0; i < json_object_size(answer); i++) {
        const char *k = json_object_get_key_at(answer, i);
        if (!k || !strcmp(k, "label")) continue;
        if (!is_names(json_object_get_at(answer, i))) return json_bool(false);
    }
    return json_bool(true);
}

json_t *w17_onboarding_4983bc_build_values(const json_t *facts, const json_t *picks) {
    (void)facts;
    json_t *out = json_array();
    json_t *opts = (picks && picks->type == JSON_OBJECT) ? json_obj_get(picks, "build_options") : NULL;
    if (opts && opts->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(opts); i++) {
            json_t *o = json_get(opts, i);
            json_t *id = (o && o->type == JSON_OBJECT) ? json_obj_get(o, "id") : NULL;
            json_append(out, json_string((id && id->type == JSON_STRING && id->str_val) ? id->str_val : ""));
        }
    }
    json_append(out, json_string(FIXED_ID));
    json_append(out, json_string(OWN_ID));
    return out;
}

json_t *w17_onboarding_4983bc_constants(void) {
    json_t *out = json_object();
    json_set(out, "PENDING", json_string("pending"));
    json_set(out, "SHOWN", json_string("shown"));
    json_set(out, "DONE", json_string("done"));
    json_set(out, "SKIPPED", json_string("skipped"));
    json_set(out, "ONBOARDING", json_string("onboarding"));
    json_set(out, "COMPLETE", json_string("complete"));
    json_set(out, "PHASE_SKIPPED", json_string("skipped"));
    json_set(out, "CARD_TIMEOUT_S", json_number(300));
    json_set(out, "MAX_PLUGINS", json_number(5));
    json_set(out, "FIXED_ID", json_string(FIXED_ID));
    json_set(out, "OWN_ID", json_string(OWN_ID));
    json_t *ids = json_array();
    for (size_t i = 0; i < BEAT_COUNT; i++) json_append(ids, json_string(BEAT_IDS[i]));
    json_set(out, "BEAT_IDS", ids);
    json_t *lay = json_array();
    for (size_t i = 0; i < 2; i++) json_append(lay, json_string(LAYOUT_IDS[i]));
    json_set(out, "LAYOUT_IDS", lay);
    json_t *tour = json_array();
    for (size_t i = 0; i < 3; i++) json_append(tour, json_string(TOUR_IDS[i]));
    json_set(out, "TOUR_IDS", tour);
    return out;
}
