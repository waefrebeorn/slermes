/*
 * admitted_authors.c - C11 port of upstream gateway/relay/admitted_authors.py
 * (gate w25_admittedau_967705).
 *
 * PoP: w25_admittedau_967705_relayed_discord @ gateway/relay/admitted_authors.py:_relayed_discord
 * PoP: w25_admittedau_967705_note_delivered @ gateway/relay/admitted_authors.py:note_delivered
 * PoP: w25_admittedau_967705_answer_admitted @ gateway/relay/admitted_authors.py:answer_admitted
 * PoP: w25_admittedau_967705_interaction_admitted @ gateway/relay/admitted_authors.py:interaction_admitted
 * PoP: w25_admittedau_967705_constants @ gateway/relay/admitted_authors.py:<literal tables>
 *
 * The process-level OrderedDict, gateway.config.Platform, the event object and
 * the command registry are seams.  What is ported here is the part that is a
 * pure function of values: whether a source is a relayed Discord one, how a
 * delivery updates the bounded recency set, and which senders an interaction
 * may reach the runner for.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_admittedau_967705.h"

/* Python truthiness of a JSON value. */
static int au_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Shortest decimal text that round-trips to *d* -- Python's str() of a float. */
static void au_fmt_double(double d, char *buf, size_t n) {
    int prec;
    if (d == (double)(long long)d && fabs(d) < 1e15) {
        snprintf(buf, n, "%.0f", d);
        return;
    }
    for (prec = 15; prec <= 17; prec++) {
        snprintf(buf, n, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) return;
    }
}

/* str(value): the Python text form used as the admission-set key.  Scalar ids
 * only -- a container id would need repr() of a nested structure, and no caller
 * stores one (NULL matches nothing). */
static char *au_pystr(const json_t *v) {
    char buf[64];
    if (!v) return strdup("None");
    switch (v->type) {
    case JSON_NULL: return strdup("None");
    case JSON_BOOL: return strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        au_fmt_double(v->num_val, buf, sizeof buf);
        return strdup(buf);
    case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
    case JSON_ARRAY:
    case JSON_OBJECT: return NULL;
    }
    return NULL;
}

/* _relayed_discord: `is True` -- only the bool, never 1. */
static int au_relayed(const json_t *source) {
    const json_t *flag, *platform;
    if (!source || source->type != JSON_OBJECT) return 0;
    flag = json_obj_get(source, "delivered_via_upstream_relay");
    if (!flag || flag->type != JSON_BOOL || !flag->bool_val) return 0;
    platform = json_obj_get(source, "platform");
    return platform && platform->type == JSON_STRING && platform->str_val
           && !strcmp(platform->str_val, "discord");
}

json_t *w25_admittedau_967705_relayed_discord(const json_t *req) {
    if (!req || req->type != JSON_OBJECT) return json_bool(false);
    return json_bool(au_relayed(json_obj_get(req, "source")) ? true : false);
}

/* The bounded recency set, materialised from the request. */
typedef struct {
    char **keys;
    size_t n, cap;
} au_set;

static void au_set_free(au_set *s) {
    size_t i;
    for (i = 0; i < s->n; i++) free(s->keys[i]);
    free(s->keys);
    s->keys = NULL;
    s->n = s->cap = 0;
}

static void au_set_push(au_set *s, char *key) {
    if (s->n == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 8;
        char **grown = (char **)realloc(s->keys, cap * sizeof(char *));
        if (!grown) { free(key); return; }
        s->keys = grown;
        s->cap = cap;
    }
    s->keys[s->n++] = key;
}

static int au_set_index(const au_set *s, const char *key) {
    size_t i;
    for (i = 0; i < s->n; i++) if (!strcmp(s->keys[i], key)) return (int)i;
    return -1;
}

static void au_set_load(au_set *s, const json_t *req) {
    const json_t *admitted = json_obj_get(req, "admitted");
    size_t i, n = (admitted && admitted->type == JSON_ARRAY) ? json_len(admitted) : 0;
    memset(s, 0, sizeof *s);
    for (i = 0; i < n; i++) {
        char *key = au_pystr(json_get(admitted, i));
        if (!key) continue;
        if (au_set_index(s, key) >= 0) { free(key); continue; } /* one dict key */
        au_set_push(s, key);
    }
}

static size_t au_max(const json_t *req) {
    const json_t *v = json_obj_get(req, "max");
    if (v && v->type == JSON_NUMBER && v->num_val >= 0) return (size_t)v->num_val;
    if (v && v->type == JSON_BOOL) return v->bool_val ? 1u : 0u;
    return (size_t)W25_ADMITTEDAU_967705_MAX;
}

json_t *w25_admittedau_967705_note_delivered(const json_t *req) {
    const json_t *source, *uid;
    au_set set;
    size_t cap;
    json_t *out;

    if (!req || req->type != JSON_OBJECT) req = json_null();
    au_set_load(&set, req);
    source = json_obj_get(req, "source");
    uid = (source && source->type == JSON_OBJECT) ? json_obj_get(source, "user_id") : NULL;

    if (au_relayed(source) && au_truthy(uid)) {
        char *key = au_pystr(uid);
        if (key) {
            int idx = au_set_index(&set, key);
            if (idx >= 0) { /* move_to_end */
                memmove(&set.keys[idx], &set.keys[idx + 1],
                        (set.n - (size_t)idx - 1) * sizeof(char *));
                set.n--;
            }
            au_set_push(&set, key);
            cap = au_max(req);
            while (set.n > cap) { /* popitem(last=False) */
                free(set.keys[0]);
                memmove(&set.keys[0], &set.keys[1], (set.n - 1) * sizeof(char *));
                set.n--;
            }
        }
    }

    out = json_object();
    {
        json_t *arr = json_array();
        size_t i;
        for (i = 0; i < set.n; i++) json_append(arr, json_string(set.keys[i]));
        json_set(out, "admitted", arr);
    }
    au_set_free(&set);
    return out;
}

/* answer_admitted: every sender but a relayed Discord one this gateway never
 * admitted (a merged turn's "" included). */
static int au_answer(const json_t *req) {
    const json_t *source, *sender;
    au_set set;
    char *key;
    int found;

    source = json_obj_get(req, "source");
    if (!au_relayed(source)) return 1;
    sender = json_obj_get(req, "sender");
    if (!au_truthy(sender)) return 0;
    key = au_pystr(sender);
    if (!key) return 0;
    au_set_load(&set, req);
    found = au_set_index(&set, key) >= 0;
    au_set_free(&set);
    free(key);
    return found;
}

json_t *w25_admittedau_967705_answer_admitted(const json_t *req) {
    if (!req || req->type != JSON_OBJECT) return json_bool(true);
    return json_bool(au_answer(req) ? true : false);
}

/* resolve_command's lookup key: name.lower().lstrip("/"). */
static char *au_norm_command(const char *name) {
    char *out;
    size_t i, lead = 0;
    if (!name) name = "";
    while (name[lead] == '/') lead++;
    out = strdup(name + lead);
    if (!out) return NULL;
    for (i = 0; out[i]; i++)
        if (out[i] >= 'A' && out[i] <= 'Z') out[i] = (char)(out[i] - 'A' + 'a');
    return out;
}

static int au_unadmitted_command(const char *normalized) {
    return normalized && (!strcmp(normalized, "approve") || !strcmp(normalized, "deny"));
}

json_t *w25_admittedau_967705_interaction_admitted(const json_t *req) {
    const json_t *sender, *admins, *command;
    char *sender_s = NULL, *name = NULL;
    int allowed = 0;

    if (!req || req->type != JSON_OBJECT) return json_bool(false);
    if (au_answer(req)) return json_bool(true);

    sender = json_obj_get(req, "sender");
    sender_s = au_truthy(sender) ? au_pystr(sender) : strdup("");
    if (sender_s && sender_s[0]) {
        admins = json_obj_get(req, "admin_ids");
        if (admins && admins->type == JSON_ARRAY) {
            size_t i;
            for (i = 0; i < json_len(admins); i++) {
                char *a = au_pystr(json_get(admins, i));
                if (a && !strcmp(a, sender_s)) { allowed = 1; free(a); break; }
                free(a);
            }
        }
    }
    if (!allowed) {
        command = json_obj_get(req, "command");
        if (au_truthy(command)) {
            name = au_norm_command(command->type == JSON_STRING ? command->str_val : "");
            allowed = au_unadmitted_command(name);
        }
    }
    free(sender_s);
    free(name);
    return json_bool(allowed ? true : false);
}

json_t *w25_admittedau_967705_constants(void) {
    json_t *o = json_object(), *cmds = json_array();
    json_append(cmds, json_string("approve"));
    json_append(cmds, json_string("deny"));
    json_set(o, "max", json_number((double)W25_ADMITTEDAU_967705_MAX));
    json_set(o, "unadmitted_commands", cmds);
    return o;
}
