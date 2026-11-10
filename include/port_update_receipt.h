/*
 * port_update_receipt.h -- pure helpers ported from
 * hermes_cli/update_receipt.py.
 *
 * The rest of the module is ContextVar plumbing and durable-filesystem
 * bookkeeping; those stay in Python.
 */
#ifndef PORT_UPDATE_RECEIPT_H
#define PORT_UPDATE_RECEIPT_H

#include <stddef.h>

/* _str_records -- dict entries reduced to stringified `keys`. Non-dict entries
 * are skipped. `entries` is a NULL-terminated list of flat [key, value, ...]
 * arrays; `keys` a NULL-terminated array of field names. When `want_pid`, an
 * integer `pid` field is emitted first.
 * Returns a malloc'd array of records; each record is a NULL-terminated array
 * of malloc'd strings. Caller frees every string, every record and the array. */
char ***ur_str_records(const char *const *const *entries,
                       const char *const *keys, int want_pid,
                       int *out_count);

/* _run_file -- the archive filename for one run:
 * update_<YYYYMMDD>_<HHMMSS>_<pid>_<update_id>.json.
 * `now` is the strftime("%Y%m%d%H%M%S") fallback; NULL uses the real clock.
 * Caller frees. */
char *ur_run_filename(const char *directory, const char *const *data,
                      const char *now, long default_pid);

/* _launcher_correlation_id -- HERMES_UPDATE_CORRELATION_ID, stripped, or NULL
 * when absent/empty. `env` is a flat [key, value, ...] array; NULL reads the
 * live environment. Caller frees. */
char *ur_launcher_corr_id(const char *const *env);

#endif /* PORT_UPDATE_RECEIPT_H */
