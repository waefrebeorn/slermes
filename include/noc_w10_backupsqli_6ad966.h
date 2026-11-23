#ifndef NOC_W10_BACKUPSQLI_6AD966_H
#define NOC_W10_BACKUPSQLI_6AD966_H

/* PoP: w10_backupsqli_6ad966_uses_wal @ hermes_cli/backup_sqlite.py:_uses_wal
 * PoP: w10_backupsqli_6ad966_pages_per_second @ hermes_cli/backup_sqlite.py:_CopyRate.pages_per_second
 * PoP: w10_backupsqli_6ad966_rollback_hold @ hermes_cli/backup_sqlite.py:_stepped_rollback_journal_copy */
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* WAL detection from a 19-byte header supplied as a JSON byte array.
 * Mirrors _uses_wal: read(19), len == 19 and header[18] == 2. */
struct json_t *w10_backupsqli_6ad966_uses_wal(const struct json_t *head);

/* _CopyRate.pages_per_second(): pages / max(seconds, 1e-6). */
struct json_t *w10_backupsqli_6ad966_pages_per_second(const struct json_t *pages,
                                                      const struct json_t *seconds);

/* The whole-copy hold estimate from _stepped_rollback_journal_copy:
 * rate.total / max(rate.pages_per_second(), 1.0). */
struct json_t *w10_backupsqli_6ad966_rollback_hold(const struct json_t *total,
                                                   const struct json_t *pages,
                                                   const struct json_t *seconds);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_BACKUPSQLI_6AD966_H */
