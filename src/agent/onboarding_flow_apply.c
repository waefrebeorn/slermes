/* onboarding_flow_apply.c -- canonical C11 port of session_rules/onboarding_flow.py:apply.
 *
 * First-run onboarding as data: the beats, the run record, and how each event moves the record.
 * The Python is pure (no I/O, no clock); so is this, which is what makes the two comparable case by
 * case -- see the differential harness in tests/oracle/onboarding_flow_oracle.py.
 *
 * The one seam: the build beat's eligible ids come from session_rules/onboarding_first_tasks.py's
 * catalog (task table + per-OS gates), which is data ported separately. `eligible_ids` supplies them.
 */
#include "onboarding_flow_apply.h"
#include "hermes_json.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PENDING "pending"
#define SHOWN   "shown"
#define DONE    "done"
#define SKIPPED "skipped"

#define PHASE_ONBOARDING "onboarding"
#define PHASE_COMPLETE   "complete"
#define PHASE_SKIPPED    "skipped"

#define MAX_PICKS 4
#define MAX_LABEL 32

typedef struct {
    const char *id;
    const char *picks[3];          /* NULL-terminated */
    const char *value;             /* pick a value given in words sets; NULL = no value */
    const char *const *values;     /* accepted ids; NULL = dynamic or not checked here */
    int when;                      /* 0 always, 1 needs facts.plugins, 2 needs facts.tours */
} beat_t;

static const char *const LAYOUT_IDS[] = { "sidebar-left", "terminal-deck", NULL };
static const char *const TOUR_IDS[]   = { "quick", "full", "skip", NULL };

static const beat_t BEATS[] = {
    { "name",       { "name", NULL },                                  "name",   NULL,        0 },
    { "theme",      { "accent", "mode", NULL },                        "accent", NULL,        0 },
    { "layout",     { "layout", NULL },                                "layout", LAYOUT_IDS,  0 },
    { "connectors", { "connected", "newly_connected", NULL },          NULL,     NULL,        0 },
    { "plugins",    { "installed", NULL },                             NULL,     NULL,        1 },
    { "tour",       { "tour", NULL },                                  "tour",   TOUR_IDS,    2 },
    { "build",      { "build", NULL },                                 "build",  NULL,        0 },
};
#define BEAT_COUNT (int)(sizeof BEATS / sizeof BEATS[0])

const char *const ONBOARDING_FLOW_BEAT_IDS[] = {
    "name", "theme", "layout", "connectors", "plugins", "tour", "build", NULL
};

/* ── small helpers ─────────────────────────────────────────────────────────────────────────── */

static void set_str(json_t *o, const char *k, const char *v) {
    if (!o || !k) return;
    json_set(o, k, v ? json_string(v) : json_null());
}

static json_t *obj(json_t *parent, const char *key) {
    json_t *v = parent ? json_obj_get(parent, key) : NULL;
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

static int beat_index(const char *id) {
    if (!id) return -1;
    for (int i = 0; i < BEAT_COUNT; i++)
        if (strcmp(BEATS[i].id, id) == 0) return i;
    return -1;
}

static bool picks_has(const beat_t *b, const char *key) {
    for (int i = 0; b->picks[i]; i++)
        if (strcmp(b->picks[i], key) == 0) return true;
    return false;
}

static bool facts_plugins_present(const json_t *facts) {
    const json_t *p = facts ? json_obj_get(facts, "plugins") : NULL;
    return p && p->type == JSON_ARRAY && json_len((json_t *)p) > 0;
}

static bool facts_tours(const json_t *facts) {
    if (!facts) return true;
    const json_t *t = json_obj_get(facts, "tours");
    if (!t) return true;
    return t->type == JSON_BOOL ? t->bool_val : true;
}

static bool beat_when(const beat_t *b, const json_t *facts, const json_t *picks) {
    (void)picks;
    if (b->when == 1) return facts_plugins_present(facts);
    if (b->when == 2) return facts_tours(facts);
    return true;
}

static const char *next_beat(const json_t *record) {
    const json_t *beats = record ? json_obj_get(record, "beats") : NULL;
    if (!beats) return NULL;
    for (int i = 0; ONBOARDING_FLOW_BEAT_IDS[i]; i++) {
        const char *st = json_get_str(beats, ONBOARDING_FLOW_BEAT_IDS[i], PENDING);
        if (strcmp(st, PENDING) == 0 || strcmp(st, SHOWN) == 0) return ONBOARDING_FLOW_BEAT_IDS[i];
    }
    return NULL;
}

const char *onboarding_flow_next_beat(const json_t *record) { return next_beat(record); }

static const char *next_field(const json_t *record) {
    const char *n = next_beat(record);
    return n ? n : "done";
}

static bool is_active(const json_t *record) {
    if (!record || record->type != JSON_OBJECT) return false;
    const char *phase = json_get_str(record, "phase", "");
    return strcmp(phase, PHASE_ONBOARDING) == 0;
}

static void set_beat(json_t *record, const char *beat, const char *state) {
    json_t *beats = obj(record, "beats");
    if (beats && beat_index(beat) >= 0) set_str(beats, beat, state);
}

static const char *get_beat(const json_t *record, const char *beat) {
    const json_t *beats = record ? json_obj_get(record, "beats") : NULL;
    return beats ? json_get_str(beats, beat, PENDING) : PENDING;
}

/* record.picks.update({k: v for k in patch if k in beat.picks}) */
static void settle(json_t *record, const char *beat_id, const json_t *patch) {
    int idx = beat_index(beat_id);
    if (idx < 0) return;
    const beat_t *b = &BEATS[idx];
    set_beat(record, beat_id, DONE);
    json_t *picks = obj(record, "picks");
    if (!picks || !patch || patch->type != JSON_OBJECT) return;
    for (size_t i = 0; i < json_object_size(patch); i++) {
        const char *k = json_object_get_key_at(patch, i);
        if (!k || !picks_has(b, k)) continue;
        json_t *v = json_object_get_at((json_t *)patch, i);
        json_set(picks, k, v ? json_copy(v) : json_null());
    }
}

/* Beat.after: "<beat>_<pick>" for tour/build when a pick exists, else "after_<beat>". */
static char *after_key(const beat_t *b, const json_t *picks) {
    char buf[128];
    if ((strcmp(b->id, "tour") == 0 || strcmp(b->id, "build") == 0) && picks) {
        const char *pick = json_get_str(picks, b->id, "");
        if (pick && *pick) {
            snprintf(buf, sizeof buf, "%s_%s", b->id, pick);
            return strdup(buf);
        }
    }
    snprintf(buf, sizeof buf, "after_%s", b->id);
    return strdup(buf);
}

static bool array_has_str(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < json_len((json_t *)arr); i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && strcmp(v->str_val ? v->str_val : "", s) == 0) return true;
    }
    return false;
}

/* ── pick validation (pick_error) ──────────────────────────────────────────────────────────── */

static char *pick_error(const json_t *options, const json_t *eligible) {
    char problems[1024];
    problems[0] = '\0';
    bool bad = false;

    if (!options || options->type != JSON_ARRAY || json_len((json_t *)options) == 0) {
        snprintf(problems, sizeof problems, "options is required");
        bad = true;
    } else if (json_len((json_t *)options) > MAX_PICKS) {
        snprintf(problems, sizeof problems, "pick at most %d", MAX_PICKS);
        bad = true;
    } else {
        char seen[8][64];
        int seen_n = 0;
        for (size_t i = 0; i < json_len((json_t *)options); i++) {
            const json_t *o = json_get(options, i);
            const char *id = (o && o->type == JSON_OBJECT) ? json_get_str(o, "id", NULL) : NULL;
            char one[256] = "";
            if (!id) snprintf(one, sizeof one, "each option is {\"id\": ..., \"label\": ...}");
            else if (strcmp(id, "hermes") == 0)
                snprintf(one, sizeof one, "hermes is added last by itself; do not pick it");
            else if (!array_has_str(eligible, id))
                snprintf(one, sizeof one, "'%s' is not an eligible id", id);
            else {
                bool dup = false;
                for (int s = 0; s < seen_n; s++)
                    if (strcmp(seen[s], id) == 0) dup = true;
                if (dup) snprintf(one, sizeof one, "'%s' is picked twice", id);
            }
            if (one[0]) {
                if (bad) strncat(problems, "; ", sizeof problems - strlen(problems) - 1);
                strncat(problems, one, sizeof problems - strlen(problems) - 1);
                bad = true;
                continue;
            }
            const char *label = json_get_str(o, "label", NULL);
            if (!label || !*label || strspn(label, " \t") == strlen(label)) {
                if (bad) strncat(problems, "; ", sizeof problems - strlen(problems) - 1);
                char t[160];
                snprintf(t, sizeof t, "the label for %s is empty", id);
                strncat(problems, t, sizeof problems - strlen(problems) - 1);
                bad = true;
            } else if (strlen(label) > MAX_LABEL) {
                if (bad) strncat(problems, "; ", sizeof problems - strlen(problems) - 1);
                char t[200];
                snprintf(t, sizeof t, "\"%s\" is %zu characters; the limit is %d. Shorten it",
                         label, strlen(label), MAX_LABEL);
                strncat(problems, t, sizeof problems - strlen(problems) - 1);
                bad = true;
            } else if (seen_n < 8) {
                snprintf(seen[seen_n++], 64, "%s", id);
            }
        }
    }
    if (!bad) return NULL;

    /* Named eligible ids, joined. */
    char ids[512] = "";
    if (eligible && eligible->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len((json_t *)eligible); i++) {
            const char *e = json_get_str((json_t *)eligible, NULL, NULL);
            (void)e;
            const json_t *v = json_get(eligible, i);
            if (v && v->type == JSON_STRING && v->str_val) {
                if (ids[0]) strncat(ids, ", ", sizeof ids - strlen(ids) - 1);
                strncat(ids, v->str_val, sizeof ids - strlen(ids) - 1);
            }
        }
    }
    char *out = malloc(1600);
    if (!out) return NULL;
    snprintf(out, 1600,
             "%s. Call setup_beat(beat=\"build\", options=[{\"id\": ..., \"label\": ...}]) with up to "
             "%d of these ids: %s.", problems, MAX_PICKS, ids);
    return out;
}

/* ── valid_answer ──────────────────────────────────────────────────────────────────────────── */

static bool is_names(const json_t *v) {
    if (!v || v->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len((json_t *)v); i++) {
        const json_t *e = json_get(v, i);
        if (!e || e->type != JSON_STRING) return false;
    }
    return true;
}

static bool in_ids(const char *const *ids, const char *v) {
    if (!ids || !v) return false;
    for (int i = 0; ids[i]; i++)
        if (strcmp(ids[i], v) == 0) return true;
    return false;
}

static bool values_accept(const char *beat_id, const json_t *answer, const json_t *facts,
                          const json_t *picks, const json_t *eligible) {
    (void)facts;
    const char *v = NULL;
    if (strcmp(beat_id, "layout") == 0) {
        v = json_get_str(answer, "layout", NULL);
        return v && in_ids(LAYOUT_IDS, v);
    }
    if (strcmp(beat_id, "tour") == 0) {
        v = json_get_str(answer, "tour", NULL);
        return v && in_ids(TOUR_IDS, v);
    }
    if (strcmp(beat_id, "build") == 0) {
        v = json_get_str(answer, "build", NULL);
        if (!v) return false;
        if (strcmp(v, "hermes") == 0 || strcmp(v, "own") == 0) return true;
        if (array_has_str(eligible, v)) return true;
        const json_t *opts = picks ? json_obj_get(picks, "build_options") : NULL;
        if (opts && opts->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len((json_t *)opts); i++) {
                const json_t *o = json_get(opts, i);
                const char *id = (o && o->type == JSON_OBJECT) ? json_get_str(o, "id", NULL) : NULL;
                if (id && strcmp(id, v) == 0) return true;
            }
        }
        return false;
    }
    return false;
}

static bool valid_answer(const char *beat_id, const json_t *answer, const json_t *facts,
                         const json_t *picks, const json_t *eligible) {
    int idx = beat_index(beat_id);
    if (idx < 0 || !answer || answer->type != JSON_OBJECT) return false;
    const beat_t *b = &BEATS[idx];

    const json_t *label = json_obj_get(answer, "label");
    if (label && label->type != JSON_STRING) return false;

    /* Every key must be one of the beat's picks, and there must be at least one. */
    size_t keys = 0;
    for (size_t i = 0; i < json_object_size(answer); i++) {
        const char *k = json_object_get_key_at(answer, i);
        if (!k || strcmp(k, "label") == 0) continue;
        if (!picks_has(b, k)) return false;
        keys++;
    }
    if (keys == 0) return false;

    if (b->values || strcmp(beat_id, "build") == 0)
        return values_accept(beat_id, answer, facts, picks, eligible);

    if (strcmp(beat_id, "name") == 0) {
        const char *n = json_get_str(answer, "name", NULL);
        return n && *n && strspn(n, " \t") != strlen(n);
    }
    if (strcmp(beat_id, "theme") == 0) {
        for (size_t i = 0; i < json_object_size(answer); i++) {
            const char *k = json_object_get_key_at(answer, i);
            if (!k || strcmp(k, "label") == 0) continue;
            const json_t *v = json_object_get_at((json_t *)answer, i);
            if (!v || v->type == JSON_NULL) continue;
            if (v->type != JSON_STRING) return false;
        }
        return true;
    }
    if (strcmp(beat_id, "plugins") == 0) {
        const json_t *inst = json_obj_get(answer, "installed");
        if (!is_names(inst)) return false;
        const json_t *facts_plugins = facts ? json_obj_get(facts, "plugins") : NULL;
        for (size_t i = 0; i < json_len((json_t *)inst); i++) {
            const json_t *v = json_get(inst, i);
            if (!v || v->type != JSON_STRING) return false;
            bool ok = false;
            if (facts_plugins && facts_plugins->type == JSON_ARRAY) {
                for (size_t j = 0; j < json_len((json_t *)facts_plugins); j++) {
                    const json_t *p = json_get(facts_plugins, j);
                    if (p && p->type == JSON_STRING && p->str_val &&
                        strcmp(p->str_val, v->str_val ? v->str_val : "") == 0) { ok = true; break; }
                }
            }
            if (!ok) return false;
        }
        return true;
    }
    /* connectors: "connected" present and every value a list of names. */
    if (!json_has(answer, "connected")) return false;
    for (size_t i = 0; i < json_object_size(answer); i++) {
        const char *k = json_object_get_key_at(answer, i);
        if (!k || strcmp(k, "label") == 0) continue;
        const json_t *v = json_object_get_at((json_t *)answer, i);
        if (!is_names(v)) return false;
    }
    return true;
}

/* ── present / note_present ────────────────────────────────────────────────────────────────── */

static json_t *note_present(json_t *record, const char *beat_id, const json_t *present) {
    json_t *out = json_object();
    json_t *picks = obj(record, "picks");
    int idx = beat_index(beat_id);
    if (idx >= 0 && present && present->type == JSON_OBJECT) {
        const beat_t *b = &BEATS[idx];
        for (size_t i = 0; i < json_object_size(present); i++) {
            const char *k = json_object_get_key_at(present, i);
            if (!k) continue;
            if (!picks_has(b, k)) continue;
            json_t *v = json_object_get_at((json_t *)present, i);
            json_set(out, k, v ? json_copy(v) : json_null());
            if (picks) json_set(picks, k, v ? json_copy(v) : json_null());
        }
    }
    return out;
}

/* ── handlers ──────────────────────────────────────────────────────────────────────────────── */

typedef struct {
    json_t *record;
    json_t *result;
    char *reminder;    /* owned or NULL */
} step_t;

static step_t step_new(json_t *record) {
    step_t s = { record, json_object(), NULL };
    return s;
}

static step_t h_shown(json_t *record, const json_t *event, const json_t *facts,
                      const json_t *eligible, double now) {
    const char *beat = json_get_str(event, "beat", "");
    int idx = beat_index(beat);
    step_t s = step_new(record);
    if (idx < 0) { set_str(s.result, "error", "unknown beat"); return s; }
    const beat_t *b = &BEATS[idx];
    json_t *picks = obj(record, "picks");

    if (!beat_when(b, facts, picks)) {
        set_beat(record, beat, SKIPPED);
        set_str(s.result, "beat", beat);
        json_set(s.result, "skipped", json_bool(true));
        set_str(s.result, "next", next_field(record));
        s.reminder = strdup("moved_on");
        return s;
    }
    if (strcmp(beat, "build") == 0) {
        const json_t *options = json_obj_get(event, "options");
        bool have_stored = false;
        if (picks) {
            const json_t *bo = json_obj_get(picks, "build_options");
            have_stored = bo && bo->type == JSON_ARRAY && json_len((json_t *)bo) > 0;
        }
        bool empty = !options || options->type != JSON_ARRAY || json_len((json_t *)options) == 0;
        if (!(empty && have_stored)) {
            char *err = pick_error(options, eligible);
            if (err) {
                set_str(s.result, "error", err);
                free(err);
                return s;
            }
            if (!empty && picks) {
                json_t *stored = json_array();
                for (size_t i = 0; i < json_len((json_t *)options); i++) {
                    const json_t *o = json_get(options, i);
                    const char *id = json_get_str(o, "id", NULL);
                    const char *label = json_get_str(o, "label", "");
                    char trimmed[256];
                    snprintf(trimmed, sizeof trimmed, "%s", label);
                    size_t L = strlen(trimmed);
                    while (L > 0 && (trimmed[L-1] == ' ' || trimmed[L-1] == '\t')) trimmed[--L] = '\0';
                    json_t *e = json_object();
                    set_str(e, "id", id);
                    set_str(e, "label", trimmed);
                    json_append(stored, e);
                }
                json_set(picks, "build_options", stored);
            }
        }
    }
    if (strcmp(beat, "connectors") == 0) {
        const char *look = json_get_str(record, "look_around", "not_offered");
        if (strcmp(look, "not_offered") == 0) {
            set_str(record, "look_around", "offered");
            const json_t *suggest = json_obj_get(event, "suggest");
            bool has = false;
            if (suggest && suggest->type == JSON_ARRAY) {
                for (size_t i = 0; i < json_len((json_t *)suggest); i++) {
                    const json_t *v = json_get(suggest, i);
                    const char *sl = (v && v->type == JSON_STRING) ? v->str_val : NULL;
                    if (sl && strspn(sl, " \t") != strlen(sl)) { has = true; break; }
                }
            }
            if (!has) {
                set_str(s.result, "error", "offer first");
                s.reminder = strdup("offer_first");
                return s;
            }
        }
    }
    for (int i = 0; i < idx; i++) {
        if (strcmp(get_beat(record, BEATS[i].id), SHOWN) == 0) set_beat(record, BEATS[i].id, DONE);
    }
    if (strcmp(get_beat(record, beat), DONE) != 0) set_beat(record, beat, SHOWN);
    json_t *waiting = json_object();
    set_str(waiting, "beat", beat);
    set_str(waiting, "tool_call_id", json_get_str(event, "tool_call_id", ""));
    json_set(waiting, "since", json_number(now));
    json_set(record, "waiting", waiting);
    return s;
}

static step_t h_answered(json_t *record, const json_t *event, const json_t *facts,
                         const json_t *eligible) {
    const char *beat = json_get_str(event, "beat", "");
    int idx = beat_index(beat);
    step_t s = step_new(record);
    if (idx < 0) { set_str(s.result, "error", "unknown beat"); return s; }
    json_set(record, "waiting", json_null());

    const json_t *answer = json_obj_get(event, "answer");
    if (!valid_answer(beat, answer, facts, obj(record, "picks"), eligible)) {
        char msg[256];
        snprintf(msg, sizeof msg,
                 "The card sent an answer this beat does not accept. Bring it back with "
                 "setup_beat(beat=\"%s\").", beat);
        set_str(s.result, "beat", beat);
        set_str(s.result, "error", msg);
        return s;
    }
    settle(record, beat, answer);
    set_str(s.result, "beat", beat);
    json_t *ans = json_object();
    for (size_t i = 0; i < json_object_size(answer); i++) {
        const char *k = json_object_get_key_at(answer, i);
        if (!k) continue;
        json_t *v = json_object_get_at((json_t *)answer, i);
        json_set(ans, k, v ? json_copy(v) : json_null());
    }
    json_set(s.result, "answer", ans);
    const char *said = json_get_str(event, "said", "");
    if (said && *said) set_str(s.result, "said", said);
    set_str(s.result, "next", next_field(record));
    s.reminder = after_key(&BEATS[idx], obj(record, "picks"));
    return s;
}

static step_t h_note(json_t *record, const json_t *event, const char *kind,
                     const char *reminder_key) {
    const char *beat = json_get_str(event, "beat", "");
    step_t s = step_new(record);
    json_set(record, "waiting", json_null());
    set_str(s.result, "beat", beat);
    json_set(s.result, kind, json_bool(true));
    json_t *present = note_present(record, beat, json_obj_get(event, "present"));
    for (size_t i = 0; i < json_object_size(present); i++) {
        const char *k = json_object_get_key_at(present, i);
        json_t *v = json_object_get_at(present, i);
        json_set(s.result, k, v ? json_copy(v) : json_null());
    }
    json_free(present);
    if (reminder_key) s.reminder = strdup(reminder_key);
    return s;
}

static step_t h_recorded(json_t *record, const json_t *event, const json_t *facts,
                         const json_t *eligible) {
    const char *beat_id = json_get_str(event, "beat", "");
    int idx = beat_index(beat_id);
    step_t s = step_new(record);
    if (idx < 0) { set_str(s.result, "error", "unknown beat"); return s; }
    const beat_t *b = &BEATS[idx];

    if (strcmp(beat_id, "theme") == 0 || strcmp(beat_id, "layout") == 0) {
        char msg[160];
        snprintf(msg, sizeof msg, "Apply the %s with set_theme; its result settles this beat.", beat_id);
        set_str(s.result, "error", msg);
        set_str(s.result, "next", next_field(record));
        return s;
    }
    const char *value = json_get_str(event, "value", "");
    if (b->values) {
        bool ok = false;
        for (int i = 0; b->values[i]; i++) if (strcmp(b->values[i], value) == 0) ok = true;
        if (strcmp(beat_id, "build") == 0) {
            ok = strcmp(value, "hermes") == 0 || strcmp(value, "own") == 0 || array_has_str(eligible, value);
            const json_t *opts = obj(record, "picks") ? json_obj_get(obj(record, "picks"), "build_options") : NULL;
            if (!ok && opts && opts->type == JSON_ARRAY) {
                for (size_t i = 0; i < json_len((json_t *)opts); i++) {
                    const char *id = json_get_str(json_get(opts, i), "id", NULL);
                    if (id && strcmp(id, value) == 0) ok = true;
                }
            }
        }
        if (!ok) {
            char list[256] = "";
            if (b->values)
                for (int i = 0; b->values[i]; i++) {
                    if (list[0]) strncat(list, ", ", sizeof list - strlen(list) - 1);
                    strncat(list, b->values[i], sizeof list - strlen(list) - 1);
                }
            char msg[400];
            if (strcmp(beat_id, "build") == 0)
                snprintf(msg, sizeof msg, "value for build must be one of the card's ids, hermes or own.");
            else
                snprintf(msg, sizeof msg, "value for %s must be one of: %s.", beat_id, list);
            set_str(s.result, "error", msg);
            set_str(s.result, "next", next_field(record));
            return s;
        }
    }
    json_t *patch = json_object();
    if (b->value) set_str(patch, b->value, value);
    settle(record, beat_id, patch);
    json_free(patch);
    set_str(s.result, "recorded", beat_id);
    json_t *present = note_present(record, beat_id, json_obj_get(event, "present"));
    for (size_t i = 0; i < json_object_size(present); i++) {
        const char *k = json_object_get_key_at(present, i);
        json_t *v = json_object_get_at(present, i);
        json_set(s.result, k, v ? json_copy(v) : json_null());
    }
    json_free(present);
    set_str(s.result, "next", next_field(record));
    s.reminder = after_key(b, obj(record, "picks"));
    return s;
}

static step_t h_skipped(json_t *record, const json_t *event) {
    const char *beat = json_get_str(event, "beat", "");
    step_t s = step_new(record);
    set_beat(record, beat, SKIPPED);
    set_str(s.result, "recorded", beat);
    json_set(s.result, "skipped", json_bool(true));
    json_t *present = note_present(record, beat, json_obj_get(event, "present"));
    for (size_t i = 0; i < json_object_size(present); i++) {
        const char *k = json_object_get_key_at(present, i);
        json_t *v = json_object_get_at(present, i);
        json_set(s.result, k, v ? json_copy(v) : json_null());
    }
    json_free(present);
    set_str(s.result, "next", next_field(record));
    s.reminder = strdup("moved_on");
    return s;
}

static step_t h_end(json_t *record, const json_t *event) {
    step_t s = step_new(record);
    const char *outcome = json_get_str(event, "outcome", "skipped");
    set_str(record, "phase", PHASE_SKIPPED);
    set_str(record, "ended_by", outcome);
    json_set(record, "waiting", json_null());
    for (int i = 0; ONBOARDING_FLOW_BEAT_IDS[i]; i++)
        if (strcmp(get_beat(record, ONBOARDING_FLOW_BEAT_IDS[i]), DONE) != 0)
            set_beat(record, ONBOARDING_FLOW_BEAT_IDS[i], SKIPPED);
    json_set(s.result, "skipped_all", json_bool(true));
    s.reminder = strdup("skip_all");
    return s;
}

static step_t h_done(json_t *record) {
    step_t s = step_new(record);
    if (strcmp(get_beat(record, "build"), PENDING) == 0) {
        set_str(s.result, "error",
                "Onboarding ends after the first build is shown and its close card is answered. "
                "If they want to stop, tell them about the Skip setup button.");
        set_str(s.result, "next", next_field(record));
        return s;
    }
    set_beat(record, "build", DONE);
    s.reminder = strdup("complete");
    return s;
}

static step_t h_applied(json_t *record, const json_t *event) {
    step_t s = step_new(record);
    const char *tool = json_get_str(event, "tool", "");
    const json_t *args = json_obj_get(event, "args");
    const json_t *result = json_obj_get(event, "result");

    /* Collect the beats this result settles, in order. */
    const char *beats[3];
    json_t *patches[3];
    int n = 0;
    if (strcmp(tool, "set_theme") == 0 && result && result->type == JSON_OBJECT) {
        const json_t *values = json_obj_get(result, "applied");
        if (!values || values->type != JSON_OBJECT) values = json_obj_get(result, "picked");
        if (values && values->type == JSON_OBJECT) {
            json_t *theme = json_object();
            for (int i = 0; i < 2; i++) {
                const char *k = i == 0 ? "accent" : "mode";
                json_t *v = json_obj_get(values, k);
                if (v) json_set(theme, k, json_copy(v));
            }
            if (json_object_size(theme) > 0) { beats[n] = "theme"; patches[n++] = theme; }
            else json_free(theme);
            json_t *l = json_obj_get(values, "layout");
            if (l) {
                json_t *p = json_object();
                json_set(p, "layout", json_copy(l));
                beats[n] = "layout"; patches[n++] = p;
            }
        }
    } else if (strcmp(tool, "manage_connections") == 0 && args &&
               strcmp(json_get_str(args, "action", ""), "display") == 0 &&
               result && json_obj_get(result, "connected")) {
        json_t *p = json_object();
        const json_t *c = json_obj_get(result, "connected");
        const json_t *nc = json_obj_get(result, "newly_connected");
        json_set(p, "connected", (c && c->type == JSON_ARRAY) ? json_copy(c) : json_array());
        json_set(p, "newly_connected", (nc && nc->type == JSON_ARRAY) ? json_copy(nc) : json_array());
        beats[n] = "connectors"; patches[n++] = p;
    } else if (strcmp(tool, "gui_tour") == 0 && args &&
               strcmp(json_get_str(args, "action", ""), "start") == 0 &&
               (!result || !json_obj_get(result, "error"))) {
        json_t *p = json_object();
        const char *preset = json_get_str(args, "preset", "");
        set_str(p, "tour", (preset && *preset) ? preset : "full");
        beats[n] = "tour"; patches[n++] = p;
    }

    const char *settled = NULL, *repeated = NULL;
    for (int i = 0; i < n; i++) {
        bool was_done = strcmp(get_beat(record, beats[i]), DONE) == 0;
        if (!was_done) settled = beats[i];
        else if (strcmp(beats[i], "theme") == 0 || strcmp(beats[i], "layout") == 0) repeated = beats[i];
        settle(record, beats[i], patches[i]);
        json_t *waiting = obj(record, "waiting");
        if (waiting && strcmp(json_get_str(waiting, "beat", ""), beats[i]) == 0)
            json_set(record, "waiting", json_null());
    }
    for (int i = 0; i < n; i++) json_free(patches[i]);

    if (settled) {
        char buf[96];
        snprintf(buf, sizeof buf, "after_%s", settled);
        s.reminder = strdup(buf);
    } else if (repeated) {
        char buf[96];
        snprintf(buf, sizeof buf, "already_%s", repeated);
        s.reminder = strdup(buf);
    }
    return s;
}

static step_t h_ended(json_t *record, const json_t *event) {
    step_t s = step_new(record);
    const char *type = json_get_str(event, "type", "");
    if (strcmp(type, "applied") == 0) return s;
    if (strcmp(type, "done") == 0 && strcmp(json_get_str(record, "phase", ""), PHASE_COMPLETE) == 0) {
        json_set(s.result, "completed", json_bool(true));
        s.reminder = strdup("complete");
        return s;
    }
    if (strcmp(type, "end") == 0) {
        json_set(s.result, "skipped_all", json_bool(true));
        return s;
    }
    set_str(s.result, "error", "Onboarding is over in this chat; it is a normal chat now.");
    return s;
}

/* ── apply ─────────────────────────────────────────────────────────────────────────────────── */

json_t *onboarding_flow_apply(const json_t *record, const json_t *event, const json_t *facts,
                              const json_t *eligible_ids, double now) {
    if (!event || event->type != JSON_OBJECT) return NULL;
    const char *type = json_get_str(event, "type", "");
    if (!*type) return NULL;

    json_t *working = record ? json_copy(record) : json_object();
    if (!working) return NULL;

    step_t step;
    if (!is_active(working)) {
        step = h_ended(working, event);
    } else if (strcmp(type, "shown") == 0) {
        step = h_shown(working, event, facts, eligible_ids, now);
    } else if (strcmp(type, "answered") == 0) {
        step = h_answered(working, event, facts, eligible_ids);
    } else if (strcmp(type, "wrote") == 0) {
        step = h_note(working, event, "wrote_instead", "wrote");
    } else if (strcmp(type, "timed_out") == 0) {
        step = h_note(working, event, "timed_out", "timed_out");
    } else if (strcmp(type, "recorded") == 0) {
        step = h_recorded(working, event, facts, eligible_ids);
    } else if (strcmp(type, "skipped") == 0) {
        step = h_skipped(working, event);
    } else if (strcmp(type, "end") == 0) {
        step = h_end(working, event);
    } else if (strcmp(type, "done") == 0) {
        step = h_done(working);
    } else if (strcmp(type, "applied") == 0) {
        step = h_applied(working, event);
    } else {
        json_free(working);
        return NULL;
    }

    /* The run completes when the build beat settles. */
    if (is_active(step.record) && strcmp(get_beat(step.record, "build"), DONE) == 0) {
        set_str(step.record, "phase", PHASE_COMPLETE);
        set_str(step.record, "ended_by", "done");
        json_set(step.record, "waiting", json_null());
        json_set(step.result, "completed", json_bool(true));
    }
    if (record) {
        char *a = json_serialize(record);
        char *b = json_serialize(step.record);
        bool same = a && b && strcmp(a, b) == 0;
        free(a);
        free(b);
        if (!same) json_set(step.record, "updated_at", json_number(now));
    }

    json_t *out = json_object();
    json_set(out, "record", step.record);
    json_set(out, "result", step.result);
    set_str(out, "reminder", step.reminder ? step.reminder : "");
    free(step.reminder);
    return out;
}
