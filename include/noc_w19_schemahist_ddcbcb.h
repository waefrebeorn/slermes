/* noc_w19_schemahist_ddcbcb.h - public C API for persistence/sqlite/admin/_schema_history.py port. */
#ifndef NOC_W19_SCHEMAHIST_DDCBCB_H
#define NOC_W19_SCHEMAHIST_DDCBCB_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_schemahist_ddcbcb_apply @ persistence/sqlite/admin/_schema_history.py:_apply */
json_t *w19_schemahist_ddcbcb_apply(json_t *columns, json_t *edits);
/* PoP: w19_schemahist_ddcbcb_declared_snapshots @ persistence/sqlite/admin/_schema_history.py:declared_snapshots */
json_t *w19_schemahist_ddcbcb_declared_snapshots(const char *table);
/* PoP: w19_schemahist_ddcbcb_current_declared_columns @ persistence/sqlite/admin/_schema_history.py:current_declared_columns */
json_t *w19_schemahist_ddcbcb_current_declared_columns(const char *table);
/* PoP: w19_schemahist_ddcbcb_reachable_physical_layouts @ persistence/sqlite/admin/_schema_history.py:reachable_physical_layouts */
json_t *w19_schemahist_ddcbcb_reachable_physical_layouts(const char *table, json_t *accept, double limit);
/* PoP: w19_schemahist_ddcbcb_schema_history @ persistence/sqlite/admin/_schema_history.py:SCHEMA_HISTORY */
json_t *w19_schemahist_ddcbcb_schema_history(const char *table);
/* PoP: w19_schemahist_ddcbcb_tables @ persistence/sqlite/admin/_schema_history.py:SCHEMA_HISTORY */
json_t *w19_schemahist_ddcbcb_tables(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_SCHEMAHIST_DDCBCB_H */
