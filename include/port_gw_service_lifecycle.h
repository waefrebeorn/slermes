/*
 * port_gw_service_lifecycle.h — service-lifecycle FLOW cores + high-value leaves
 * of hermes_cli/gateway.py (the gateway CLI / service lifecycle).
 *
 * Ports the DECISION sequences of `hermes gateway install/uninstall/start/stop/
 * restart/status`: the backend-selection ladder (systemd / launchd / windows /
 * none), the installed-service-kind probe, the s6 dispatch fallthroughs, the
 * host-multiplexer ownership checks, the no-backend error/exit-code table, the
 * graceful SIGUSR1 restart observation ladder, and the profile/home resolution
 * leaves they branch on. All live OS / service / registry / subprocess I/O
 * enters through the gwsl_seams_t callbacks below; the decision cores perform
 * NO I/O. Each seam documents its live counterpart.
 *
 * Sequences are observable through a gwsl_trace_t: seam implementations record
 * one JSON event per call (arguments + scripted result) in call order;
 * tests/oracle_gw_service_lifecycle.py compares that ordered event list against
 * recording monkeypatch stubs driving the LIVE Python with the same scripts.
 *
 * sys.exit is modeled as a gwsl_verdict_t (exited + exit_code), never a real
 * exit; an uncaught live exception is modeled as a gwsl_exc_t propagated to the
 * caller. The harness compares ordered trace + ret + exit/raised verdict.
 *
 * JSON values are libjson json_t. Ownership is documented per function.
 */

#ifndef PORT_GW_SERVICE_LIFECYCLE_H
#define PORT_GW_SERVICE_LIFECYCLE_H

#include <stdbool.h>
#include <stddef.h>

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Trace recorder (harness infrastructure, no Python counterpart) ─────────
 * events is a JSON_ARRAY of event objects. gwsl_trace_add takes ownership of
 * ev (frees it when t is NULL). */
typedef struct gwsl_trace {
    json_t *events;   /* owned JSON_ARRAY; NULL until first add */
} gwsl_trace_t;

void gwsl_trace_add(gwsl_trace_t *t, json_t *ev);
void gwsl_trace_free(gwsl_trace_t *t);

/* ── Exit verdict / exception model ────────────────────────────────────────
 * sys.exit(code) -> {exited=true, exit_code=code}. A normal return is
 * {exited=false}. An uncaught live exception is carried in gwsl_exc_t. */
typedef struct gwsl_verdict {
    bool exited;
    int exit_code;
} gwsl_verdict_t;

typedef enum gwsl_exc_kind {
    GWSL_EXC_NONE = 0,
    GWSL_EXC_CALLED_PROCESS,   /* subprocess.CalledProcessError */
    GWSL_EXC_RUNTIME,          /* RuntimeError (incl. SystemScopeRequiresRootError) */
    GWSL_EXC_OSERROR,          /* OSError */
    GWSL_EXC_TIMEOUT,          /* subprocess.TimeoutExpired */
    GWSL_EXC_SYSTEM_EXIT,      /* SystemExit (exit_code carries the code) */
    GWSL_EXC_KEY_ERROR         /* KeyError (missing _NO_BACKEND_MESSAGES key) */
} gwsl_exc_kind_t;

typedef struct gwsl_exc {
    gwsl_exc_kind_t kind;
    int exit_code;             /* SystemExit code */
} gwsl_exc_t;

/* Map a scripted exception type name to the enum (GWSL_EXC_NONE for ""/NULL). */
gwsl_exc_kind_t gwsl_exc_kind_from_name(const char *name);
const char *gwsl_exc_kind_name(gwsl_exc_kind_t k);

/* ── Seams ──────────────────────────────────────────────────────────────────
 * One shared seam table; each flow calls exactly the members the live code
 * path reaches. NULL members are never called on a scripted path that does not
 * reach them. ctx is the harness/oracle script context. Raising seams fill
 * *exc (GWSL_EXC_NONE when the call succeeded).
 *
 * Member -> live counterpart:
 *   supports_systemd      -> supports_systemd_services()
 *   is_macos              -> is_macos()
 *   is_windows            -> is_windows()
 *   is_wsl                -> hermes_constants.is_wsl()
 *   is_container          -> hermes_constants.is_container()
 *   is_termux             -> hermes_constants.is_termux()
 *   is_apt_termux         -> _is_apt_termux_install()
 *   is_supervised         -> tools.process_registry._is_supervised_gateway_process()
 *   detect_service_mgr    -> hermes_cli.service_manager.detect_service_manager() (malloc'd)
 *   systemd_unit_installed-> _systemd_unit_installed()
 *   launchd_plist_exists  -> get_launchd_plist_path().exists()
 *   service_name          -> get_service_name() (borrowed)
 *   scope_label           -> _service_scope_label(system) (borrowed)
 *   current_profile_name  -> _current_profile_name() (borrowed)
 *   get_hermes_home       -> hermes_cli.config.get_hermes_home() (borrowed)
 *   active_profile_name   -> hermes_cli.profiles.get_active_profile_name() (borrowed/NULL)
 *   profile_is_standalone -> hermes_cli.profiles.profile_is_standalone(home)
 *   standalone_notice     -> gateway_multiplex_mode.STANDALONE_DEPRECATION_NOTICE (borrowed)
 *   routing_home          -> hermes_constants.get_routing_process_hermes_home() (borrowed)
 *   named_profile_home    -> hermes_constants.named_profile_home(home) (borrowed/NULL)
 *   service_mgmt_blocked  -> (computed in-core from is_termux/is_apt_termux; NOT a seam)
 *   gw_windows_call       -> _gw_windows().<method>(**kwargs) (ret descriptor; *exc)
 *   systemd_call          -> globals()["systemd_"+verb](**{system}) (*exc)
 *   systemd_status        -> systemd_status(deep, system=, full=) (*exc)
 *   launchd_call          -> globals()["launchd_"+verb]() (*exc)
 *   launchd_install       -> launchd_install(force, start_now=, force_unit_path=) (*exc)
 *   launchd_status        -> launchd_status(deep) (*exc)
 *   is_managed            -> hermes_cli.config.is_managed()
 *   managed_error         -> hermes_cli.config.managed_error(action)
 *   refuses_container     -> refuses_container_user_scope_install(system)
 *   record_service_choice -> gateway_setup_service.record_service_choice(action)
 *   install_systemd_cli   -> _install_systemd_from_cli(args, force=, system=, run_as_user=) (*exc)
 *   profile_lifecycle     -> gateway_profile_lifecycle.profile_lifecycle(action, args)
 *   guard_named_profile   -> _guard_named_profile_under_multiplexer(force=) (*exc)
 *   refuse_sessions_live  -> gateway_lifecycle_guards.refuse_while_sessions_live(action, args) (*exc)
 *   dispatch_s6           -> _dispatch_via_service_manager_if_s6(action) (*exc)
 *   dispatch_all_s6       -> _dispatch_all_via_service_manager_if_s6(action) (*exc)
 *   host_owner            -> _host_multiplexer_for_all_verb() (owner descriptor/NULL)
 *   served_owner          -> _served_by_another_host_gateway() (owner descriptor/NULL)
 *   host_is_ours          -> _host_multiplexer_is_ours(owner)
 *   named_served          -> named_profile_served_by_running_multiplexer()
 *   find_pids             -> find_gateway_pids() (NEW int array)
 *   kill_processes        -> kill_gateway_processes(all_profiles=)
 *   stop_profile          -> stop_profile_gateway()
 *   stop_host_multiplexer -> _stop_host_multiplexer(owner)
 *   report_stop_failure   -> gateway_profile_lifecycle.report_profile_stop_failure()
 *   wait_gateway_exit     -> _wait_for_gateway_exit(timeout=, force_after=)
 *   wait_port_free        -> _wait_for_api_server_port_free()
 *   discard_host_record   -> _discard_dead_host_record()
 *   forget_host_probe     -> _forget_host_gateway_probe()
 *   spawn_detached        -> _spawn_detached_gateway()
 *   run_gateway           -> run_gateway(verbose=, replace=)
 *   get_snapshot          -> get_gateway_runtime_snapshot(system=) (NEW descriptor)
 *   print_parked_status   -> gateway_profile_lifecycle.print_parked_status()
 *   report_incompat_owner -> gateway_status_notes.report_incompatible_owner(pids)
 *   print_standalone_cfg  -> gateway_status_notes.print_standalone_by_config()
 *   print_served_urls     -> _print_served_ingress_urls(profile)
 *   print_unserved_urls   -> _print_unserved_shared_ingress(profile)
 *   print_multiplex_reason-> _print_multiplex_standalone_reason()
 *   print_runtime_health  -> _print_runtime_health()
 *   print_dup_creds       -> _print_duplicate_credential_warnings()
 *   print_other_profiles  -> _print_other_profiles_gateway_status()
 *   print_unfolded_note   -> _print_unfolded_gateway_note(owner)
 *   print_mismatch        -> _print_gateway_process_mismatch(snapshot)
 *   owner_describe        -> owner.describe() (borrowed)
 *   host_scope_enter/exit -> host_scope_for_all_verb(owner) context manager
 *   restart_scope_enter/exit -> restart_all_verb_scope(owner) context manager
 *   owe_online_notice     -> the notice callback yielded by restart_all_verb_scope
 *   restart_wait_budget   -> _get_restart_exit_wait_budget()
 *   process_start_time    -> gateway.status.get_process_start_time(pid) (*has)
 *   drain_reporter        -> update_cmd_drain_report.drain_progress_reporter(budget_s=) (borrowed token)
 *   graceful_sigusr1      -> _graceful_restart_via_sigusr1(pid, budget, on_progress=)
 *   leave_restart_pending -> update_cmd_drain_report.leave_restart_pending(pid, label, start_time=)
 *   wait_systemd_restart  -> _wait_for_systemd_service_restart(system=, previous_pid=, replacement_observed=)
 *   systemd_start_limited -> _systemd_service_is_start_limited(system=)
 *   read_unit_props       -> _read_systemd_unit_properties(system=) (NEW object)
 *   unit_read_text        -> unit_path.read_text(encoding="utf-8-sig") (malloc'd; *raised on OSError)
 *   print                 -> builtins print(*args) (pre-formatted text)
 *   print_error           -> hermes_cli.setup.print_error(text)
 */
typedef struct gwsl_seams {
    void *ctx;
    bool (*supports_systemd)(void *ctx);
    bool (*is_macos)(void *ctx);
    bool (*is_windows)(void *ctx);
    bool (*is_wsl)(void *ctx);
    bool (*is_container)(void *ctx);
    bool (*is_termux)(void *ctx);
    bool (*is_apt_termux)(void *ctx);
    bool (*is_supervised)(void *ctx);
    char *(*detect_service_mgr)(void *ctx);
    bool (*systemd_unit_installed)(void *ctx);
    bool (*launchd_plist_exists)(void *ctx);
    const char *(*service_name)(void *ctx);
    const char *(*scope_label)(void *ctx, bool system);
    const char *(*current_profile_name)(void *ctx);
    const char *(*get_hermes_home)(void *ctx);
    const char *(*active_profile_name)(void *ctx);
    bool (*profile_is_standalone)(void *ctx, const char *home);
    const char *(*standalone_notice)(void *ctx);
    const char *(*routing_home)(void *ctx);
    const char *(*named_profile_home)(void *ctx, const char *home);
    json_t *(*gw_windows_call)(void *ctx, const char *method, const json_t *kwargs, gwsl_exc_t *exc);
    void (*systemd_call)(void *ctx, const char *verb, int system_tristate, gwsl_exc_t *exc);
    void (*systemd_status)(void *ctx, bool deep, bool system, bool full, gwsl_exc_t *exc);
    void (*launchd_call)(void *ctx, const char *verb, gwsl_exc_t *exc);
    void (*launchd_install)(void *ctx, bool force, bool start_now, bool force_unit_path, gwsl_exc_t *exc);
    void (*launchd_status)(void *ctx, bool deep, gwsl_exc_t *exc);
    bool (*is_managed)(void *ctx);
    void (*managed_error)(void *ctx, const char *action);
    bool (*refuses_container)(void *ctx, bool system);
    void (*record_service_choice)(void *ctx, const char *action);
    void (*install_systemd_cli)(void *ctx, const json_t *args, bool force, bool system,
                                const char *run_as_user, gwsl_exc_t *exc);
    bool (*profile_lifecycle)(void *ctx, const char *action, const json_t *args);
    void (*guard_named_profile)(void *ctx, bool force, gwsl_exc_t *exc);
    void (*refuse_sessions_live)(void *ctx, const char *action, const json_t *args, gwsl_exc_t *exc);
    bool (*dispatch_s6)(void *ctx, const char *action, gwsl_exc_t *exc);
    bool (*dispatch_all_s6)(void *ctx, const char *action, gwsl_exc_t *exc);
    json_t *(*host_owner)(void *ctx);
    json_t *(*served_owner)(void *ctx);
    bool (*host_is_ours)(void *ctx, const json_t *owner);
    bool (*named_served)(void *ctx);
    json_t *(*find_pids)(void *ctx);
    int (*kill_processes)(void *ctx, bool all_profiles);
    bool (*stop_profile)(void *ctx);
    int (*stop_host_multiplexer)(void *ctx, const json_t *owner);
    void (*report_stop_failure)(void *ctx);
    void (*wait_gateway_exit)(void *ctx, double timeout, double force_after);
    void (*wait_port_free)(void *ctx);
    void (*discard_host_record)(void *ctx);
    void (*forget_host_probe)(void *ctx);
    bool (*spawn_detached)(void *ctx);
    void (*run_gateway)(void *ctx, int verbose, bool replace);
    json_t *(*get_snapshot)(void *ctx, bool system);
    bool (*print_parked_status)(void *ctx);
    bool (*report_incompat_owner)(void *ctx, const json_t *pids);
    void (*print_standalone_cfg)(void *ctx);
    void (*print_served_urls)(void *ctx, const char *profile);
    void (*print_unserved_urls)(void *ctx, const char *profile);
    void (*print_multiplex_reason)(void *ctx);
    void (*print_runtime_health)(void *ctx);
    void (*print_dup_creds)(void *ctx);
    void (*print_other_profiles)(void *ctx);
    void (*print_unfolded_note)(void *ctx, const json_t *owner);
    void (*print_mismatch)(void *ctx, const json_t *snapshot);
    const char *(*owner_describe)(void *ctx, const json_t *owner);
    void (*host_scope_enter)(void *ctx, const json_t *owner);
    void (*host_scope_exit)(void *ctx);
    void (*restart_scope_enter)(void *ctx, const json_t *owner);
    void (*restart_scope_exit)(void *ctx);
    void (*owe_online_notice)(void *ctx);
    double (*restart_wait_budget)(void *ctx);
    double (*process_start_time)(void *ctx, int pid, bool *has);
    const char *(*drain_reporter)(void *ctx, double budget_s);
    bool (*graceful_sigusr1)(void *ctx, int pid, double budget, const char *on_progress);
    bool (*leave_restart_pending)(void *ctx, int pid, const char *label, double start_time, bool has_start);
    bool (*wait_systemd_restart)(void *ctx, bool system, int previous_pid, bool *replacement_observed);
    bool (*systemd_start_limited)(void *ctx, bool system);
    json_t *(*read_unit_props)(void *ctx, bool system);
    char *(*unit_read_text)(void *ctx, const char *path, bool *raised);
    void (*reap_orphans)(void *ctx, gwsl_exc_t *exc, const char **exc_msg);
    void (*logger_debug)(void *ctx, const char *msg);
    bool (*get_linger_status)(void *ctx, bool *ok);
    const char *(*getuser)(void *ctx);
    long (*get_running_pid)(void *ctx);
    bool (*declares_external_supervisor)(void *ctx, long pid);
    void (*restart_externally_supervised)(void *ctx, long pid);
    void (*refuse_restart_service_managed)(void *ctx, long pid, gwsl_exc_t *exc);
    void (*start_restart_replacement)(void *ctx, const json_t *args, bool force, gwsl_exc_t *exc);
    void (*print)(void *ctx, const char *text);
    void (*print_error)(void *ctx, const char *text);
} gwsl_seams_t;

/* ── Pure / probe leaves ─────────────────────────────────────────────────── */

/* "systemd" / "launchd" / "windows" (static strings) or NULL. windows=false
 * never probes is_windows. */
/* PoP: gwsl_service_backend @ hermes_cli/gateway.py:_service_backend */
const char *gwsl_service_backend(bool windows, const gwsl_seams_t *s);

/* Installed-kind ladder with an injected windows thunk. thunk may be NULL
 * (treated as false). Returns static string or NULL. */
/* PoP: gwsl_installed_service_kind_for @ hermes_cli/gateway.py:_installed_service_kind_for */
const char *gwsl_installed_service_kind_for(const gwsl_seams_t *s,
                                            bool (*windows_thunk)(void *thunk_ctx),
                                            void *thunk_ctx);

/* _installed_service_kind(): kind_for(lambda: is_windows() and _gw_windows().is_installed()). */
/* PoP: gwsl_installed_service_kind @ hermes_cli/gateway.py:_installed_service_kind */
const char *gwsl_installed_service_kind(const gwsl_seams_t *s);

/* Dispatch verb on backend. system_tristate: 0=false, 1=true, -1=None.
 * Uncaught backend exception propagates via *exc. */
/* PoP: gwsl_service_call @ hermes_cli/gateway.py:_service_call */
void gwsl_service_call(const char *backend, const char *verb, int system_tristate,
                       const gwsl_seams_t *s, gwsl_exc_t *exc);

/* Stop the installed service; true if one was stopped. Uncaught exception via *exc. */
/* PoP: gwsl_stop_installed_service @ hermes_cli/gateway.py:_stop_installed_service */
bool gwsl_stop_installed_service(bool system, const gwsl_seams_t *s, gwsl_exc_t *exc);

/* Profile name when home is <default>/profiles/<name> with a service-safe name,
 * else NULL. malloc'd result (caller frees). Paths are host-native strings. */
/* PoP: gwsl_profile_name_from_home @ hermes_cli/gateway.py:_profile_name_from_home */
char *gwsl_profile_name_from_home(const char *home, const char *default_root);

/* Environment="NAME=..." value from a unit file text (read via seam), with
 * systemd quoting undone; NULL when absent/unreadable. malloc'd (caller frees). */
/* PoP: gwsl_unit_environment_value @ hermes_cli/gateway.py:_unit_environment_value */
char *gwsl_unit_environment_value(const char *unit_path, const char *name, const gwsl_seams_t *s);

/* int(value) when it parses and is > 0, else -1 (None). value is a JSON scalar. */
/* PoP: gwsl_positive_pid @ hermes_cli/gateway.py:_positive_pid */
long gwsl_positive_pid(const json_t *value);

/* ── No-backend error/exit table ─────────────────────────────────────────── */

/* Print the (subcommand, reason) message lines and exit with its code (None ->
 * plain return). *key_error=true when the pair is absent (live KeyError). */
/* PoP: gwsl_no_backend_exit @ hermes_cli/gateway.py:_no_backend_exit */
gwsl_verdict_t gwsl_no_backend_exit(const char *subcommand, const char *reason,
                                    const gwsl_seams_t *s, bool *key_error);

/* Fallthrough reason ladder: WSL (only when wsl) -> container (s6 hint only
 * when s6) -> unsupported, then no_backend_exit. */
/* PoP: gwsl_handle_no_backend_flow @ hermes_cli/gateway.py:_handle_no_backend */
gwsl_verdict_t gwsl_handle_no_backend_flow(const char *subcommand, bool wsl, bool s6,
                                           const gwsl_seams_t *s, bool *key_error);

/* ── Graceful systemd restart observation ────────────────────────────────── */

/* "start" / "restart" (static) or NULL when systemd owns the relaunch. */
/* PoP: gwsl_systemd_graceful_restart_action_flow @ hermes_cli/gateway.py:_systemd_graceful_restart_action */
const char *gwsl_systemd_graceful_restart_action_flow(bool system, int pid, const gwsl_seams_t *s);

/* ── Command flows ───────────────────────────────────────────────────────── */

/* PoP: gwsl_cmd_install_flow @ hermes_cli/gateway.py:_cmd_install */
gwsl_verdict_t gwsl_cmd_install_flow(const json_t *args, const gwsl_seams_t *s, gwsl_exc_t *exc);
/* PoP: gwsl_cmd_uninstall_flow @ hermes_cli/gateway.py:_cmd_uninstall */
gwsl_verdict_t gwsl_cmd_uninstall_flow(const json_t *args, const gwsl_seams_t *s, gwsl_exc_t *exc);
/* PoP: gwsl_cmd_start_flow @ hermes_cli/gateway.py:_cmd_start */
gwsl_verdict_t gwsl_cmd_start_flow(const json_t *args, const gwsl_seams_t *s, gwsl_exc_t *exc);
/* PoP: gwsl_cmd_stop_flow @ hermes_cli/gateway.py:_cmd_stop */
gwsl_verdict_t gwsl_cmd_stop_flow(const json_t *args, const gwsl_seams_t *s, gwsl_exc_t *exc);
/* PoP: gwsl_cmd_restart_flow @ hermes_cli/gateway.py:_cmd_restart */
gwsl_verdict_t gwsl_cmd_restart_flow(const json_t *args, const gwsl_seams_t *s, gwsl_exc_t *exc);
/* PoP: gwsl_cmd_status_flow @ hermes_cli/gateway.py:_cmd_status */
gwsl_verdict_t gwsl_cmd_status_flow(const json_t *args, const gwsl_seams_t *s, gwsl_exc_t *exc);
/* PoP: gwsl_restart_all_flow @ hermes_cli/gateway.py:_restart_all */
gwsl_verdict_t gwsl_restart_all_flow(bool system, bool detach, const gwsl_seams_t *s, gwsl_exc_t *exc);
/* PoP: gwsl_restart_all_as_host_flow @ hermes_cli/gateway.py:_restart_all_as_host */
gwsl_verdict_t gwsl_restart_all_as_host_flow(const json_t *owner, bool system, bool detach,
                                             const gwsl_seams_t *s, gwsl_exc_t *exc);

#ifdef __cplusplus
}
#endif

#endif /* PORT_GW_SERVICE_LIFECYCLE_H */