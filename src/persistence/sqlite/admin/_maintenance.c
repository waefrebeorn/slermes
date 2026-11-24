/*
 * _maintenance.c - real C11 port of upstream Hermes persistence/sqlite/admin/_maintenance.py
 * pure helpers: the adaptive FTS chunk size, the teardown/vacuum progress payloads and the
 * page-stat arithmetic. Everything that opens state.db stays in the Python store.
 * PoP: w19_maintenanc_c262d4_next_chunk_rows @ persistence/sqlite/admin/_maintenance.py:_FtsStorageOptimizer._next_chunk_rows
 * PoP: w19_maintenanc_c262d4_teardown_info @ persistence/sqlite/admin/_maintenance.py:_TeardownProgress.info
 * PoP: w19_maintenanc_c262d4_vacuum_info @ persistence/sqlite/admin/_maintenance.py:_VacuumProgress.info
 * PoP: w19_maintenanc_c262d4_logical_size @ persistence/sqlite/admin/_maintenance.py:MaintenanceAdmin.logical_size_bytes
 * PoP: w19_maintenanc_c262d4_freelist_ratio @ persistence/sqlite/admin/_maintenance.py:MaintenanceAdmin.freelist_ratio
 */
#include "noc_w19_maintenanc_c262d4.h"

#include <math.h>

/* _FtsStorageOptimizer chunk-size bounds. */
static const long long FTS_REBUILD_CHUNK_MIN_ROWS = 50;
static const long long FTS_REBUILD_CHUNK_MAX_ROWS = 100000;
static const double FTS_REBUILD_MIN_COST_S = 1e-4;

json_t *w19_maintenanc_c262d4_next_chunk_rows(double rows, double cost_s, double target_s) {
    double cost = cost_s > FTS_REBUILD_MIN_COST_S ? cost_s : FTS_REBUILD_MIN_COST_S;
    double scale = target_s / cost;
    if (scale > 2.0) scale = 2.0;
    if (scale < 0.5) scale = 0.5;
    /* int(rows * scale): truncation toward zero. */
    long long scaled = (long long)(rows * scale);
    if (scaled > FTS_REBUILD_CHUNK_MAX_ROWS) scaled = FTS_REBUILD_CHUNK_MAX_ROWS;
    if (scaled < FTS_REBUILD_CHUNK_MIN_ROWS) scaled = FTS_REBUILD_CHUNK_MIN_ROWS;
    return json_number((double)scaled);
}

json_t *w19_maintenanc_c262d4_teardown_info(const char *table, long long table_index,
                                            long long table_count, long long total,
                                            long long drained) {
    json_t *out = json_object();
    long long percent = 100;
    if (total != 0) {
        percent = (100 * drained) / total;   /* Python // on non-negative ints */
        if (percent > 100) percent = 100;
    }
    json_set(out, "phase", json_string("teardown"));
    json_set(out, "percent", json_number((double)percent));
    json_set(out, "indexed", json_number((double)drained));
    json_set(out, "total", json_number((double)total));
    json_set(out, "table", json_string(table ? table : ""));
    json_set(out, "table_index", json_number((double)table_index));
    json_set(out, "table_count", json_number((double)table_count));
    return out;
}

json_t *w19_maintenanc_c262d4_vacuum_info(double size_bytes, double estimated_bytes,
                                          double elapsed) {
    json_t *out = json_object();
    json_set(out, "phase", json_string("vacuum"));
    json_set(out, "percent", json_number(0));
    json_set(out, "indexed", json_number(0));
    json_set(out, "total", json_number(0));
    json_set(out, "elapsed", json_number(elapsed));
    json_set(out, "size_bytes", json_number(size_bytes));
    json_set(out, "estimated_bytes", json_number(estimated_bytes));
    return out;
}

json_t *w19_maintenanc_c262d4_logical_size(double page_count, double page_size, int ok) {
    if (!ok) return json_null();
    return json_number(page_count * page_size);
}

json_t *w19_maintenanc_c262d4_freelist_ratio(double page_count, double freelist_count, int ok) {
    if (!ok) return json_null();
    if (page_count > 0) return json_number(freelist_count / page_count);
    return json_number(0.0);
}
