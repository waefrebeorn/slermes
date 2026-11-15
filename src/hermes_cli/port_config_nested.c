/* port_config_nested.c -- dict navigation ported from hermes_cli/config.py.
 *
 * PoP: cfg_split_key_path       @ hermes_cli/config.py:split_key_path
 * PoP: cfg_greedy_literal_match @ hermes_cli/config.py:greedy_literal_match
 * PoP: cfg_phantom_sibling      @ hermes_cli/config.py:phantom_sibling
 * PoP: cfg_locate_nested        @ hermes_cli/config.py:_locate_nested
 * PoP: cfg_set_nested           @ hermes_cli/config.py:_set_nested
 * PoP: cfg_get_nested           @ hermes_cli/config.py:_get_nested
 * PoP: cfg_unset_nested         @ hermes_cli/config.py:_unset_nested
 *
 * `split_key_path` is defined in port_config.c; it is reused here so the
 * escape semantics live in exactly one place.
 *
 * The #84064 family: config key names routinely contain dots (model IDs like
 * grok-4.6, Matrix room IDs, versioned provider names), so navigation prefers an
 * existing literal dotted key over blind splitting, and refuses to create an
 * intermediate mapping that would shadow an existing dotted sibling.
 */
#include "port_config_nested.h"
#include "port_config.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ── greedy_literal_match ───────────────────────────────────────────────── */
int
cfg_greedy_literal_match(json_t *container, char **parts, int nparts,
                         char *out, size_t cap)
{
    if (!container || container->type != JSON_OBJECT || !parts || nparts <= 0)
        return 0;

    for (int n = nparts; n > 0; n--) {
        /* join parts[0..n) with '.' */
        size_t need = 0;
        for (int i = 0; i < n; i++)
            need += strlen(parts[i]) + (i ? 1 : 0);
        if (need + 1 > cap)
            continue;
        size_t w = 0;
        for (int i = 0; i < n; i++) {
            if (i)
                out[w++] = '.';
            size_t l = strlen(parts[i]);
            memcpy(out + w, parts[i], l);
            w += l;
        }
        out[w] = '\0';

        if (json_obj_get(container, out) != NULL)
            return n;
    }
    return 0;
}

/* ── phantom_sibling ────────────────────────────────────────────────────── */
int
cfg_phantom_sibling(json_t *container, const char *part,
                    char *out, size_t cap)
{
    if (!container || container->type != JSON_OBJECT || !part)
        return 0;
    size_t plen = strlen(part);
    for (size_t i = 0; i < container->c.count; i++) {
        const char *k = container->c.keys[i];
        if (!k)
            continue;
        if (strncmp(k, part, plen) == 0 && k[plen] == '.') {
            snprintf(out, cap, "%s", k);
            return 1;
        }
    }
    return 0;
}

/* ── _locate_nested ──────────────────────────────────────────────────────── */
int
cfg_locate_nested(json_t *root, char **parts, int nparts, cfg_loc_t *loc)
{
    if (!root || !parts || nparts <= 0 || !loc)
        return 0;

    memset(loc, 0, sizeof(*loc));
    json_t **parents = NULL;
    char **parent_keys = NULL;
    int np = 0;

    json_t *current = root;
    int i = 0;
    for (;;) {
        const char *key = NULL;
        long index = 0;
        int is_index = 0;
        int consumed = 1;

        int remaining = nparts - i;

        if (current->type == JSON_ARRAY) {
            if (remaining < 1)
                goto fail;
            const char *s = parts[i];
            char *end = NULL;
            index = strtol(s, &end, 10);
            if (!s[0] || (end && *end))
                goto fail;                       /* not a numeric index */
            if (index < 0 || (size_t)index >= current->c.count)
                goto fail;                       /* out of range */
            /* loc.key is always owned by the caller, so copy it: aliasing
             * parts[i] would double-free when the caller frees its own copy. */
            key = strdup(s);
            if (!key)
                goto fail;
            is_index = 1;
        } else if (current->type == JSON_OBJECT) {
            char joined[2048];
            int n = cfg_greedy_literal_match(current, parts + i, remaining,
                                             joined, sizeof(joined));
            if (n <= 0)
                goto fail;                       /* no literal key matches */
            consumed = n;
            /* Stash a stable copy: `joined` is reused next iteration. */
            key = strdup(joined);
            if (!key)
                goto fail;
        } else {
            goto fail;                           /* scalar before path end */
        }

        i += consumed;
        if (i >= nparts) {
            loc->container = current;
            loc->key = (char *)key;
            loc->is_index = is_index;
            loc->parents = parents;
            loc->parent_keys = parent_keys;
            loc->n_parents = np;
            return 1;
        }

        /* descend, recording the hop */
        json_t *next;
        if (is_index)
            next = current->c.items[index];
        else
            next = json_obj_get(current, key);

        json_t **np2 = (json_t **)realloc(parents, sizeof(json_t *) * (size_t)(np + 1));
        if (!np2)
            goto fail;
        parents = np2;
        char **nk2 = (char **)realloc(parent_keys, sizeof(char *) * (size_t)(np + 1));
        if (!nk2)
            goto fail;
        parent_keys = nk2;
        parents[np] = current;
        parent_keys[np] = (char *)key;
        np++;

        current = next;
        if (!current)
            goto fail;
    }

fail:
    free(parents);
    free(parent_keys);
    return 0;
}

/* ── _set_nested ─────────────────────────────────────────────────────────── */
int
cfg_set_nested(json_t *root, const char *dotted_key, json_t *value)
{
    if (!root || !dotted_key || !value)
        return -1;

    char *segs[256];
    int n = cfg_split_key_path(dotted_key, segs, 256);
    if (n <= 0)
        return -1;

    json_t *current = root;
    int i = 0;
    while (i < n) {
        int remaining = n - i;
        int at_leaf = (remaining == 1);

        if (current->type == JSON_ARRAY) {
            const char *s = segs[i];
            char *end = NULL;
            long idx = strtol(s, &end, 10);
            if (!s[0] || (end && *end)) {
                /* The Python only wraps the NON-leaf branch in
                 * `except (TypeError, ValueError)`; at the leaf a non-numeric
                 * index propagates as a bare ValueError. */
                for (int k = 0; k < n; k++) free(segs[k]);
                return 1;
            }
            if (at_leaf) {
                if (idx < 0 || (size_t)idx >= current->c.count) {
                    for (int k = 0; k < n; k++) free(segs[k]);
                    return 2;                /* list assignment out of range */
                }
                current->c.items[idx] = value;
                for (int k = 0; k < n; k++) free(segs[k]);
                return 0;
            }
            if (idx < 0 || (size_t)idx >= current->c.count) {
                for (int k = 0; k < n; k++) free(segs[k]);
                return 2;
            }
            current = current->c.items[idx];
            i++;
            continue;
        }

        if (current->type == JSON_OBJECT) {
            char joined[2048];
            int consumed = cfg_greedy_literal_match(current, segs + i, remaining,
                                                    joined, sizeof(joined));
            if (consumed > 0) {
                if (i + consumed == n) {
                    json_set(current, joined, value);
                    for (int k = 0; k < n; k++) free(segs[k]);
                    return 0;
                }
                /* Preserve dicts and lists; replace a scalar with a fresh map. */
                json_t *child = json_obj_get(current, joined);
                if (!child)
                    return 2;
                if (child->type != JSON_OBJECT && child->type != JSON_ARRAY) {
                    json_t *fresh = json_object();
                    if (!fresh)
                        return -1;
                    json_set(current, joined, fresh);
                    child = fresh;
                }
                current = child;
                i += consumed;
                continue;
            }

            const char *part = segs[i];
            if (at_leaf) {
                json_set(current, part, value);
                for (int k = 0; k < n; k++) free(segs[k]);
                return 0;
            }
            char shadowed[2048];
            if (cfg_phantom_sibling(current, part, shadowed, sizeof(shadowed)))
                return 1;                        /* refuse: phantom sibling */

            json_t *child = json_obj_get(current, part);
            if (!child) {
                child = json_object();
                if (!child)
                    return -1;
                json_set(current, part, child);
            }
            current = child;
            i++;
            continue;
        }

        return 2;                                /* scalar in the middle */
    }

    for (int k = 0; k < n; k++) free(segs[k]);
    return 0;
}

/* ── _get_nested ─────────────────────────────────────────────────────────── */
json_t *
cfg_get_nested(json_t *root, const char *dotted_key)
{
    if (!root || !dotted_key)
        return NULL;

    char *segs[256];
    int n = cfg_split_key_path(dotted_key, segs, 256);
    if (n <= 0) {
        for (int k = 0; k < n; k++) free(segs[k]);
        return NULL;
    }

    cfg_loc_t loc;
    if (!cfg_locate_nested(root, segs, n, &loc)) {
        for (int k = 0; k < n; k++) free(segs[k]);
        return NULL;
    }
    json_t *found;
    if (loc.is_index)
        found = loc.container->c.items[strtol(loc.key, NULL, 10)];
    else
        found = json_obj_get(loc.container, loc.key);

    free(loc.key);
    free(loc.parents);
    free(loc.parent_keys);
    for (int k = 0; k < n; k++) free(segs[k]);
    return found;
}

/* ── _unset_nested ───────────────────────────────────────────────────────── */
int
cfg_unset_nested(json_t *root, const char *dotted_key)
{
    if (!root || !dotted_key)
        return 0;

    char *segs[256];
    int n = cfg_split_key_path(dotted_key, segs, 256);
    if (n <= 0) {
        for (int k = 0; k < n; k++) free(segs[k]);
        return 0;
    }

    cfg_loc_t loc;
    if (!cfg_locate_nested(root, segs, n, &loc)) {
        for (int k = 0; k < n; k++) free(segs[k]);
        return 0;
    }

    /* del current[key] -- for a list leaf this shifts, matching Python `del`. */
    if (loc.is_index) {
        long idx = strtol(loc.key, NULL, 10);
        if (idx < 0 || (size_t)idx >= loc.container->c.count) {
            free(loc.key); free(loc.parents); free(loc.parent_keys);
            for (int k = 0; k < n; k++) free(segs[k]);
            return 0;
        }
        json_t *doomed = loc.container->c.items[idx];
        for (size_t j = (size_t)idx; j + 1 < loc.container->c.count; j++)
            loc.container->c.items[j] = loc.container->c.items[j + 1];
        loc.container->c.count--;
        json_free(doomed);
        /* A list is never dropped for becoming empty (only object containers
         * are), so there is nothing to prune here. */
        free(loc.key); free(loc.parents); free(loc.parent_keys);
        for (int k = 0; k < n; k++) free(segs[k]);
        return 1;
    }

    json_obj_del(loc.container, loc.key);

    /* Drop empty object containers left behind, walking back up. */
    for (int i = loc.n_parents - 1; i >= 0; i--) {
        json_t *cur = loc.container;
        if (cur->type != JSON_OBJECT || cur->c.count != 0)
            break;
        json_t *parent = loc.parents[i];
        const char *part = loc.parent_keys[i];
        if (parent->type == JSON_OBJECT)
            json_obj_del(parent, part);
        loc.container = parent;
    }

    free(loc.key);
    free(loc.parents);
    free(loc.parent_keys);
    for (int k = 0; k < n; k++) free(segs[k]);
    return 1;
}
