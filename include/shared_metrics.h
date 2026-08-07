/*
 * shared_metrics.h — Port of hermes_cli/observability/shared_metrics.py.
 */
#ifndef SHARED_METRICS_H
#define SHARED_METRICS_H

#include <stdbool.h>
#include <stddef.h>

#include "hermes_json.h"  /* json_t (opaque struct) */

struct sm_datetime {
    int year, mon, day, hour, min, sec;
};

typedef struct {
    char *database_path;
    char *outbox_directory;
} shared_metrics_store_t;

/* PoP: _utc_now @ hermes_cli/observability/shared_metrics.py:_utc_now */
void sm_utc_now(struct sm_datetime *out);

/* PoP: _isoformat @ hermes_cli/observability/shared_metrics.py:_isoformat
 * Caller frees. */
char *sm_isoformat(const struct sm_datetime *dt);

/* PoP: _ensure_private_directory @ hermes_cli/observability/shared_metrics.py:_ensure_private_directory */
int sm_ensure_private_directory(const char *path);

/* PoP: _ensure_private_file @ hermes_cli/observability/shared_metrics.py:_ensure_private_file */
int sm_ensure_private_file(const char *path);

/* PoP: __init__ @ hermes_cli/observability/shared_metrics.py:__init__
 * Caller frees via shared_metrics_store_free. */
shared_metrics_store_t *shared_metrics_store_init(const char *hermes_home);
void shared_metrics_store_free(shared_metrics_store_t *store);

/* PoP: record_counter @ hermes_cli/observability/shared_metrics.py:record_counter */
int sm_record_counter(shared_metrics_store_t *store,
                        const char *metric_name, const char *dimensions_json,
                        const char *hermes_version);

/* PoP: record_model_call @ hermes_cli/observability/shared_metrics.py:record_model_call */
int sm_record_model_call(shared_metrics_store_t *store, json_t *dimensions,
                          const char *hermes_version);

/* PoP: _pending_period_count @ hermes_cli/observability/shared_metrics.py:_pending_period_count */
int sm_pending_period_count(const char *db_path);

/* PoP: _create_package @ hermes_cli/observability/shared_metrics.py:_create_package
 * Returns malloc'd payload JSON or NULL. Caller frees. */
char *sm_create_package(shared_metrics_store_t *store);

/* PoP: _create_pending_packages_if_due @ hermes_cli/observability/shared_metrics.py:_create_pending_packages_if_due */
int sm_create_pending_packages_if_due(shared_metrics_store_t *store);

/* PoP: _export_pending_packages @ hermes_cli/observability/shared_metrics.py:_export_pending_packages */
int sm_export_pending_packages(shared_metrics_store_t *store);

/* PoP: _prune_expired_history @ hermes_cli/observability/shared_metrics.py:_prune_expired_history */
int sm_prune_expired_history(shared_metrics_store_t *store);

/* PoP: _export_and_prune @ hermes_cli/observability/shared_metrics.py:_export_and_prune */
int sm_export_and_prune(shared_metrics_store_t *store);

/* PoP: create_and_export_package @ hermes_cli/observability/shared_metrics.py:create_and_export_package */
int sm_create_and_export_package(shared_metrics_store_t *store);

/* PoP: create_and_export_package_if_due @ hermes_cli/observability/shared_metrics.py:create_and_export_package_if_due */
int sm_create_and_export_package_if_due(shared_metrics_store_t *store);

/* PoP: counter_snapshot @ hermes_cli/observability/shared_metrics.py:counter_snapshot
 * Returns malloc'd JSON array. Caller frees. */
json_t *sm_counter_snapshot(shared_metrics_store_t *store);

/* PoP: counter_dimensions_are_valid @ hermes_cli/observability/shared_metrics_contract.py:counter_dimensions_are_valid */
bool sm_counter_dimensions_are_valid(const char *metric, json_t *dims);

#endif /* SHARED_METRICS_H */
