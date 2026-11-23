/*
 * sessions_discard.c - C home for upstream Hermes hermes_cli/sessions_discard.py.
 * PoP: w15_sessionsdi_6183f9_confirm_prompt @ hermes_cli/sessions_discard.py:_confirm
 * PoP: w15_sessionsdi_6183f9_discard_plan @ hermes_cli/sessions_discard.py:_discard
 *
 * Pure confirmation-copy and discard-plan logic: the gateway snapshot, TTY state and
 * confirmation answer are injected as JSON inputs, so no gateway or REPL is touched.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_sessionsdi_6183f9.h"

static char *dupstr(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* "Discard N lost turn(s) on session S? ...\n  AID\n..." — exactly what _confirm prints. */
struct json_t *w15_sessionsdi_6183f9_confirm_prompt(const char *session_id, const struct json_t *rows) {
    size_t n = (rows && rows->type == JSON_ARRAY) ? rows->c.count : 0;
    size_t cap = 512 + n * 128;
    char *out = (char *)malloc(cap);
    if (!out) return json_string("");
    size_t m = 0;
    m += (size_t)snprintf(out + m, cap - m,
        "Discard %zu lost turn(s) on session %s? They are acknowledged, not replayed.\n",
        n, session_id ? session_id : "");
    if (m >= cap) m = cap;
    for (size_t i = 0; i < n && m < cap; i++) {
        json_t *row = json_get(rows, i);
        json_t *aid = (row && row->type == JSON_OBJECT) ? json_obj_get(row, "admission_id") : NULL;
        m += (size_t)snprintf(out + m, cap - m, "  %s\n", jstr(aid));
        if (m > cap) m = cap;
    }
    out[m < cap ? m : cap] = 0;
    json_t *r = json_string(out);
    free(out);
    return r;
}

static int in_array(const struct json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < arr->c.count; i++) if (!strcmp(jstr(json_get(arr, i)), s)) return 1;
    return 0;
}

static void add_unique_sorted(json_t *arr, const char *s) {
    if (in_array(arr, s)) return;
    json_append(arr, json_string(s));
    /* insertion sort by strcmp (Python sorted() on ASCII strings) */
    for (size_t i = 1; i < arr->c.count; i++) {
        size_t j = i;
        while (j > 0 && strcmp(jstr(json_get(arr, j - 1)), jstr(json_get(arr, j))) > 0) {
            json_t *tmp = arr->c.items[j - 1];
            arr->c.items[j - 1] = arr->c.items[j];
            arr->c.items[j] = tmp;
            j--;
        }
    }
}

struct json_t *w15_sessionsdi_6183f9_discard_plan(const struct json_t *snapshot,
                                                  const struct json_t *admission,
                                                  int yes, int isatty, int confirm) {
    json_t *result = json_object();
    json_t *chosen = json_array();
    json_t *missing = json_array();
    json_t *lines = json_array();
    json_t *errors = json_array();
    json_set(result, "chosen", chosen);
    json_set(result, "missing", missing);
    json_set(result, "lines", lines);
    json_set(result, "stderr_lines", errors);
    json_set(result, "error", json_null());
    json_set(result, "code", json_number(0));

    const char *sid = "";
    if (snapshot && snapshot->type == JSON_OBJECT) {
        sid = jstr(json_obj_get(snapshot, "session_id"));
        if (!sid[0]) sid = jstr(json_obj_get(snapshot, "stored_session_id"));
    }
    json_t *pending = (snapshot && snapshot->type == JSON_OBJECT) ? json_obj_get(snapshot, "pending") : NULL;
    size_t n = (pending && pending->type == JSON_ARRAY) ? pending->c.count : 0;

    /* unknown = [row for row in pending if row.get("status") == "unknown"] */
    json_t *unknown = json_array();
    for (size_t i = 0; i < n; i++) {
        json_t *row = json_get(pending, i);
        if (!row || row->type != JSON_OBJECT) continue;
        json_t *st = json_obj_get(row, "status");
        if (st && st->type == JSON_STRING && !strcmp(st->str_val ? st->str_val : "", "unknown"))
            json_append(unknown, json_copy(row));
    }

    int use_admission = admission && admission->type == JSON_ARRAY && admission->c.count > 0;
    if (use_admission) {
        for (size_t i = 0; i < unknown->c.count; i++) {
            json_t *row = json_get(unknown, i);
            json_t *aid = json_obj_get(row, "admission_id");
            if (in_array(admission, jstr(aid))) json_append(chosen, json_copy(aid));
        }
        for (size_t i = 0; i < admission->c.count; i++) {
            const char *a = jstr(json_get(admission, i));
            if (!in_array(chosen, a)) add_unique_sorted(missing, a);
        }
        if (missing->c.count > 0) {
            char *joined = (char *)malloc(2048);
            if (joined) {
                size_t m = 0;
                joined[0] = 0;
                for (size_t i = 0; i < missing->c.count; i++) {
                    if (i) { memcpy(joined + m, ", ", 2); m += 2; }
                    const char *s = jstr(json_get(missing, i));
                    size_t sl = strlen(s);
                    memcpy(joined + m, s, sl); m += sl;
                }
                joined[m] = 0;
                char buf[2300];
                snprintf(buf, sizeof(buf), "Not an unknown (lost) admission of this session: %s", joined);
                json_set(result, "error", json_string(buf));
                free(joined);
            }
            json_set(result, "code", json_number(-1));
            json_set(result, "chosen", json_array());
            json_free(unknown);
            return result;
        }
    }

    if (chosen->c.count == 0 && !use_admission) {
        for (size_t i = 0; i < unknown->c.count; i++) {
            json_t *aid = json_obj_get(json_get(unknown, i), "admission_id");
            json_append(chosen, json_copy(aid));
        }
    }

    if (chosen->c.count == 0) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "Nothing to discard: session %s has no unknown admission.", sid);
        json_append(lines, json_string(buf));
        json_set(result, "code", json_number(0));
        json_free(unknown);
        return result;
    }

    if (!yes) {
        if (!isatty) {
            json_append(errors, json_string(
                "Refusing to discard without confirmation on a non-interactive stdin; pass --yes."));
            json_set(result, "code", json_number(2));
            /* chosen was filtered but nothing resolved: clear it to mirror rpc-driven C view */
            json_set(result, "chosen", json_array());
            json_free(unknown);
            return result;
        }
        if (!confirm) {
            json_append(lines, json_string("Cancelled; nothing was discarded."));
            json_set(result, "code", json_number(1));
            json_set(result, "chosen", json_array());
            json_free(unknown);
            return result;
        }
    }

    /* resolved: only these produce prompt.resolve_unknown rpcs */
    json_t *resolved = json_array();
    for (size_t i = 0; i < chosen->c.count; i++) {
        const char *aid = jstr(json_get(chosen, i));
        json_append(resolved, json_string(aid));
        char buf[1024];
        snprintf(buf, sizeof(buf), "Discarded %s", aid);
        json_append(lines, json_string(buf));
    }
    json_set(result, "chosen", resolved); /* json_set frees the previous chosen array */
    json_set(result, "code", json_number(0));
    json_free(unknown);
    return result;
}

