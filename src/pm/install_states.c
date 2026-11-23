/*
 * install_states.c - Orphan install-state predicates (C11 port).
 *
 * PoP: w6_installstat_b9e0b6_provably_deleted @ pm/install_states.py:_provably_deleted
 * PoP: w6_installstat_b9e0b6_is_orphan @ pm/install_states.py:_is_orphan
 * PoP: w6_installstat_b9e0b6_retention_seconds @ pm/install_states.py:_retention_seconds
 *
 * Path/filesystem facts are supplied as pre-resolved scalars from the fixture;
 * retention comes from the resolved config value (default 14 when absent).
 */
#include "noc_w6_installstat_b9e0b6.h"

#include <stdlib.h>
#include <string.h>

static int py_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
int w6_installstat_b9e0b6_provably_deleted(int root_exists, int is_relative_to_data,
                                          int parent_is_worktrees, int parent_is_dir) {
    if (root_exists) return 0;
    return (is_relative_to_data || (parent_is_worktrees && parent_is_dir)) ? 1 : 0;
}
int w6_installstat_b9e0b6_is_orphan(int state_is_dir, int record_is_file, const char *record_root,
                                    int root_exists, int provably_deleted,
                                    int retention_set, double age_seconds, double retention) {
    if (!state_is_dir || !record_is_file) return 0;
    const char *s = record_root ? record_root : "";
    while (*s && py_space((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && py_space((unsigned char)e[-1])) e--;
    if (e == s) return 0;            /* empty .project-root record */
    if (root_exists) return 0;       /* checkout still visible */
    if (provably_deleted) return 1;
    if (retention_set && age_seconds > retention) return 1;
    return 0;
}
double w6_installstat_b9e0b6_retention_seconds(int kind, const char *val) {
    if (kind == 0) return -1;        /* None / non-numeric */
    if (kind == 1) {                 /* int/float config value */
        double d = val ? atof(val) : 0.0;
        if (!(d > 0)) return -1;
        return d * 86400.0;
    }
    if (kind == 2) return -1;        /* string config value */
    if (kind == 3) return (val && !strcmp(val, "1")) ? 86400.0 : -1;  /* bool */
    return -1;
}

