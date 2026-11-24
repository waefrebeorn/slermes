/*
 * update_cmd_pause_gate.c — C11 port of upstream Hermes
 * hermes_cli/update_cmd_pause_gate.py (gate w25_updatecmdp_22338d).
 *
 * PoP: w25_updatecmdp_22338d_checkout_move @ hermes_cli/update_cmd_pause_gate.py:_checkout_move
 * PoP: w25_updatecmdp_22338d_settle @ hermes_cli/update_cmd_pause_gate.py:_settle_checkout_move
 *
 * One checkout-writing git step (switch, merge, reset, upstream sync, rollback,
 * stash) is bound to the pause record BEFORE git writes a file: the commits it
 * can move to and the tracked paths it rewrites, on a baseline taken at the HEAD
 * it starts from; and it is judged the moment git returns.  The pause record and
 * git are seams -- install root, tree read, torn-path read and the record write
 * are fixed inputs here.  What is ported is what those values decide.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_updatecmdp_22338d.h"

/* Python truthiness of a JSON value. */
static int up_truthy(const json_t *v) {
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

/* Python '==' over the shapes a tree field holds; absent and null are both None. */
static int up_equal(const json_t *a, const json_t *b) {
    size_t i, n;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER) return (a->bool_val ? 1.0 : 0.0) == b->num_val;
    if (b->type == JSON_BOOL && a->type == JSON_NUMBER) return a->num_val == (b->bool_val ? 1.0 : 0.0);
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!up_equal(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !up_equal(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

/* all(now[key] == found[key] for key in found) */
static int up_same_tree(const json_t *now, const json_t *found) {
    size_t i, n = json_object_size(found);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(found, i);
        if (!k || !up_equal(json_obj_get(now, k), json_object_get_at(found, i))) return 0;
    }
    return 1;
}

/* repr() of a scalar as an f-string interpolates it ("None" for null). */
static void up_text(const json_t *v, char *buf, size_t size) {
    if (!v) { snprintf(buf, size, "None"); return; }
    switch (v->type) {
    case JSON_STRING: snprintf(buf, size, "%s", v->str_val ? v->str_val : ""); return;
    case JSON_NUMBER: snprintf(buf, size, "%g", v->num_val); return;
    case JSON_NULL:   snprintf(buf, size, "None"); return;
    default:          snprintf(buf, size, "%s", ""); return;
    }
}

/* sorted({...}) over tracked paths. */
typedef struct {
    char  **items;
    size_t  n, cap;
} up_set;

static void up_set_free(up_set *s) {
    size_t i;
    for (i = 0; i < s->n; i++) free(s->items[i]);
    free(s->items);
    s->items = NULL;
    s->n = s->cap = 0;
}

static void up_set_add(up_set *s, const char *text) {
    size_t i, len;
    if (!text) return;
    for (i = 0; i < s->n; i++)
        if (strcmp(s->items[i], text) == 0) return;
    if (s->n == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 8;
        char **grown = (char **)realloc(s->items, cap * sizeof(char *));
        if (!grown) return;
        s->items = grown;
        s->cap = cap;
    }
    len = strlen(text);
    s->items[s->n] = (char *)malloc(len + 1);
    if (!s->items[s->n]) return;
    memcpy(s->items[s->n], text, len + 1);
    s->n++;
}

static void up_set_add_all(up_set *s, const json_t *array) {
    size_t i, n = json_len(array);
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(array, i);
        if (v && v->type == JSON_STRING) up_set_add(s, v->str_val);
    }
}

static int up_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static json_t *up_set_array(up_set *s) {
    json_t *out = json_array();
    size_t i;
    if (s->n > 1) qsort(s->items, s->n, sizeof(char *), up_cmp);
    for (i = 0; i < s->n; i++) json_append(out, json_string(s->items[i]));
    return out;
}

/* token["baselines"] = [b for b in token["baselines"] if b is not baseline] */
static void up_drop_baseline(json_t *token, const json_t *baseline) {
    json_t *baselines = json_obj_get(token, "baselines");
    size_t i, n;
    if (!baselines || baselines->type != JSON_ARRAY) return;
    n = json_len(baselines);
    for (i = 0; i < n; i++) {
        if (json_get(baselines, i) == baseline) {
            json_array_remove(baselines, i);   /* identity, not equality */
            return;
        }
    }
}

/* _settle_checkout_move's decision, over mutable token/baseline. */
static void up_settle(json_t *token, json_t *baseline, const json_t *found,
                      const json_t *now, const json_t *torn, const json_t *step) {
    int moved = !up_equal(json_obj_get(now, "pre_sha"), json_obj_get(found, "pre_sha"));
    int whole;
    if (moved) {
        if (torn && torn->type != JSON_NULL) {
            json_t *landed = json_obj_get(baseline, "landed");
            char sha[256];
            if (!landed || landed->type != JSON_OBJECT) {
                json_set(baseline, "landed", json_object());
                landed = json_obj_get(baseline, "landed");
            }
            up_text(json_obj_get(now, "pre_sha"), sha, sizeof(sha));
            json_set(landed, sha, json_copy(torn));
            whole = torn->type == JSON_ARRAY && json_len(torn) == 0;
        } else {
            whole = 0;   /* torn is None: unknown stays unjudged */
        }
    } else {
        whole = up_truthy(json_obj_get(step, "whole")) || up_same_tree(now, found);
    }
    if (!up_equal(json_obj_get(baseline, "pause_id"), json_obj_get(token, "pause_id")) && whole)
        up_drop_baseline(token, baseline);
}

json_t *w25_updatecmdp_22338d_settle(const json_t *req) {
    json_t *token = json_copy(json_obj_get(req, "token"));
    json_t *baselines = json_obj_get(token, "baselines");
    const json_t *found = json_obj_get(req, "found");
    const json_t *now = json_obj_get(req, "now");
    json_t *step = json_object(), *baseline, *out = json_object();
    int index = (int)json_get_num(req, "baseline_index", 0);

    if (up_truthy(json_obj_get(req, "step_whole"))) json_set(step, "whole", json_bool(1));
    if (!baselines || baselines->type != JSON_ARRAY || index < 0
        || (size_t)index >= json_len(baselines)) {
        json_free(step);
        json_set(out, "token", token);
        json_set(out, "baseline", json_null());
        json_set(out, "writes", json_number(0));
        return out;
    }
    baseline = json_get(baselines, (size_t)index);
    up_settle(token, baseline, found, now, json_obj_get(req, "torn"), step);
    json_set(out, "token", token);
    json_set(out, "baseline", json_copy(baseline));
    json_set(out, "writes", json_number(1));
    json_free(step);
    return out;
}

json_t *w25_updatecmdp_22338d_checkout_move(const json_t *req) {
    json_t *token = json_copy(json_obj_get(req, "token"));
    json_t *out = json_object(), *step = json_object();
    json_t *baselines, *baseline = NULL;
    const json_t *found = json_obj_get(req, "found");
    const json_t *now = json_obj_get(req, "now");
    const json_t *targets_in = json_obj_get(req, "targets");
    const json_t *paths_in = json_obj_get(req, "paths");
    int revert = up_truthy(json_obj_get(req, "revert"));
    int writes = 0;
    up_set targets = {NULL, 0, 0}, paths = {NULL, 0, 0};
    size_t i, n;

    if (up_truthy(json_obj_get(req, "step_whole"))) json_set(step, "whole", json_bool(1));

    /* targets = sorted({t for t in targets if t}) */
    n = json_len(targets_in);
    for (i = 0; i < n; i++) {
        const json_t *t = json_get(targets_in, i);
        if (up_truthy(t) && t->type == JSON_STRING) up_set_add(&targets, t->str_val);
    }
    up_set_add_all(&paths, paths_in);

    /* root is only read when a pause id and something to bind are present. */
    if (!up_truthy(json_obj_get(token, "pause_id"))
        || (!targets.n && !paths.n && !revert) || !found || found->type != JSON_OBJECT) {
        json_set(out, "step", step);
        json_set(out, "token", token);
        json_set(out, "writes", json_number(0));
        up_set_free(&targets);
        up_set_free(&paths);
        return out;
    }

    /* paths = {*paths, *(found["dirty_at_pause"] or [] if revert else ())} */
    if (revert) up_set_add_all(&paths, json_obj_get(found, "dirty_at_pause"));

    baselines = json_obj_get(token, "baselines");
    if (!baselines || baselines->type != JSON_ARRAY) {
        json_set(token, "baselines", json_array());
        baselines = json_obj_get(token, "baselines");
    }
    {
        char pause_id[512], pre_sha[256], move_id[800];
        up_text(json_obj_get(token, "pause_id"), pause_id, sizeof(pause_id));
        up_text(json_obj_get(found, "pre_sha"), pre_sha, sizeof(pre_sha));
        snprintf(move_id, sizeof(move_id), "%s@%s", pause_id, pre_sha);

        json_t *wanted_id = json_string(move_id);
        n = json_len(baselines);
        for (i = 0; i < n; i++) {
            json_t *candidate = json_get(baselines, i);
            const json_t *cid = json_obj_get(candidate, "pause_id");
            if (!up_equal(json_obj_get(candidate, "pre_sha"), json_obj_get(found, "pre_sha")))
                continue;
            if (up_equal(cid, json_obj_get(token, "pause_id")) || up_equal(cid, wanted_id))
                { baseline = candidate; break; }
        }
        json_free(wanted_id);
        if (!baseline) {
            /* baseline = {"pause_id": move_id, **found} */
            json_t *fresh = json_object();
            json_set(fresh, "pause_id", json_string(move_id));
            for (i = 0; i < json_object_size(found); i++) {
                const char *k = json_object_get_key_at(found, i);
                if (k) json_set(fresh, k, json_copy(json_object_get_at(found, i)));
            }
            json_append(baselines, fresh);
            baseline = fresh;
        }
        /* for key, bound in (("move_targets", targets), ("move_paths", paths)) */
        {
            const char *keys[2] = {"move_targets", "move_paths"};
            up_set *sets[2] = {&targets, &paths};
            int k;
            for (k = 0; k < 2; k++) {
                if (!sets[k]->n) continue;
                {
                    up_set merged = {NULL, 0, 0};
                    up_set_add_all(&merged, json_obj_get(baseline, keys[k]));
                    for (i = 0; i < sets[k]->n; i++) up_set_add(&merged, sets[k]->items[i]);
                    json_set(baseline, keys[k], up_set_array(&merged));
                    up_set_free(&merged);
                }
            }
        }
    }
    writes = 1;   /* _pause_record.write({**token, "resume_needed": True}) */

    up_settle(token, baseline, found, now, json_obj_get(req, "torn"), step);
    writes = 2;   /* the settle write */

    json_set(out, "step", step);
    json_set(out, "token", token);
    json_set(out, "writes", json_number(writes));
    up_set_free(&targets);
    up_set_free(&paths);
    return out;
}
