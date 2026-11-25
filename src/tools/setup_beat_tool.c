/*
 * setup_beat_tool.c — C home for upstream Hermes tools/setup_beat_tool.py.
 *
 * PoP: w8_setupbeatt_cca545_suggest_from @ tools/setup_beat_tool.py:_suggest_from
 * PoP: w8_setupbeatt_cca545_unlisted_suggestions @ tools/setup_beat_tool.py:_unlisted_suggestions
 */
#include "noc_w8_setupbeatt_cca545.h"
#include "hermes_json.h"
#include "hermes_core_types.h"
#include "hermes_state_db.h"
#include "onboarding_flow_apply.h"
#include "onboarding_reminders.h"
#include "onboarding_task_catalog.h"
#include "noc_w7_onboardingt_01579d.h"
#include "registry.h"
#include "hermes_logger.h"
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *strip_ws(const char *s) {
    if (!s) return NULL;
    const unsigned char *a = (const unsigned char *)s;
    while (*a && is_ws(*a)) a++;
    const unsigned char *b = a + strlen((const char *)a);
    while (b > a && is_ws(b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, a, n); out[n] = '\0'; }
    return out;
}

static int cmp_strptr(const void *a, const void *b) {
    const char *x = *(const char *const *)a, *y = *(const char *const *)b;
    return strcmp(x ? x : "", y ? y : "");
}

/* Python sorted(slugs) then ", ".join(...), as the card suffix. */
char *w8_setupbeatt_cca545_suggest_from(const struct json_t *slugs) {
    if (!slugs || slugs->type != JSON_ARRAY || !json_len(slugs)) return (char *)strdup("");
    size_t n = json_len(slugs);
    const char **items = (const char **)calloc(n, sizeof(const char *));
    if (!items) return (char *)strdup("");
    size_t k = 0;
    for (size_t i = 0; i < n; ++i) {
        struct json_t *v = json_get(slugs, i);
        if (v && v->type == JSON_STRING) items[k++] = v->str_val;
    }
    qsort(items, k, sizeof(const char *), cmp_strptr);
    size_t cap = 128;
    for (size_t i = 0; i < k; ++i) cap += strlen(items[i]) + 2;
    char *out = (char *)malloc(cap);
    if (out) {
        size_t at = (size_t)sprintf(out, " Suggest only connectors from this list, by these slugs: ");
        for (size_t i = 0; i < k; ++i) {
            if (i) at += (size_t)sprintf(out + at, ", ");
            at += (size_t)sprintf(out + at, "%s", items[i]);
        }
        sprintf(out + at, ".");
    }
    free(items);
    return out;
}

/* str(slug) for the JSON scalar kinds used here (string/number/bool/null). */
static char *py_str(const struct json_t *v) {
    if (!v) return (char *)strdup("None");
    switch (v->type) {
        case JSON_STRING: return (char *)strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL: return (char *)strdup(v->bool_val ? "True" : "False");
        case JSON_NULL: return (char *)strdup("None");
        case JSON_NUMBER: {
            char buf[64];
            if (v->num_val == (double)(long long)v->num_val)
                sprintf(buf, "%lld", (long long)v->num_val);
            else
                sprintf(buf, "%g", v->num_val);
            return (char *)strdup(buf);
        }
        default: return (char *)strdup("");
    }
}

static int in_slugs(const struct json_t *slugs, const char *name) {
    if (!slugs || slugs->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(slugs); ++i) {
        struct json_t *v = json_get(slugs, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, name)) return 1;
    }
    return 0;
}

char *w8_setupbeatt_cca545_unlisted_suggestions(const struct json_t *args,
        const struct json_t *slugs) {
    if (!args || args->type != JSON_OBJECT) return NULL;
    struct json_t *suggest = json_obj_get(args, "suggest");
    int is_arr = suggest && suggest->type == JSON_ARRAY && json_len(suggest);
    int is_str = suggest && suggest->type == JSON_STRING && suggest->str_val && *suggest->str_val;
    if (!is_arr && !is_str) return NULL;   /* empty/falsy suggest -> () -> None */
    size_t count = is_str ? strlen(suggest->str_val) : json_len(suggest);
    const char **wanted = (const char **)calloc(count ? count : 1, sizeof(const char *));
    size_t nw = 0;
    if (wanted) {
        for (size_t i = 0; i < count; ++i) {
            char *raw;
            if (is_str) {
                raw = (char *)malloc(2);
                if (raw) { raw[0] = suggest->str_val[i]; raw[1] = '\0'; }
            } else {
                raw = py_str(json_get(suggest, i));
            }
            char *t = strip_ws(raw ? raw : "");
            if (t && *t) {
                char *lower = (char *)malloc(strlen(t) + 1);
                if (lower) {
                    size_t j;
                    for (j = 0; t[j]; ++j) lower[j] = (char)tolower((unsigned char)t[j]);
                    lower[j] = '\0';
                    wanted[nw++] = lower;
                }
            }
            free(t); free(raw);
        }
    }
    size_t nunlisted = 0;
    for (size_t i = 0; i < nw; ++i)
        if (!in_slugs(slugs, wanted[i])) nunlisted++;
    if (!nunlisted || !slugs || slugs->type != JSON_ARRAY) {
        for (size_t i = 0; i < nw; ++i) free((void *)wanted[i]);
        free(wanted);
        return NULL;
    }
    char *suffix = w8_setupbeatt_cca545_suggest_from(slugs);
    size_t cap = 512;
    for (size_t i = 0; i < nw; ++i) cap += strlen(wanted[i]) + 4;
    cap += suffix ? strlen(suffix) : 0;
    char *joined = (char *)malloc(cap);
    if (!joined) { free(suffix); for (size_t i = 0; i < nw; ++i) free((void *)wanted[i]); free(wanted); return NULL; }
    size_t at = 0;
    for (size_t i = 0; i < nw; ++i) {
        if (!in_slugs(slugs, wanted[i])) {
            if (at) at += (size_t)sprintf(joined + at, ", ");
            at += (size_t)sprintf(joined + at, "%s", wanted[i]);
        }
    }
    char *out = (char *)malloc(cap + 256);
    if (out) {
        sprintf(out,
            "{\"error\": \"not connectors here\", \"reminder\": "
            "\"No card shown: %s cannot be connected here. Do not mention them as connections."
            "%s Then call setup_beat(beat=\\\"connectors\\\", suggest=[...]) again, "
            "or without suggest when none fits.\"}",
            joined, suffix ? suffix : "");
    }
    free(joined); free(suffix);
    for (size_t i = 0; i < nw; ++i) free((void *)wanted[i]);
    free(wanted);
    return out;
}


/* ==================================================================
 *  The tool itself: registry_init_setup_beat
 *
 *  Port of tools/setup_beat_tool.py:setup_beat_tool. The direct events -- done, skip, and a value
 *  given in words -- need no desktop, so they run the whole chain: load the run record from the
 *  session's model_config (agent/onboarding_record.py:KEY), apply the event, save the record, render
 *  the reminder. Everything else is a card, and cards need the desktop; with no callback upstream
 *  answers exactly the string used here.
 * ================================================================== */

#define ONB_KEY "onboarding_run"

static const char *const ONB_BEATS[] = {
    "next", "name", "theme", "layout", "connectors", "plugins", "tour", "build", "done", NULL
};

static char *onb_error(const char *msg) {
    json_t *o = json_object();
    json_set(o, "error", json_string(msg ? msg : "error"));
    char *t = json_serialize(o);
    json_free(o);
    return t;
}

static void onb_copy_into(json_t *dst, const json_t *src) {
    if (!dst || !src || src->type != JSON_OBJECT) return;
    for (size_t i = 0; i < json_object_size(src); i++) {
        const char *k = json_object_get_key_at(src, i);
        json_t *v = json_object_get_at((json_t *)src, i);
        if (k) json_set(dst, k, v ? json_copy(v) : json_null());
    }
}

static bool onb_beat_known(const char *beat) {
    if (!beat) return false;
    for (int i = 0; ONB_BEATS[i]; i++) if (strcmp(ONB_BEATS[i], beat) == 0) return true;
    return false;
}

/* The event these args carry in themselves, or NULL when the beat needs a card. */
static json_t *onb_direct_event(const char *beat, const json_t *args) {
    if (!beat || !args) return NULL;
    if (strcmp(beat, "done") == 0) {
        json_t *e = json_object();
        json_set(e, "type", json_string("done"));
        return e;
    }
    const json_t *skip = json_obj_get(args, "skip");
    if (skip && skip->type == JSON_BOOL && skip->bool_val) {
        json_t *e = json_object();
        json_set(e, "type", json_string("skipped"));
        json_set(e, "beat", json_string(beat));
        json_set(e, "present", json_object());
        return e;
    }
    const json_t *val = json_obj_get(args, "value");
    if (val && val->type == JSON_STRING) {
        char *trimmed = strip_ws(val->str_val ? val->str_val : "");
        if (trimmed && *trimmed) {
            json_t *e = json_object();
            json_set(e, "type", json_string("recorded"));
            json_set(e, "beat", json_string(beat));
            json_set(e, "value", json_string(trimmed));
            json_set(e, "present", json_object());
            free(trimmed);
            return e;
        }
        free(trimmed);
    }
    return NULL;
}

static hermes_state_db_t *onb_open_db(void) {
    char home[1024], path[1200];
    hermes_get_home(home, sizeof home);
    if (!home[0]) return NULL;
    snprintf(path, sizeof path, "%s/state.db", home);
    return hermes_state_db_open(path);
}

/* records.advance's save: patch the session's model_config with the new record. */
static bool onb_save_record(hermes_state_db_t *db, const char *session_id, const json_t *record) {
    if (!db || !session_id || !*session_id || !record) return false;
    json_t *patch = json_object();
    json_set(patch, ONB_KEY, json_copy(record));
    char *text = json_serialize(patch);
    json_free(patch);
    if (!text) return false;
    bool ok = hermes_state_patch_session_model_config(db, session_id, text);
    free(text);
    return ok;
}

char *setup_beat_tool(const char *args_json, const char *session_id) {
    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *beat = (args && args->type == JSON_OBJECT) ? json_get_str(args, "beat", "") : "";
    if (!onb_beat_known(beat)) {
        json_free(args);
        return onb_error("beat must be one of: next, name, theme, layout, connectors, plugins, tour, "
                         "build, done.");
    }

    hermes_state_db_t *db = onb_open_db();
    if (!db) {
        json_free(args);
        return onb_error("could not open the state database");
    }

    json_t *record = NULL;
    if (session_id && *session_id) {
        char *raw = hermes_state_get_session_model_config_value(db, session_id, ONB_KEY, "null");
        if (raw) {
            record = json_parse(raw, NULL);
            free(raw);
        }
    }
    if (!record || record->type != JSON_OBJECT) {
        json_free(record);
        hermes_state_db_close(db);
        json_free(args);
        return onb_error("Onboarding is not running in this chat.");
    }

    const json_t *facts = json_obj_get(record, "facts");
    json_t *seam = onboarding_catalog_seam(facts, json_obj_get(record, "picks"));
    const json_t *rows = seam ? json_obj_get(seam, "eligible_rows") : NULL;

    json_t *event = (strcmp(beat, "next") != 0) ? onb_direct_event(beat, args) : NULL;
    if (event) {
        json_t *applied = onboarding_flow_apply(record, event, facts, rows, (double)time(NULL));
        json_free(event);
        if (!applied) {
            json_free(seam);
            json_free(record);
            hermes_state_db_close(db);
            json_free(args);
            return onb_error("could not apply the onboarding event");
        }
        const json_t *next_record = json_obj_get(applied, "record");
        onb_save_record(db, session_id, next_record);

        json_t *out = json_object();
        onb_copy_into(out, json_obj_get(applied, "result"));
        const char *key = json_get_str(applied, "reminder", "");
        char *rem = onboarding_reminder(key, next_record, facts,
                                        strcmp(beat, "done") == 0 ? NULL : beat, seam);
        if (rem && *rem) json_set(out, "reminder", json_string(rem));
        free(rem);

        char *ser = json_serialize(out);
        json_free(out);
        json_free(applied);
        json_free(seam);
        json_free(record);
        hermes_state_db_close(db);
        json_free(args);
        return ser;
    }

    if (strcmp(beat, "next") == 0) {
        bool active = strcmp(json_get_str(record, "phase", ""), "onboarding") == 0;
        const char *nb = active ? onboarding_flow_next_beat(record) : NULL;
        json_t *out;
        if (!active) {
            json_free(seam);
            json_free(record);
            hermes_state_db_close(db);
            json_free(args);
            return onb_error("Onboarding is over in this chat; it is a normal chat now.");
        }
        out = json_object();
        if (!nb) {
            json_set(out, "next", json_string("done"));
            char *fin = onboarding_finish_reminder(record, seam);
            if (fin) json_set(out, "reminder", json_string(fin));
            free(fin);
        } else {
            json_set(out, "beat", json_string(nb));
            const char *locale = facts ? json_get_str(facts, "locale", NULL) : NULL;
            json_t *q = onboardingt_question(nb, locale);
            if (q && q->type == JSON_STRING) json_set(out, "question", q);
            else json_free(q);
        }
        char *ser = json_serialize(out);
        json_free(out);
        json_free(seam);
        json_free(record);
        hermes_state_db_close(db);
        json_free(args);
        return ser;
    }

    /* Everything else is a card, and cards need the desktop. */
    json_free(seam);
    json_free(record);
    hermes_state_db_close(db);
    json_free(args);
    return onb_error("setup_beat cards show only in the Hermes desktop app.");
}

/* The registry dispatch has no session, so this wrapper passes the task id the agent loop knows. */
static char *handle_setup_beat(const char *args_json, const char *task_id) {
    return setup_beat_tool(args_json, task_id);
}

void registry_init_setup_beat(void) {
    registry_register("setup_beat",
        "Run the first-run onboarding in this chat. Shows the card for a beat and waits for the user, then returns their answer and what to do next. beat=next shows the next beat. Pass value to record an answer they gave in words, skip to record that they skipped a beat. The build card needs options: your picks from the list its reminder gives. beat=done ends onboarding after the first build is shown and its close card is answered. Nothing here ends it early: a user who wants to stop presses Skip setup.",
        "{\"type\":\"object\",\"properties\":{"
          "\"beat\":{\"type\":\"string\",\"enum\":[\"next\",\"name\",\"theme\",\"layout\",\"connectors\",\"plugins\",\"tour\",\"build\",\"done\"]},"
          "\"question\":{\"type\":\"string\",\"description\":\"Your line above the card.\"},"
          "\"value\":{\"type\":\"string\",\"description\":\"Record an answer given in words (a name, or an option id). No card.\"},"
          "\"skip\":{\"type\":\"boolean\",\"description\":\"They skipped this beat in words. No card.\"},"
          "\"suggest\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"description\":\"connectors only: slugs pinned as Suggested.\"},"
          "\"options\":{\"type\":\"array\",\"maxItems\":4,\"description\":\"build only: up to four picks, ids from the list.\","
            "\"items\":{\"type\":\"object\",\"required\":[\"id\",\"label\"],"
              "\"properties\":{\"id\":{\"type\":\"string\"},\"label\":{\"type\":\"string\",\"maxLength\":32}}}}},"
          "\"required\":[\"beat\"]}",
        handle_setup_beat);
}
