/*
 * update_inventory.h — C11 port of hermes_cli/update_inventory.py
 *
 * Runtime inventory + update plan (#91277 Phase 2): one read-only pass that
 * answers, BEFORE any mutation, what Hermes runtimes are running, how each
 * is deployed, which the update will touch, and how each restarts.
 */
#ifndef SLERMES_UPDATE_INVENTORY_H
#define SLERMES_UPDATE_INVENTORY_H

#include "hermes_json.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RuntimeRecord @ hermes_cli/update_inventory.py:RuntimeRecord */
typedef struct {
    char *kind;            /* gateway | dashboard | serve */
    char *profile;
    long pid;              /* 0 = unknown */
    char supervisor[16];   /* systemd | launchd | desktop | service | manual */
    char *code_sha;
    char *code_version;
    char *restart_via;
} upd_runtime_record_t;

/* UpdatePlan @ ...:UpdatePlan */
typedef struct {
    char install_method[32];
    int updatable_in_place;
    char update_mechanism[128];
    char *expected_sha;
    char *expected_version;
    char **profiles;
    size_t profiles_n;
    upd_runtime_record_t *runtimes;
    size_t runtimes_n;
} upd_plan_t;

void upd_runtime_record_free(upd_runtime_record_t *rec);
void upd_plan_free(upd_plan_t *plan);

/* to_dict @ RuntimeRecord.to_dict / UpdatePlan.to_dict */
json_t *upd_runtime_record_to_dict(const upd_runtime_record_t *rec);
json_t *upd_plan_to_dict(const upd_plan_t *plan);

/* _detect_supervisor_for_pid @ ...:_detect_supervisor_for_pid.
 * service_pids: array of service-managed pids. */
void update_inventory_detect_supervisor_for_pid(
    long pid, const long *service_pids, size_t n, char out[16]);

/* _restart_mechanism @ ...:_restart_mechanism — malloc'd string. */
char *update_inventory_restart_mechanism(const char *supervisor,
                                         const char *profile);

/* collect_runtime_inventory @ ...:collect_runtime_inventory — read-only,
 * never raises. Every collector degrades independently. */
int collect_runtime_inventory(upd_plan_t *out_plan);

/* print_update_plan @ ...:print_update_plan */
void print_update_plan(const upd_plan_t *plan);

/* record_plan_in_receipt @ ...:record_plan_in_receipt — attach to active
 * receipt. Never raises. */
void record_plan_in_receipt(const upd_plan_t *plan);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_UPDATE_INVENTORY_H */
