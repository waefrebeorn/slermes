/* noc_w19_maintenanc_c262d4.h - public C API for persistence/sqlite/admin/_maintenance.py port. */
#ifndef NOC_W19_MAINTENANC_C262D4_H
#define NOC_W19_MAINTENANC_C262D4_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_maintenanc_c262d4_next_chunk_rows @ persistence/sqlite/admin/_maintenance.py:_FtsStorageOptimizer._next_chunk_rows
 * Rows for the next chunk so it costs about target_s: scale = target_s / max(cost_s, 1e-4),
 * at most double/half per step, clamped to [50, 100000]. */
json_t *w19_maintenanc_c262d4_next_chunk_rows(double rows, double cost_s, double target_s);

/* PoP: w19_maintenanc_c262d4_teardown_info @ persistence/sqlite/admin/_maintenance.py:_TeardownProgress.info
 * {"phase":"teardown","percent":min(100,100*drained//total) or 100,"indexed":drained,
 *  "total":total,"table":table,"table_index":table_index,"table_count":table_count}. */
json_t *w19_maintenanc_c262d4_teardown_info(const char *table, long long table_index,
                                            long long table_count, long long total,
                                            long long drained);

/* PoP: w19_maintenanc_c262d4_vacuum_info @ persistence/sqlite/admin/_maintenance.py:_VacuumProgress.info
 * {"phase":"vacuum","percent":0,"indexed":0,"total":0,"elapsed":elapsed,
 *  "size_bytes":size_bytes,"estimated_bytes":estimated_bytes}. */
json_t *w19_maintenanc_c262d4_vacuum_info(double size_bytes, double estimated_bytes,
                                          double elapsed);

/* PoP: w19_maintenanc_c262d4_logical_size @ persistence/sqlite/admin/_maintenance.py:MaintenanceAdmin.logical_size_bytes
 * page_count * page_size; JSON null when the page PRAGMAs could not be read (`ok` false). */
json_t *w19_maintenanc_c262d4_logical_size(double page_count, double page_size, int ok);

/* PoP: w19_maintenanc_c262d4_freelist_ratio @ persistence/sqlite/admin/_maintenance.py:MaintenanceAdmin.freelist_ratio
 * freelist_count / page_count (0.0 for an empty page count); null when unreadable. */
json_t *w19_maintenanc_c262d4_freelist_ratio(double page_count, double freelist_count, int ok);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_MAINTENANC_C262D4_H */
