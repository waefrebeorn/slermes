#ifndef NOC_W6_INSTALLSTAT_B9E0B6_H
#define NOC_W6_INSTALLSTAT_B9E0B6_H
#include <stddef.h>
/* _provably_deleted: root missing AND (under data root | parent .worktrees exists). */
/* PoP: w6_installstat_b9e0b6_provably_deleted @ pm/install_states.py:_provably_deleted */
int w6_installstat_b9e0b6_provably_deleted(int root_exists, int is_relative_to_data,
                                          int parent_is_worktrees, int parent_is_dir);
/* _is_orphan: provably_deleted pre-resolved; retention -1 means None. */
/* PoP: w6_installstat_b9e0b6_is_orphan @ pm/install_states.py:_is_orphan */
int w6_installstat_b9e0b6_is_orphan(int state_is_dir, int record_is_file, const char *record_root,
                                    int root_exists, int provably_deleted,
                                    int retention_set, double age_seconds, double retention);
/* Store-wide retention seconds; -1 represents None. value kind:
 * 0=null/non-numeric, 1=number, 2=string, 3=bool. */
/* PoP: w6_installstat_b9e0b6_retention_seconds @ pm/install_states.py:_retention_seconds */
double w6_installstat_b9e0b6_retention_seconds(int kind, const char *val);
#endif
