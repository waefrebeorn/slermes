/*
 * noc_w32_backup_1d0d4e_p2.h - C surface for upstream
 * persistence/sqlite/admin/_backup.py (gate w32_backup_1d0d4e_p2).
 *
 * The statement and predicate half of the file: the SQL literals the restore
 * and verification run, the WAL header predicate, the integrity-check manifest
 * ({valid, message, size}) and the runtime-epoch staging decision.  Paths,
 * sqlite connections and exception objects are seams fed here as values.
 */
#ifndef NOC_W32_BACKUP_1D0D4E_P2_H
#define NOC_W32_BACKUP_1D0D4E_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The module/rollback budget constants as one object. */
json_t *w32_backup_1d0d4e_p2_constants(void);

/* {"probe": _LEDGER_PROBE}. */
json_t *w32_backup_1d0d4e_p2_ledger_probe(void);

/* {"epoch_ddl","epoch_upsert"} - the two statements of _restore_epoch_source. */
json_t *w32_backup_1d0d4e_p2_epoch_sql(void);

/* {"header_hex"} -> {"wal": bool} (_uses_wal: 19 bytes, byte 18 == 2). */
json_t *w32_backup_1d0d4e_p2_uses_wal(const json_t *req);

/* {"path","check_header","run_pragma","max_bytes","file":{kind,size,header_hex,rows}}
 * -> the {valid, message, size} dict verify_sqlite_integrity returns. */
json_t *w32_backup_1d0d4e_p2_verify_manifest(const json_t *req);

/* {"dst_name","dst_resolved_name","floor","src_epoch"} ->
 * {"stage","action","work_prefix","prepared"}. */
json_t *w32_backup_1d0d4e_p2_epoch_decision(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
