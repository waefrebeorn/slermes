/*
 * update_pause_record.c — C port of upstream Hermes hermes_cli/update_pause_record.py.
 *
 * PoP: w18_updatepaus_c43de1_without @ hermes_cli/update_pause_record.py:_without
 * PoP: w18_updatepaus_c43de1_has_work @ hermes_cli/update_pause_record.py:_has_work
 * PoP: w18_updatepaus_c43de1_relaunch_keys @ hermes_cli/update_pause_record.py:_relaunch_keys
 *
 * The durable-filesystem / process-liveness bookkeeping is out of scope; these
 * are the pure token transforms the module uses, ported verbatim (including
 * str() coercion of pid values and the "posix_units only when present" rule).
 */
#include "noc_w18_updatepaus_c43de1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *w18_updatepaus_c43de1_RECORD_STEM = ".hermes-update-paused-gateways";

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
        default:          return 0;
    }
}

/* Python str() for the scalar values used as pid/key fragments. */
static void py_str(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    switch (v->type) {
        case JSON_STRING: snprintf(buf, cap, "%s", v->str_val); return;
        case JSON_BOOL:   snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, cap, "%lld", (long long)v->num_val);
            else
                snprintf(buf, cap, "%g", v->num_val);
            return;
        default: snprintf(buf, cap, ""); return;
    }
}

static int pid_in(const json_t *pids, const json_t *v) {
    char s[128];
    py_str(v, s, sizeof(s));
    if (!json_is_array(pids)) return 0;
    for (size_t i = 0; i < json_len(pids); i++) {
        json_t *e = json_get(pids, i);
        if (e && e->type == JSON_STRING && !strcmp(e->str_val, s)) return 1;
    }
    return 0;
}

json_t *w18_updatepaus_c43de1_without(const json_t *token, const json_t *pids) {
    json_t *t = json_copy(token ? token : json_object());
    if (!json_is_object(t)) return t;

    json_t *profiles = json_obj_get(t, "profiles");
    json_t *newp = json_object();
    if (json_is_object(profiles)) {
        for (size_t i = 0; i < json_object_size(profiles); i++) {
            const char *k = json_object_get_key_at(profiles, i);
            json_t *v = json_object_get_at(profiles, i);
            if (!pid_in(pids, v)) json_set(newp, k, json_copy(v));
        }
    }
    json_set(t, "profiles", newp);

    json_t *unmapped = json_obj_get(t, "unmapped");
    json_t *newu = json_array();
    if (json_is_array(unmapped)) {
        for (size_t i = 0; i < json_len(unmapped); i++) {
            json_t *u = json_get(unmapped, i);
            json_t *pid = json_is_object(u) ? json_obj_get(u, "pid") : NULL;
            if (!pid_in(pids, pid)) json_append(newu, json_copy(u));
        }
    }
    json_set(t, "unmapped", newu);

    if (json_has(t, "posix_units")) {
        json_t *units = json_obj_get(t, "posix_units");
        json_t *newpu = json_array();
        if (json_is_array(units)) {
            for (size_t i = 0; i < json_len(units); i++) {
                json_t *u = json_get(units, i);
                json_t *pid = json_is_object(u) ? json_obj_get(u, "pid") : NULL;
                if (!pid_in(pids, pid)) json_append(newpu, json_copy(u));
            }
        }
        json_set(t, "posix_units", newpu);
    }
    return t;
}

int w18_updatepaus_c43de1_has_work(const json_t *token) {
    if (!json_is_object(token)) return 0;
    if (truthy(json_obj_get(token, "profiles"))) return 1;
    json_t *unmapped = json_obj_get(token, "unmapped");
    if (json_is_array(unmapped)) {
        for (size_t i = 0; i < json_len(unmapped); i++) {
            json_t *u = json_get(unmapped, i);
            if (json_is_object(u) && truthy(json_obj_get(u, "argv"))) return 1;
        }
    }
    if (truthy(json_obj_get(token, "services"))) return 1;
    if (truthy(json_obj_get(token, "posix_units"))) return 1;
    if (truthy(json_obj_get(token, "cold_start_if_installed"))) return 1;
    if (truthy(json_obj_get(token, "cold_start_profiles"))) return 1;
    return 0;
}

static int cmp_strptr(const void *a, const void *b) {
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    return strcmp(x, y);
}

json_t *w18_updatepaus_c43de1_relaunch_keys(const json_t *token) {
    char keys[512][256];
    size_t n = 0;
    if (json_is_object(token)) {
        json_t *profiles = json_obj_get(token, "profiles");
        if (json_is_object(profiles)) {
            for (size_t i = 0; i < json_object_size(profiles) && n < 512; i++) {
                const char *k = json_object_get_key_at(profiles, i);
                snprintf(keys[n++], 256, "profile:%s", k ? k : "");
            }
        }
        json_t *unmapped = json_obj_get(token, "unmapped");
        if (json_is_array(unmapped)) {
            for (size_t i = 0; i < json_len(unmapped) && n < 512; i++) {
                json_t *u = json_get(unmapped, i);
                if (!json_is_object(u) || !truthy(json_obj_get(u, "argv"))) continue;
                char p[128];
                py_str(json_obj_get(u, "pid"), p, sizeof(p));
                snprintf(keys[n++], 256, "unmapped:%s", p);
            }
        }
        json_t *units = json_obj_get(token, "posix_units");
        if (json_is_array(units)) {
            for (size_t i = 0; i < json_len(units) && n < 512; i++) {
                json_t *u = json_get(units, i);
                json_t *id = json_is_object(u) ? json_obj_get(u, "unit") : NULL;
                if (!id || !truthy(id)) id = json_is_object(u) ? json_obj_get(u, "label") : NULL;
                char s[128];
                py_str(id, s, sizeof(s));
                snprintf(keys[n++], 256, "unit:%s", s);
            }
        }
    }
    const char *sorted[512];
    for (size_t i = 0; i < n; i++) sorted[i] = keys[i];
    qsort(sorted, n, sizeof(char *), cmp_strptr);
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) json_append(out, json_string(sorted[i]));
    return out;
}
