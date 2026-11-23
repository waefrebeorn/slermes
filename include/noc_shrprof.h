#ifndef NOC_SHRPROF_H
#define NOC_SHRPROF_H
#include <stddef.h>

/* C port of hermes_cli/shared_profile_warning.py.
 * PoP: shrprof_shared_profile_warning @ hermes_cli/shared_profile_warning.py:shared_profile_warning
 *
 * One ledger entry as returned by ledger_entries(). has_* flags model
 * dict.get() presence; a missing/empty install is falsy in Python.
 */
typedef struct {
    int has_install;
    const char *install;
    int has_home;
    const char *hermes_home;
} shrprof_ledger_entry_t;

const char *shrprof_shared_profile_warning(const char *own_install,
                                           const char *home_key,
                                           const shrprof_ledger_entry_t *entries,
                                           size_t count);

#endif /* NOC_SHRPROF_H */
