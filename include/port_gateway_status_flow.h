/*
 * port_gateway_status_flow.h — gateway/status.py liveness/status FLOW cores
 * and leaves (LIVE authority: C:/Users/eman5/hermes-agent/gateway/status.py).
 *
 * Ports the DECISION sequences of the gateway liveness/status probe flows:
 * the record->pid->start-time reuse-guard verdict (_live_pid_from_record),
 * stale takeover/planned-stop marker reading (_read_live_pid_marker), the
 * multiplex/host scope rungs (multiplexer_liveness_for_profile,
 * live_gateway_pid_for_home, _host_gateway_serves_home), the runtime-status
 * merge/publish chain (_prepare_runtime_status_update, publish_runtime_status,
 * _apply_set_fields, _coerce_session_store, _emit_runtime_status_transition),
 * the lock probe (_probe_lock_file), the verified-owner terminate ladder
 * (_terminate_verified_owner), the on-disk merge (_merge_over_on_disk), and
 * the status-rendering / stale-detection leaves (_start_times_conflict,
 * _pid_marker_names_self, runtime_status_heartbeat_age_s,
 * profile_platforms_from_multiplexer, profile_name_for_home).
 *
 * SEAM DOCTRINE: every live effect (pid/lock/process probes, file reads,
 * record builders, clock, foreign-module imports, writer/owner/topology
 * method calls) enters through ONE universal seam:
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
 * exception (propagated or caught exactly as the live def does). Stateful
 * flows mutate an injected json_t* state object in place.
 *
 * Seam name table (name -> live call; args key order is part of the trace):
 *   pid_from_record        _pid_from_record(record, key)   args{record?,key}
 *   pid_exists             _pid_exists(pid)                args{pid}
 *   get_process_start_time _get_process_start_time(pid)    args{pid}
 *   read_json_file         _read_json_file(path)           args{path}
 *   marker_is_stale        _marker_is_stale(written_at,ttl_s) args{written_at,ttl_s}
 *   update_pause_on_disk   _update_pause_on_disk()         args{}
 *   unlink_quietly         _unlink_quietly(path)           args{path}
 *   get_running_pid_cached get_running_pid_cached(pid_path,cleanup_stale=) args{pid_path,cleanup_stale}
 *   get_runtime_status_running_pid get_runtime_status_running_pid(runtime,expected_home=) args{runtime?,expected_home?}
 *   read_runtime_status    read_runtime_status(path)       args{path}
 *   build_pid_record       _build_pid_record()             args{}
 *   build_runtime_status_record _build_runtime_status_record() args{}
 *   utc_now_iso            _utc_now_iso()                  args{}
 *   get_code_identity_fields _get_code_identity_fields()   args{}
 *   get_runtime_status_path _get_runtime_status_path()     args{}
 *   parse_active_agents    parse_active_agents(raw)        args{raw}
 *   scoped_lock_owner_state _scoped_lock_owner_state(owner_pid,owner_start_time) args{owner_pid,owner_start_time}
 *   terminate_pid          terminate_pid(pid,force=,expected_start_time=) args{pid,force,expected_start_time?}
 *   wait_for_scoped_lock_owner_exit _wait_for_scoped_lock_owner_exit(...) args{owner_pid,owner_start_time,attempts,delay}
 *   try_acquire_file_lock  _try_acquire_file_lock(handle)  args{handle}
 *   release_file_lock      _release_file_lock(handle)      args{handle}
 *   handle.close           handle.close()                  args{handle}
 *   profile_name_for_home  profile_name_for_home(profile_home) args{profile_home}
 *   profile_name_for_home_priv _profile_name_for_home(home) args{profile_home}
 *   same_hermes_home       _same_hermes_home(left,right)   args{left,right}
 *   shared_listener_mirror_platforms shared_listener_mirror_platforms(runtime,profile) args{runtime?,profile}
 *   normalize_updated_at   normalize_updated_at(value)     args{value?}
 *   getpid                 os.getpid()                     args{}
 *   datetime_now           datetime.now(UTC)               args{} ret epoch seconds
 *   path_resolve           Path.resolve()                  args{path} ret resolved path
 *   host_gateway           gateway.host_attach.host_gateway() args{}
 *   host_profile_name_for_home gateway.host_attach.profile_name_for_home(ph) args{profile_home}
 *   owner.serves           host_attach owner.serves(name)  args{name}
 *   host_gateway_topology  gateway.host_topology.host_gateway_topology() args{}
 *   topology.serves        topology.serves(name)           args{name}
 *   named_profile_served_by_running_multiplexer hermes_cli.gateway....(name) args{name}
 *   live_default_gateway_pid hermes_cli.gateway_multiplex_served.live_default_gateway_pid() args{}
 *   get_default_hermes_root hermes_constants.get_default_hermes_root() args{}
 *   get_runtime_status_writer _get_runtime_status_writer() args{}
 *   writer.submit          writer.submit(path,payload)     args{path,payload}
 *   emit_runtime_status_transition agent.monitoring.gateway_health.emit_runtime_status_transition(prev,payload) args{previous_payload,payload}
 *
 * Injected module-state object `st` for prepare/publish (mirrors
 * _runtime_status_state_path / _runtime_status_state):
 *   {"state_path": str|null, "state": obj|null}   mutated in place.
 *
 * Internal (NOT seamed; real on both sides): _start_times_conflict,
 * _apply_set_fields, _coerce_session_store, _prepare_runtime_status_update
 * (inside publish), _emit_runtime_status_transition (inside publish).
 */
#ifndef PORT_GATEWAY_STATUS_FLOW_H
#define PORT_GATEWAY_STATUS_FLOW_H

#include <stdbool.h>
#include <stddef.h>
#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gsf_trace { json_t *events; } gsf_trace_t;
void gsf_trace_add(gsf_trace_t *t, json_t *ev);
void gsf_trace_free(gsf_trace_t *t);

typedef struct gsf_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gsf_seams_t;

json_t *gsf_call(const gsf_seams_t *s, const char *name, const json_t *args);
const char *gsf_raise_class(const json_t *r);
bool gsf_is_raise(const json_t *r);

/* _live_pid_from_record(record) -> pid | None | raise */
/* PoP: gsf_flow_live_pid_from_record @ gateway/status.py:_live_pid_from_record */
json_t *gsf_flow_live_pid_from_record(const json_t *record, const gsf_seams_t *s);

/* _read_live_pid_marker(path, ttl_s) -> [record,target_pid,target_start_time] | None */
/* PoP: gsf_flow_read_live_pid_marker @ gateway/status.py:_read_live_pid_marker */
json_t *gsf_flow_read_live_pid_marker(const char *path, double ttl_s, const gsf_seams_t *s);

/* multiplexer_liveness_for_profile(profile_dir) -> [pid, runtime] | None */
/* PoP: gsf_flow_multiplexer_liveness @ gateway/status.py:multiplexer_liveness_for_profile */
json_t *gsf_flow_multiplexer_liveness(const char *profile_dir, const gsf_seams_t *s);

/* live_gateway_pid_for_home(home) -> pid | None */
/* PoP: gsf_flow_live_gateway_pid_for_home @ gateway/status.py:live_gateway_pid_for_home */
json_t *gsf_flow_live_gateway_pid_for_home(const char *home, const gsf_seams_t *s);

/* _prepare_runtime_status_update(**fields) -> [path,payload,previous]; mutates st */
/* PoP: gsf_flow_prepare_runtime_status @ gateway/status.py:_prepare_runtime_status_update */
json_t *gsf_flow_prepare_runtime_status(json_t *st, const json_t *fields, const gsf_seams_t *s);

/* publish_runtime_status(**fields) -> generation; mutates st */
/* PoP: gsf_flow_publish_runtime_status @ gateway/status.py:publish_runtime_status */
json_t *gsf_flow_publish_runtime_status(json_t *st, const json_t *fields, const gsf_seams_t *s);

/* _probe_lock_file(handle) -> bool held */
/* PoP: gsf_flow_probe_lock_file @ gateway/status.py:_probe_lock_file */
json_t *gsf_flow_probe_lock_file(const char *handle, const gsf_seams_t *s);

/* _terminate_verified_owner(owner_pid, owner_start_time, graceful_attempts=, force_attempts=) -> pid | None */
/* PoP: gsf_flow_terminate_verified_owner @ gateway/status.py:_terminate_verified_owner */
json_t *gsf_flow_terminate_verified_owner(double owner_pid, double owner_start_time,
                                          double graceful_attempts, double force_attempts,
                                          const gsf_seams_t *s);

/* _host_gateway_serves_home(pid, profile_home) -> bool */
/* PoP: gsf_flow_host_gateway_serves_home @ gateway/status.py:_host_gateway_serves_home */
json_t *gsf_flow_host_gateway_serves_home(double pid, const char *profile_home, const gsf_seams_t *s);

/* _merge_over_on_disk(path, payload) -> merged dict */
/* PoP: gsf_flow_merge_over_on_disk @ gateway/status.py:_merge_over_on_disk */
json_t *gsf_flow_merge_over_on_disk(const char *path, const json_t *payload, const gsf_seams_t *s);

/* _start_times_conflict(recorded_start, current_start) -> bool (pure) */
/* PoP: gsf_start_times_conflict @ gateway/status.py:_start_times_conflict */
json_t *gsf_start_times_conflict(const json_t *recorded, const json_t *current);

/* _pid_marker_names_self(target_pid, target_start_time) -> bool */
/* PoP: gsf_pid_marker_names_self @ gateway/status.py:_pid_marker_names_self */
json_t *gsf_pid_marker_names_self(double target_pid, const json_t *target_start_time, const gsf_seams_t *s);

/* runtime_status_heartbeat_age_s(record) -> int | None */
/* PoP: gsf_runtime_status_heartbeat_age_s @ gateway/status.py:runtime_status_heartbeat_age_s */
json_t *gsf_runtime_status_heartbeat_age_s(const json_t *record, const gsf_seams_t *s);

/* _coerce_session_store(session_store) -> {"status": str} (pure) */
/* PoP: gsf_coerce_session_store @ gateway/status.py:_coerce_session_store */
json_t *gsf_coerce_session_store(const json_t *session_store);

/* profile_platforms_from_multiplexer(runtime, profile) -> dict */
/* PoP: gsf_profile_platforms_from_multiplexer @ gateway/status.py:profile_platforms_from_multiplexer */
json_t *gsf_profile_platforms_from_multiplexer(const json_t *runtime, const char *profile, const gsf_seams_t *s);

/* profile_name_for_home(profile_home) -> str | None */
/* PoP: gsf_profile_name_for_home @ gateway/status.py:profile_name_for_home */
json_t *gsf_profile_name_for_home(const char *profile_home, const gsf_seams_t *s);

/* _apply_set_fields(target, fields) -> None; mutates target.
 * fields: array of {"key": str, "value"?: any (absent == _UNSET), "coerce"?: tag} */
/* PoP: gsf_apply_set_fields @ gateway/status.py:_apply_set_fields */
json_t *gsf_apply_set_fields(json_t *target, const json_t *fields, const gsf_seams_t *s);

/* _emit_runtime_status_transition(previous_payload, payload) -> None (suppresses Exception) */
/* PoP: gsf_emit_runtime_status_transition @ gateway/status.py:_emit_runtime_status_transition */
json_t *gsf_emit_runtime_status_transition(const json_t *previous, const json_t *payload, const gsf_seams_t *s);

#ifdef __cplusplus
}
#endif
#endif /* PORT_GATEWAY_STATUS_FLOW_H */
