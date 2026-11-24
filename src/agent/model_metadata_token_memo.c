/*
 * model_metadata_token_memo.c - C11 port of upstream
 * agent/model_metadata_token_memo.py (gate w24_modelmetad_ad7f50).
 *
 * PoP: w24_modelmetad_ad7f50_entry_bytes @ agent/model_metadata_token_memo.py:_entry_bytes
 * PoP: w24_modelmetad_ad7f50_oversize @ agent/model_metadata_token_memo.py:remember_message_tokens
 * PoP: w24_modelmetad_ad7f50_memo @ agent/model_metadata_token_memo.py:remember_message_tokens
 * PoP: w24_modelmetad_ad7f50_forget @ agent/model_metadata_token_memo.py:forget_message_token_memo
 * PoP: w24_modelmetad_ad7f50_constants @ agent/model_metadata_token_memo.py:<module bounds>
 *
 * The memo is bounded two ways, both least-recently-used first, and both
 * bounds govern idle entries only: an entry looked up inside the active window
 * is never evicted, so a conversation larger than the budget overshoots while
 * it runs instead of missing on every message.  That policy, the per-entry
 * byte estimate and the long-string sweep are pure functions of values and are
 * what this file owns.  The clock, the lock, the OrderedDict and the object
 * sizes are seams: the caller supplies the time and the measured pin bytes.
 */
#include <stdlib.h>
#include <string.h>
#include "noc_w24_modelmetad_ad7f50.h"

typedef struct {
    char *key;          /* owned */
    double size;        /* _entry_bytes of the entry, measured by the caller */
    double touched;     /* clock time of the last lookup or insert */
} mm_slot;

static void mm_slot_free(mm_slot *s) {
    free(s->key);
    s->key = NULL;
}

/* Remove index i, shifting the tail left (the LRU order is the array order). */
static void mm_remove_at(mm_slot *slots, size_t *n, size_t i) {
    size_t j;
    mm_slot_free(&slots[i]);
    for (j = i; j + 1 < *n; j++) slots[j] = slots[j + 1];
    (*n)--;
    slots[*n].key = NULL;
}

/* entry_bytes: sum of the pin sizes plus the per-leaf overhead. */
static double mm_entry_bytes(const json_t *sizes) {
    size_t n = json_len(sizes), i;
    double total = 0.0;
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(sizes, i);
        if (v && v->type == JSON_NUMBER) total += v->num_val;
        else if (v && v->type == JSON_BOOL) total += v->bool_val ? 1.0 : 0.0;
    }
    return total + (double)W24_MODELMETAD_AD7F50_LEAF_OVERHEAD * (double)n;
}

static double mm_num(const json_t *v, double def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

json_t *w24_modelmetad_ad7f50_entry_bytes(const json_t *req) {
    return json_number(mm_entry_bytes(json_obj_get(req, "pin_sizes")));
}

json_t *w24_modelmetad_ad7f50_oversize(const json_t *req) {
    return json_bool(mm_num(json_obj_get(req, "size"), 0.0) > W24_MODELMETAD_AD7F50_MAX_BYTES);
}

static int mm_find(const mm_slot *slots, size_t n, const char *key) {
    size_t i;
    for (i = 0; i < n; i++) if (slots[i].key && key && !strcmp(slots[i].key, key)) return (int)i;
    return -1;
}

json_t *w24_modelmetad_ad7f50_memo(const json_t *req) {
    const json_t *ops = json_obj_get(req, "ops");
    size_t n_ops = json_len(ops), cap, n = 0, i;
    double retained = 0.0;
    mm_slot *slots;
    json_t *out, *keys;

    cap = n_ops + 1;
    slots = (mm_slot *)calloc(cap, sizeof(mm_slot));
    if (!slots) return json_object();
    for (i = 0; i < n_ops; i++) {
        const json_t *op = json_get(ops, i);
        const char *what = json_get_str(op, "op", "");
        const char *key = json_get_str(op, "key", NULL);
        double now = mm_num(json_obj_get(op, "now"), 0.0);
        int at;

        if (!strcmp(what, "lookup")) {
            at = mm_find(slots, n, key);
            if (at >= 0) {
                mm_slot moved = slots[at];
                size_t j;
                moved.touched = now;
                for (j = (size_t)at; j + 1 < n; j++) slots[j] = slots[j + 1];
                slots[n - 1] = moved;   /* move_to_end */
            }
            continue;
        }
        if (strcmp(what, "remember")) continue;

        {
            double size = mm_entry_bytes(json_obj_get(op, "sizes"));
            double idle_before = now - W24_MODELMETAD_AD7F50_ACTIVE_WINDOW_S;
            char *copy;
            if (size > W24_MODELMETAD_AD7F50_MAX_BYTES) continue;  /* recomputed, never admitted */
            if (n == 0) retained = 0.0;                            /* an emptied memo restarts the count */
            at = mm_find(slots, n, key);
            if (at >= 0) {
                retained -= slots[at].size;
                mm_remove_at(slots, &n, (size_t)at);
            }
            if (n + 2 > cap) {
                mm_slot *grown = (mm_slot *)realloc(slots, (cap * 2) * sizeof(mm_slot));
                if (!grown) break;
                slots = grown;
                cap *= 2;
            }
            copy = strdup(key ? key : "");
            slots[n].key = copy;
            slots[n].size = size;
            slots[n].touched = now;
            n++;
            retained += size;
            while (n > 1 && (n > W24_MODELMETAD_AD7F50_MAX_ENTRIES ||
                             retained > W24_MODELMETAD_AD7F50_MAX_BYTES)) {
                if (slots[0].touched >= idle_before) break;
                retained -= slots[0].size;
                mm_remove_at(slots, &n, 0);
            }
        }
    }
    out = json_object();
    keys = json_array();
    for (i = 0; i < n; i++) json_append(keys, json_string(slots[i].key ? slots[i].key : ""));
    json_set(out, "keys", keys);
    json_set(out, "retained", json_number(retained));
    for (i = 0; i < n; i++) mm_slot_free(&slots[i]);
    free(slots);
    return out;
}

/* The value a name stands for: texts[name] when declared, else the name. */
static const char *mm_resolve(const json_t *texts, const char *name) {
    const char *mapped;
    if (!name) return "";
    if (!texts || texts->type != JSON_OBJECT) return name;
    mapped = json_get_str(texts, name, NULL);
    return mapped ? mapped : name;
}

/* Collect the long (len > 64) string values of a message tree into owned. */
static void mm_collect(const json_t *value, const json_t *texts,
                       char ***owned, size_t *n, size_t *cap) {
    size_t i;
    if (!value) return;
    if (value->type == JSON_STRING) {
        const char *text = mm_resolve(texts, value->str_val);
        size_t len = strlen(text), j;
        if (len <= W24_MODELMETAD_AD7F50_LONG_STRING) return;
        for (j = 0; j < *n; j++) if (!strcmp((*owned)[j], text)) return;
        if (*n + 2 > *cap) {
            size_t next = *cap ? *cap * 2 : 8;
            char **grown = (char **)realloc(*owned, next * sizeof(char *));
            if (!grown) return;
            *owned = grown;
            *cap = next;
        }
        (*owned)[(*n)++] = strdup(text);
        return;
    }
    if (value->type == JSON_ARRAY) {
        for (i = 0; i < json_len(value); i++) mm_collect(json_get(value, i), texts, owned, n, cap);
        return;
    }
    if (value->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(value); i++)
            mm_collect(json_object_get_at(value, i), texts, owned, n, cap);
    }
}

json_t *w24_modelmetad_ad7f50_forget(const json_t *req) {
    const json_t *texts = json_obj_get(req, "texts");
    const json_t *memo = json_obj_get(req, "memo");
    const json_t *keep = json_obj_get(req, "keep");
    char **owned = NULL, **kept = NULL;
    size_t n_owned = 0, cap_owned = 0, n_kept = 0, cap_kept = 0;
    size_t n_memo = json_len(memo), i, j, k, live_n = 0, evicted = 0;
    mm_slot *live;
    json_t *out, *keys;

    mm_collect(json_obj_get(req, "messages"), texts, &owned, &n_owned, &cap_owned);
    if (n_owned && keep && keep->type != JSON_NULL) {
        mm_collect(keep, texts, &kept, &n_kept, &cap_kept);
        for (i = 0; i < n_kept; i++)
            for (j = 0; j < n_owned; j++)
                if (!strcmp(owned[j], kept[i])) {
                    free(owned[j]);
                    for (k = j; k + 1 < n_owned; k++) owned[k] = owned[k + 1];
                    n_owned--;
                    break;
                }
    }
    live = (mm_slot *)calloc(n_memo ? n_memo : 1, sizeof(mm_slot));
    if (!live) {
        out = json_object();
        json_set(out, "evicted", json_number(0));
        json_set(out, "keys", json_array());
        return out;
    }
    /* The cache the sweep inspects: one entry per memo row, as inserted. */
    for (i = 0; i < n_memo; i++) {
        const json_t *row = json_get(memo, i);
        const json_t *pins_raw = row && row->type == JSON_OBJECT ? json_obj_get(row, "pins") : NULL;
        size_t np = json_len(pins_raw), p;
        double size = 0.0;
        int stale = 0;
        for (p = 0; p < np; p++) {
            const json_t *pin = json_get(pins_raw, p);
            const char *text;
            if (!pin || pin->type != JSON_STRING) continue;
            text = mm_resolve(texts, pin->str_val);
            if (strlen(text) > W24_MODELMETAD_AD7F50_LONG_STRING) {
                for (j = 0; j < n_owned; j++)
                    if (!strcmp(owned[j], text)) stale = 1;
            }
            size += 1.0;
        }
        live[live_n].key = strdup(json_get_str(row, "key", ""));
        live[live_n].size = size;
        live[live_n].touched = 0.0;
        if (stale) {
            evicted++;
            mm_slot_free(&live[live_n]);
        } else {
            live_n++;
        }
    }
    out = json_object();
    json_set(out, "evicted", json_number((double)evicted));
    keys = json_array();
    for (i = 0; i < live_n; i++) json_append(keys, json_string(live[i].key ? live[i].key : ""));
    json_set(out, "keys", keys);
    for (i = 0; i < live_n; i++) mm_slot_free(&live[i]);
    free(live);
    for (i = 0; i < n_owned; i++) free(owned[i]);
    free(owned);
    for (i = 0; i < n_kept; i++) free(kept[i]);
    free(kept);
    return out;
}

json_t *w24_modelmetad_ad7f50_constants(void) {
    json_t *o = json_object();
    json_set(o, "max_entries", json_number(W24_MODELMETAD_AD7F50_MAX_ENTRIES));
    json_set(o, "max_bytes", json_number(W24_MODELMETAD_AD7F50_MAX_BYTES));
    json_set(o, "leaf_overhead", json_number(W24_MODELMETAD_AD7F50_LEAF_OVERHEAD));
    json_set(o, "active_window_s", json_number(W24_MODELMETAD_AD7F50_ACTIVE_WINDOW_S));
    json_set(o, "long_string", json_number(W24_MODELMETAD_AD7F50_LONG_STRING));
    return o;
}
