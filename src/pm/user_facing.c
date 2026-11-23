/*
 * user_facing.c — C home for upstream Hermes pm/user_facing.py
 * Gate: w5_userfacing_57dafa
 *
 * PoP: w5_userfacing_57dafa_copies @ pm/user_facing.py:copies
 * PoP: w5_userfacing_57dafa_user_facing_path @ pm/user_facing.py:user_facing_path
 *
 * The Python module's inputs are process-global (OS, sealed payload, PM store).
 * Those are injected as explicit seams from the fixture; the list algorithms
 * (normcase keys, swap, first-occurrence dedup, python-stub cut) are ported 1:1.
 */
#include "noc_w5_userfacing_57dafa.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char **v; size_t n, cap; } UFList;

static void uf_push(UFList *l, const char *s) {
    if (!s) return;
    if (l->n == l->cap) {
        size_t nc = l->cap ? l->cap * 2 : 8;
        char **tmp = (char **)realloc(l->v, nc * sizeof(char *));
        if (!tmp) return;
        l->v = tmp; l->cap = nc;
    }
    l->v[l->n++] = strdup(s);
}

static void uf_free(UFList *l) {
    for (size_t i = 0; i < l->n; i++) free(l->v[i]);
    free(l->v); l->v = NULL; l->n = l->cap = 0;
}

static int uf_has(const UFList *l, const char *s) {
    for (size_t i = 0; i < l->n; i++) if (!strcmp(l->v[i], s)) return 1;
    return 0;
}

static void uf_from_json(UFList *l, const struct json_t *arr) {
    if (!arr || arr->type != JSON_ARRAY) return;
    for (size_t i = 0; i < json_len(arr); i++) {
        struct json_t *it = json_get(arr, i);
        if (it && it->type == JSON_STRING) uf_push(l, it->str_val);
    }
}

/* os.path.normcase on Windows: nt._path_normcase == lower + '/' -> '\\'. */
static char *uf_normcase(const char *s) {
    char *out = strdup(s ? s : "");
    for (char *p = out; *p; p++) {
        if (*p == '/') *p = '\\';
        else *p = (char)tolower((unsigned char)*p);
    }
    return out;
}

static void uf_push_norm(UFList *l, const char *s) {
    char *k = uf_normcase(s);
    uf_push(l, k);
    free(k);
}

/* dict.fromkeys ordering: first occurrence wins. */
static void uf_push_unique(UFList *l, const char *s) {
    if (!uf_has(l, s)) uf_push(l, s);
}

static struct json_t *uf_to_json(const UFList *l) {
    struct json_t *a = json_array();
    for (size_t i = 0; i < l->n; i++) json_append(a, json_string(l->v[i]));
    return a;
}

/* ---- copies() ---------------------------------------------------------- */

typedef struct { char *key; UFList copied; } UFSwap;

struct json_t *w5_userfacing_57dafa_copies(const struct json_t *dirs_j, int applies,
                                           const struct json_t *sealed_map,
                                           const struct json_t *copy_map) {
    UFList dirs; memset(&dirs, 0, sizeof(dirs));
    uf_from_json(&dirs, dirs_j);
    if (!applies) {
        struct json_t *out = uf_to_json(&dirs);
        uf_free(&dirs);
        return out;
    }

    UFSwap *swaps = NULL; size_t nsw = 0, cap = 0;
    const char *names[] = { "npm", "node", "python" };  /* COPIED */
    for (size_t ni = 0; ni < 3; ni++) {
        UFList sealed; memset(&sealed, 0, sizeof(sealed));
        uf_from_json(&sealed, json_obj_get(sealed_map, names[ni]));
        if (sealed.n) {
            UFList copied; memset(&copied, 0, sizeof(copied));
            uf_from_json(&copied, json_obj_get(copy_map, names[ni]));
            if (copied.n == 0) {   /* `if copied:` guards swap.update in Python */
                uf_free(&copied); uf_free(&sealed);
                continue;
            }
            for (size_t si = 0; si < sealed.n; si++) {
                char *k = uf_normcase(sealed.v[si]);
                size_t found = nsw;
                for (size_t j = 0; j < nsw; j++) if (!strcmp(swaps[j].key, k)) { found = j; break; }
                if (found == nsw) {
                    if (nsw == cap) {
                        size_t nc = cap ? cap * 2 : 4;
                        UFSwap *tmp = (UFSwap *)realloc(swaps, nc * sizeof(UFSwap));
                        if (!tmp) break;
                        swaps = tmp; cap = nc;
                    }
                    memset(&swaps[nsw], 0, sizeof(UFSwap));
                    swaps[nsw].key = strdup(k);
                    nsw++;
                } else {
                    uf_free(&swaps[found].copied);
                    memset(&swaps[found].copied, 0, sizeof(UFList));
                }
                for (size_t ci = 0; ci < copied.n; ci++)
                    uf_push(&swaps[found].copied, copied.v[ci]);
                free(k);
            }
            uf_free(&copied);
        }
        uf_free(&sealed);
    }

    UFList out; memset(&out, 0, sizeof(out));
    for (size_t i = 0; i < dirs.n; i++) {
        char *k = uf_normcase(dirs.v[i]);
        size_t found = nsw;
        for (size_t j = 0; j < nsw; j++) if (!strcmp(swaps[j].key, k)) { found = j; break; }
        free(k);
        if (found < nsw) {
            for (size_t j = 0; j < swaps[found].copied.n; j++)
                uf_push_unique(&out, swaps[found].copied.v[j]);
        } else {
            uf_push_unique(&out, dirs.v[i]);
        }
    }
    struct json_t *result = uf_to_json(&out);
    uf_free(&out);
    for (size_t j = 0; j < nsw; j++) { free(swaps[j].key); uf_free(&swaps[j].copied); }
    free(swaps);
    uf_free(&dirs);
    return result;
}

/* ---- user_facing_path() ------------------------------------------------ */

char *w5_userfacing_57dafa_user_facing_path(const char *path,
                                            const struct json_t *store_j,
                                            const struct json_t *ours_j,
                                            const struct json_t *python_copies_j,
                                            const struct json_t *stub_flags) {
    const char *p = path ? path : "";
    UFList store; memset(&store, 0, sizeof(store));
    uf_from_json(&store, store_j);
    if (store.n == 0) return strdup(p);

    UFList ours, python_copies;
    memset(&ours, 0, sizeof(ours));
    memset(&python_copies, 0, sizeof(python_copies));
    uf_from_json(&ours, ours_j);
    uf_from_json(&python_copies, python_copies_j);

    UFList keys; memset(&keys, 0, sizeof(keys));
    for (size_t i = 0; i < store.n; i++) uf_push_norm(&keys, store.v[i]);
    for (size_t i = 0; i < ours.n; i++) uf_push_norm(&keys, ours.v[i]);

    /* kept = [p for p in path.split(os.pathsep) if p and normcase(p) not in keys] */
    UFList kept; memset(&kept, 0, sizeof(kept));
    const char *s = p;
    while (1) {
        const char *c = strchr(s, ';');
        size_t n = c ? (size_t)(c - s) : strlen(s);
        if (n) {
            char *part = (char *)malloc(n + 1);
            memcpy(part, s, n); part[n] = '\0';
            char *k = uf_normcase(part);
            if (!uf_has(&keys, k)) uf_push(&kept, part);
            free(k); free(part);
        }
        if (!c) break;
        s = c + 1;
    }

    UFList python; memset(&python, 0, sizeof(python));
    for (size_t i = 0; i < python_copies.n; i++) uf_push_norm(&python, python_copies.v[i]);

    /* cut = first index whose directory is the Store stub */
    size_t cut = kept.n;
    for (size_t i = 0; i < kept.n; i++) {
        char *k = uf_normcase(kept.v[i]);
        struct json_t *v = json_obj_get(stub_flags, k);
        if (v && v->type == JSON_BOOL && v->bool_val) { cut = i; free(k); break; }
        free(k);
    }

    UFList early, late, all;
    memset(&early, 0, sizeof(early));
    memset(&late, 0, sizeof(late));
    memset(&all, 0, sizeof(all));
    for (size_t i = 0; i < ours.n; i++) {
        char *k = uf_normcase(ours.v[i]);
        int in_python = uf_has(&python, k);
        free(k);
        if (in_python) uf_push(&early, ours.v[i]);
        else uf_push(&late, ours.v[i]);
    }
    for (size_t i = 0; i < cut; i++) uf_push_unique(&all, kept.v[i]);
    for (size_t i = 0; i < early.n; i++) uf_push_unique(&all, early.v[i]);
    for (size_t i = cut; i < kept.n; i++) uf_push_unique(&all, kept.v[i]);
    for (size_t i = 0; i < late.n; i++) uf_push_unique(&all, late.v[i]);

    size_t total = 1;
    for (size_t i = 0; i < all.n; i++) total += strlen(all.v[i]) + 1;
    char *out = (char *)malloc(total);
    if (out) {
        out[0] = '\0';
        for (size_t i = 0; i < all.n; i++) {
            if (i) strcat(out, ";");
            strcat(out, all.v[i]);
        }
    }
    uf_free(&all); uf_free(&early); uf_free(&late);
    uf_free(&python); uf_free(&kept); uf_free(&keys);
    uf_free(&python_copies); uf_free(&ours); uf_free(&store);
    return out ? out : strdup("");
}
