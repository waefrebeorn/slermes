/*
 * session_authorities.c - C port of upstream Hermes gateway/session_authorities.py
 *
 * PoP: w14_sessionaut_eb9edc_registry @ gateway/session_authorities.py:SessionAuthorities
 *
 * Pure registry logic (keyed by home). Persistence, adapters and event loops are
 * replaced by JSON seams from the fixture.
 */
#include "noc_w14_sessionaut_eb9edc.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static int jbool(const json_t *v, int def) {
    return (v && v->type == JSON_BOOL) ? (v->bool_val ? 1 : 0) : def;
}

typedef struct {
    char *home;
    char *id;        /* authority profile_id; NULL = reserved/unbuilt slot */
    int retiring;
    char *name;
    int starting;    /* reserved by add(home,None) and not yet filled */
} Slot;

#define CAP 128
typedef struct {
    Slot s[CAP];
    size_t n;
    const char *launch;
    int multiplexed;
} Reg;

static Slot *find_slot(Reg *r, const char *home) {
    if (!home) return NULL;
    for (size_t i = 0; i < r->n; i++) if (!strcmp(r->s[i].home, home)) return &r->s[i];
    return NULL;
}
static char *dupstr(const char *s) { return s ? _strdup(s) : NULL; }
static json_t *auth_id(const Slot *s) {
    return (s && s->id) ? json_string(s->id) : json_null();
}
static int cmpstr(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

json_t *w14_sessionaut_eb9edc_registry(const json_t *state) {
    Reg r; memset(&r, 0, sizeof r);
    r.launch = json_get_str(state, "launch_home", "");
    r.multiplexed = jbool(json_obj_get(state, "multiplexed_init"), 0);

    const json_t *ops = json_obj_get(state, "ops");
    if (json_is_array(ops)) {
        for (size_t i = 0; i < json_len(ops); i++) {
            const json_t *op = json_get(ops, i);
            if (!json_is_array(op) || json_len(op) < 2) continue;
            const char *kind = jstr(json_get(op, 0));
            const char *home = jstr(json_get(op, 1));
            if (!kind || !home) continue;
            if (!strcmp(kind, "add")) {
                const json_t *auth = json_get(op, 2);
                const char *name = jstr(json_get(op, 3));
                Slot *sl = find_slot(&r, home);
                if (sl) continue; /* duplicate -> RuntimeError in Python; fixtures avoid it */
                if (r.n >= CAP) continue;
                sl = &r.s[r.n++];
                memset(sl, 0, sizeof *sl);
                sl->home = dupstr(home);
                sl->name = dupstr(name);
                if (auth && auth->type == JSON_OBJECT) {
                    sl->id = dupstr(jstr(json_obj_get(auth, "id")));
                    sl->retiring = jbool(json_obj_get(auth, "retiring"), 0);
                    sl->starting = 0;
                } else {
                    sl->id = NULL; sl->retiring = 0; sl->starting = 1;
                }
                if (strcmp(home, r.launch) != 0) r.multiplexed = 1;
            } else if (!strcmp(kind, "replace")) {
                const json_t *auth = json_get(op, 2);
                Slot *sl = find_slot(&r, home);
                if (!sl || sl->id) continue;
                sl->id = dupstr(jstr(json_is_object(auth) ? json_obj_get(auth, "id") : NULL));
                sl->retiring = jbool(json_is_object(auth) ? json_obj_get(auth, "retiring") : NULL, 0);
                sl->starting = 0;
            } else if (!strcmp(kind, "remove")) {
                Slot *sl = find_slot(&r, home);
                if (!sl || !strcmp(home, r.launch)) continue;
                free(sl->home); free(sl->id); free(sl->name);
                for (size_t k = (size_t)(sl - r.s); k + 1 < r.n; k++) r.s[k] = r.s[k+1];
                r.n--;
            }
        }
    }

    json_t *out = json_array();
    const json_t *qs = json_obj_get(state, "queries");
    if (json_is_array(qs)) {
        for (size_t i = 0; i < json_len(qs); i++) {
            const json_t *q = json_get(qs, i);
            if (!json_is_array(q) || json_len(q) < 1) { json_append(out, json_null()); continue; }
            const char *kind = jstr(json_get(q, 0));
            if (!kind) { json_append(out, json_null()); continue; }
            if (!strcmp(kind, "len")) {
                int n = 0; for (size_t k = 0; k < r.n; k++) if (r.s[k].id) n++;
                json_append(out, json_number(n));
            } else if (!strcmp(kind, "contains")) {
                json_append(out, json_bool(find_slot(&r, jstr(json_get(q, 1))) != NULL));
            } else if (!strcmp(kind, "for_home")) {
                json_append(out, auth_id(find_slot(&r, jstr(json_get(q, 1)))));
            } else if (!strcmp(kind, "launch")) {
                json_append(out, auth_id(find_slot(&r, r.launch)));
            } else if (!strcmp(kind, "multiplexed")) {
                json_append(out, json_bool(r.multiplexed));
            } else if (!strcmp(kind, "starting_homes")) {
                json_t *a = json_array();
                for (size_t k = 0; k < r.n; k++) if (r.s[k].starting) json_append(a, json_string(r.s[k].home));
                json_append(out, a);
            } else if (!strcmp(kind, "iter")) {
                json_t *a = json_array();
                for (size_t k = 0; k < r.n; k++) if (r.s[k].id) json_append(a, json_string(r.s[k].id));
                json_append(out, a);
            } else if (!strcmp(kind, "serving")) {
                json_t *a = json_array();
                for (size_t k = 0; k < r.n; k++) if (r.s[k].id && !r.s[k].retiring) json_append(a, json_string(r.s[k].id));
                json_append(out, a);
            } else if (!strcmp(kind, "served_profiles")) {
                json_t *a = json_array();
                for (size_t k = 0; k < r.n; k++) if (r.s[k].id && !r.s[k].retiring) {
                    json_t *d = json_object();
                    json_set(d, "profile_id", json_string(r.s[k].id));
                    json_set(d, "home", json_string(r.s[k].id));
                    json_append(a, d);
                }
                json_append(out, a);
            } else if (!strcmp(kind, "profile_ids")) {
                char *ids[CAP]; size_t m = 0;
                for (size_t k = 0; k < r.n; k++) if (r.s[k].id && !r.s[k].retiring) ids[m++] = r.s[k].id;
                qsort(ids, m, sizeof ids[0], cmpstr);
                json_t *a = json_array();
                for (size_t k = 0; k < m; k++) if (k == 0 || strcmp(ids[k], ids[k-1]) != 0) json_append(a, json_string(ids[k]));
                json_append(out, a);
            } else if (!strcmp(kind, "profile_name")) {
                const char *home = jstr(json_get(q, 1));
                Slot *sl = find_slot(&r, home);
                if (!home || !strcmp(home, r.launch) || !sl) json_append(out, json_null());
                else json_append(out, sl->name ? json_string(sl->name) : json_null());
            } else {
                json_append(out, json_null());
            }
        }
    }

    for (size_t k = 0; k < r.n; k++) { free(r.s[k].home); free(r.s[k].id); free(r.s[k].name); }
    return out;
}
