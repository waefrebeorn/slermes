/*
 * port_working_diff.h — C port of tools/working_diff.py
 *
 * Working-tree git diff collection shared by the CLI and gateway /diff.
 * Ports: _run, _untracked_files, _untracked_diff, collect_working_diff.
 */
#ifndef SLERMES_PORT_WORKING_DIFF_H
#define SLERMES_PORT_WORKING_DIFF_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool  success;
    char *stat;              /* git diff --stat output ("" when empty) */
    char *diff;              /* full diff, untracked files folded in */
    char **untracked;        /* untracked file paths */
    int   untracked_count;
    bool  empty;             /* no changes at all */
    char *error;             /* failure message when success == false */
} wd_diff_result_t;

/* PoP: collect_working_diff @ tools/working_diff.py:collect_working_diff */
wd_diff_result_t wd_collect_working_diff(const char *cwd, const char *mode,
                                         char *const *paths, int path_count);

void wd_diff_result_free(wd_diff_result_t *res);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_PORT_WORKING_DIFF_H */
