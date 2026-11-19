/*
 * port_gateway_migrate_flow.h — hermes_cli/gateway_migrate.py migration FLOW
 * cores and leaves (LIVE authority: C:/Users/eman5/hermes-agent/hermes_cli/gateway_migrate.py).
 *
 * Ports the DECISION sequences of the multiplex convergence command: the
 * service-record leaves (_service_label, _remove_verb, _service_from_dict,
 * _recorded_services), the manifest validation / resume-target chain
 * (_manifest_secondaries, _target_from_manifest, _manifest_not_yet_served,
 * _read_manifest, _resume_target, _resume_findings), the ownership probe
 * (_own_gateway_pid), the destructive-phase preflight (_preflight_apply), the
 * default-gateway bring-up ladder (_restart_default, _enable_default_at_boot),
 * the secondary removal order (_remove_secondary_gateways, _signalled_gateways),
 * and the two compensating orchestrators (apply_migration, rollback_migration).
 *
 * SEAM DOCTRINE: every live effect (service-manager verbs, config/manifest
 * writes, file reads/unlinks, pid/owner probes, the clock, foreign-module
 * calls and print) enters through ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` borrowed JSON object;
 * result NEW json_t* (NULL == None, never a JSON_NULL node);
 * {"__raise__": cls, "msg": msg} == exception. The harness records
 * {"ev": name, "args": args, "ret": result} per call in order; the oracle's
 * recording stubs emit the identical shape driving LIVE Python with the same
 * scripts. The cores perform NO I/O.
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == Python None; a {"__raise__": cls, "msg": msg} object == a raised
 * exception (propagated or caught exactly as the live def does). Tuples that
 * CONTAIN None are returned as arrays with explicit json_null() elements.
 *
 * Plan representation (mirrors the MigrationPlan dataclass fields, NOT to_dict):
 *   {"default_home": str, "profiles": [ProfileGateway], "multiplex_flag_on": bool,
 *    "live_served": [str]|null, "manifest": obj|null, "interrupted": bool,
 *    "blockers": [str], "notices": [str], "standalone_by_config": [str]}
 * ProfileGateway:
 *   {"name": str, "home": str, "pid": int|null, "services": [[kind, system], ...],
 *    "run_as_user": str|null, "uid": int|null, "runtime_home": str|null}
 * A service tuple is a 2-element array [kind(str), system(bool)].
 *
 * Seam name table (name -> live call; args key order is part of the trace):
 *   print                    print(line) / print()            args{line?}
 *   service_op               _service_op(kind,system,verb,home,run_as_user=) args{kind,system,verb,home,run_as_user?}
 *   stop_gateway_process     _stop_gateway_process(home)      args{home}
 *   spawn_detached_gateway   _spawn_detached_gateway(home)    args{home} ret bool
 *   write_multiplex_flag     _write_multiplex_flag(home,value) args{default_home,value}
 *   atomic_json_write        utils.atomic_json_write(path,data,indent=,separators=) args{path,data,indent?,separators?}
 *   path_exists              Path.exists()                    args{path} ret bool
 *   read_text                Path.read_text(encoding=)        args{path,encoding} ret str
 *   path_unlink              Path.unlink(missing_ok=)         args{path,missing_ok}
 *   profile_is_parked        hermes_cli.profiles.profile_is_parked(home) args{home} ret bool
 *   live_gateway_pid_for_home gateway.status.live_gateway_pid_for_home(home) args{home} ret pid|null
 *   host_gateway             gateway.host_attach.host_gateway() args{} ret {pid,home}|null
 *   same_hermes_home         gateway.status._same_hermes_home(l,r) args{left,right} ret bool
 *   require_readable_config  hermes_cli.config.require_readable_config_before_write(path) args{path}
 *   require_root             gw._require_root_for_system_service(ctx) args{context}
 *   system_service_identity  gw._system_service_identity()    args{}
 *   getpwnam                 pwd.getpwnam(name)               args{name}
 *   recorded_served_profiles hermes_cli.gateway_multiplex_served.recorded_served_profiles(home) args{default_home}
 *   installed_services       _installed_services(home)        args{home} ret [[kind,system],...]
 *   get_default_hermes_root  hermes_constants.get_default_hermes_root() args{} ret path
 *   monotonic                time.monotonic()                 args{} ret seconds
 *   sleep                    time.sleep(s)                    args{seconds}
 *   strftime                 time.strftime(fmt)               args{fmt} ret str
 *   read_runtime_status      gateway.status.read_runtime_status(path) args{path} ret obj|null
 *   home_env_enter           _home_env(home) __enter__        args{home}
 *   home_env_exit            _home_env(home) __exit__         args{home}
 *
 * Internal (NOT seamed; real on both sides): the MigrationPlan / ProfileGateway
 * properties, _service_dict, _recorded_run_as_user, _manifest_path, _print,
 * _live_gateway_pid, _host_gateway_owner, _default_home, _write_manifest,
 * _wait_for_served, _wait_for_live_gateway (agent.deadline.poll_until loop),
 * _reconcile_standalone_runtime, and every ported leaf composed by the flows.
 */
#ifndef PORT_GATEWAY_MIGRATE_FLOW_H
#define PORT_GATEWAY_MIGRATE_FLOW_H

#include <stdbool.h>
#include <stddef.h>
#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gmf_trace { json_t *events; } gmf_trace_t;
void gmf_trace_add(gmf_trace_t *t, json_t *ev);
void gmf_trace_free(gmf_trace_t *t);

typedef struct gmf_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gmf_seams_t;

json_t *gmf_call(const gmf_seams_t *s, const char *name, const json_t *args);
const char *gmf_raise_class(const json_t *r);
bool gmf_is_raise(const json_t *r);

/* _service_label(service) -> str */
/* PoP: gmf_flow_service_label @ hermes_cli/gateway_migrate.py:_service_label */
json_t *gmf_flow_service_label(const json_t *service);

/* _remove_verb(service) -> str */
/* PoP: gmf_flow_remove_verb @ hermes_cli/gateway_migrate.py:_remove_verb */
json_t *gmf_flow_remove_verb(const json_t *service);

/* _service_from_dict(service) -> (kind, system) | None */
/* PoP: gmf_flow_service_from_dict @ hermes_cli/gateway_migrate.py:_service_from_dict */
json_t *gmf_flow_service_from_dict(const json_t *service);

/* _recorded_services(rec) -> [(kind, system), ...] */
/* PoP: gmf_flow_recorded_services @ hermes_cli/gateway_migrate.py:_recorded_services */
json_t *gmf_flow_recorded_services(const json_t *rec);

/* _manifest_secondaries(manifest) -> [rec, ...] | None */
/* PoP: gmf_flow_manifest_secondaries @ hermes_cli/gateway_migrate.py:_manifest_secondaries */
json_t *gmf_flow_manifest_secondaries(const json_t *manifest);

/* _target_from_manifest(manifest) -> (target, run_as_user) */
/* PoP: gmf_flow_target_from_manifest @ hermes_cli/gateway_migrate.py:_target_from_manifest */
json_t *gmf_flow_target_from_manifest(const json_t *manifest);

/* _manifest_not_yet_served(manifest, live_served) -> bool */
/* PoP: gmf_flow_manifest_not_yet_served @ hermes_cli/gateway_migrate.py:_manifest_not_yet_served */
json_t *gmf_flow_manifest_not_yet_served(const json_t *manifest, const json_t *live_served,
                                         const gmf_seams_t *s);

/* _read_manifest(default_home) -> dict | None */
/* PoP: gmf_flow_read_manifest @ hermes_cli/gateway_migrate.py:_read_manifest */
json_t *gmf_flow_read_manifest(const char *default_home, const gmf_seams_t *s);

/* _own_gateway_pid(home, owner) -> pid | None */
/* PoP: gmf_flow_own_gateway_pid @ hermes_cli/gateway_migrate.py:_own_gateway_pid */
json_t *gmf_flow_own_gateway_pid(const char *home, const json_t *owner, const gmf_seams_t *s);

/* _preflight_apply(plan, target, run_as_user) -> refusal str | None */
/* PoP: gmf_flow_preflight_apply @ hermes_cli/gateway_migrate.py:_preflight_apply */
json_t *gmf_flow_preflight_apply(const json_t *plan, const json_t *target,
                                 const char *run_as_user, const gmf_seams_t *s);

/* _restart_default(plan_default, target, default_home, run_as_user=) -> str */
/* PoP: gmf_flow_restart_default @ hermes_cli/gateway_migrate.py:_restart_default */
json_t *gmf_flow_restart_default(const json_t *plan_default, const json_t *target,
                                 const char *default_home, const char *run_as_user,
                                 const gmf_seams_t *s);

/* _enable_default_at_boot(plan) -> failure str | None */
/* PoP: gmf_flow_enable_default_at_boot @ hermes_cli/gateway_migrate.py:_enable_default_at_boot */
json_t *gmf_flow_enable_default_at_boot(const json_t *plan, const gmf_seams_t *s);

/* _remove_secondary_gateways(plan) -> None */
/* PoP: gmf_flow_remove_secondary_gateways @ hermes_cli/gateway_migrate.py:_remove_secondary_gateways */
json_t *gmf_flow_remove_secondary_gateways(const json_t *plan, const gmf_seams_t *s);

/* _resume_target(plan) -> (target, run_as_user) */
/* PoP: gmf_flow_resume_target @ hermes_cli/gateway_migrate.py:_resume_target */
json_t *gmf_flow_resume_target(const json_t *plan);

/* _resume_findings(plan, target, run_as_user) -> [str, ...] */
/* PoP: gmf_flow_resume_findings @ hermes_cli/gateway_migrate.py:_resume_findings */
json_t *gmf_flow_resume_findings(const json_t *plan, const json_t *target,
                                 const char *run_as_user, const gmf_seams_t *s);

/* _signalled_gateways(plan) -> [str, ...] */
/* PoP: gmf_flow_signalled_gateways @ hermes_cli/gateway_migrate.py:_signalled_gateways */
json_t *gmf_flow_signalled_gateways(const json_t *plan);

/* apply_migration(plan, served_wait=) -> bool */
/* PoP: gmf_flow_apply_migration @ hermes_cli/gateway_migrate.py:apply_migration */
json_t *gmf_flow_apply_migration(const json_t *plan, double served_wait, const gmf_seams_t *s);

/* rollback_migration(default_home=None) -> bool */
/* PoP: gmf_flow_rollback_migration @ hermes_cli/gateway_migrate.py:rollback_migration */
json_t *gmf_flow_rollback_migration(const char *default_home, const gmf_seams_t *s);

#ifdef __cplusplus
}
#endif
#endif /* PORT_GATEWAY_MIGRATE_FLOW_H */
