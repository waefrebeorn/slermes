/*
 * mem_trim.h — Public API for hermes_cli/mem_trim.py port.
 *
 * Rate-limited heap release: config coercion, /proc/self/status snapshotting,
 * glibc malloc_trim probing, and the trim_memory entry point.
 */
#ifndef MEM_TRIM_H
#define MEM_TRIM_H

#include <stdbool.h>
#include "hermes_json.h"

double mt_cooldown_seconds(const json_t *value);
int    mt_log_every_n(const json_t *value);
double mt_nonnegative_float(const json_t *value, double default_value);

char  *mt_read_proc_status(void);
char  *collect_memory_snapshot(int history_bytes);
bool   mt_should_log_trim(bool force, int log_every_n, int call_count,
                          const char *before_json, const char *after_json,
                          double info_log_min_delta_mb);
bool   mt_probe_glibc_malloc_trim(void);
bool   trim_memory(bool force, const char *reason, double cooldown_seconds);
void   mt_config_settings(int *enabled_out, double *cooldown_out,
                          int *log_every_n_out, double *info_delta_out);

#endif /* MEM_TRIM_H */
