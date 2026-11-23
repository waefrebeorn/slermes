#ifndef NOC_W6_CARRIED_EXTR_6BDDA2_H
#define NOC_W6_CARRIED_EXTR_6BDDA2_H
#include <stddef.h>
/* Borrowed-pointer string list; free with w6_carriedextr_6bdda2_list_free. */
typedef struct { const char **v; size_t n; } w6_carriedextr_6bdda2_list;
/* (extras, carried, automatic) with a pre-PM tree's old-venv baseline unioned in.
 * The baseline is supplied pre-resolved (pm.extras baseline lookup is a gate seam). */
/* PoP: w6_carriedextr_6bdda2_with_pre_pm_baseline @ pm/carried_extras.py:with_pre_pm_baseline */
void w6_carriedextr_6bdda2_with_pre_pm_baseline(
    const char *const *extras, size_t n_extras,
    const char *const *carried, size_t n_carried,
    const char *const *automatic, size_t n_auto,
    const char *const *baseline, size_t n_base,
    int plugins_none,
    w6_carriedextr_6bdda2_list *out_extras,
    w6_carriedextr_6bdda2_list *out_carried,
    w6_carriedextr_6bdda2_list *out_auto);
void w6_carriedextr_6bdda2_list_free(w6_carriedextr_6bdda2_list *l);
#endif
