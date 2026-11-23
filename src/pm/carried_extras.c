/*
 * carried_extras.c - Pre-PM baseline union helper (C11 port).
 *
 * PoP: w6_carriedextr_6bdda2_with_pre_pm_baseline @ pm/carried_extras.py:with_pre_pm_baseline
 *
 * The live function's only non-pure step is resolving the pre-PM baseline from
 * the package tree; the gate supplies that baseline as a pre-resolved string
 * list (same shape as pm.extras.pre_pm_baseline / committed_generation_extras).
 */
#include "noc_w6_carriedextr_6bdda2.h"

#include <stdlib.h>
#include <string.h>

static int contains(const char *const *a, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++) if (a[i] && !strcmp(a[i], s)) return 1;
    return 0;
}
static int cmp_str(const void *a, const void *b) {
    const char *x = *(const char *const *)a, *y = *(const char *const *)b;
    return strcmp(x, y);
}
static void build_concat(const char *const *a, size_t na, const char *const *b, size_t nb,
                         int skip_all_from_b, w6_carriedextr_6bdda2_list *out) {
    const char **p = (na + nb) ? (const char **)malloc((na + nb) * sizeof(const char *)) : NULL;
    size_t k = 0;
    for (size_t i = 0; i < na; i++) p[k++] = a[i];
    for (size_t i = 0; i < nb; i++) {
        if (skip_all_from_b && b[i] && !strcmp(b[i], "all")) continue;
        p[k++] = b[i];
    }
    out->v = p; out->n = k;
}
void w6_carriedextr_6bdda2_with_pre_pm_baseline(
    const char *const *extras, size_t n_extras,
    const char *const *carried, size_t n_carried,
    const char *const *automatic, size_t n_auto,
    const char *const *baseline, size_t n_base,
    int plugins_none,
    w6_carriedextr_6bdda2_list *out_extras,
    w6_carriedextr_6bdda2_list *out_carried,
    w6_carriedextr_6bdda2_list *out_auto) {
    const char **added = n_base ? (const char **)malloc(n_base * sizeof(const char *)) : NULL;
    size_t n_added = 0;
    for (size_t i = 0; i < n_base; i++)
        if (!contains(extras, n_extras, baseline[i])) added[n_added++] = baseline[i];

    if (n_added == 0) {
        build_concat(extras, n_extras, NULL, 0, 0, out_extras);
        build_concat(carried, n_carried, NULL, 0, 0, out_carried);
        build_concat(automatic, n_auto, NULL, 0, 0, out_auto);
        free(added);
        return;
    }
    /* extras: sorted(set(extras + added)) */
    const char **u = (const char **)malloc((n_extras + n_added) * sizeof(const char *));
    size_t k = 0;
    for (size_t i = 0; i < n_extras; i++) if (!contains(u, k, extras[i])) u[k++] = extras[i];
    for (size_t i = 0; i < n_added; i++) if (!contains(u, k, added[i])) u[k++] = added[i];
    qsort(u, k, sizeof(const char *), cmp_str);
    out_extras->v = u; out_extras->n = k;

    /* carried: carried + (added except "all") when there is no plugin input */
    if (plugins_none) build_concat(carried, n_carried, added, n_added, 1, out_carried);
    else build_concat(carried, n_carried, NULL, 0, 0, out_carried);
    /* automatic: automatic + added */
    build_concat(automatic, n_auto, added, n_added, 0, out_auto);
    free(added);
}
void w6_carriedextr_6bdda2_list_free(w6_carriedextr_6bdda2_list *l) {
    if (!l) return;
    free(l->v); l->v = NULL; l->n = 0;
}

