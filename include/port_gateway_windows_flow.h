/*
 * port_gateway_windows_flow.h — Windows gateway service FLOW cores of
 * hermes_cli/gateway_windows.py (LIVE authority).
 *
 * Ports the DECISION sequences of the Scheduled-Task / Startup-folder service
 * backend: start-or-report routing, the UAC elevation offer, autostart
 * reconciliation (task vs legacy .cmd vs .vbs), redundant-entry detection,
 * startup-entry removal, the start-attestation death probe (read marker ->
 * horizon -> liveness -> generation), sentinel clean-exit identity rules,
 * one-shot marker consumption, scheduled-task drift repair, live-PID discovery
 * and pre-drain PID fingerprinting.
 *
 * SEAM DOCTRINE: every live effect (schtasks queries, filesystem exists/unlink/
 * read/write, process-table scans, prompts, elevation launches, prints, marker
 * read/clear, lifecycle-sentinel reads, time horizon) enters through ONE
 * universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a borrowed JSON
 * object; the seam returns a NEW json_t* (NULL == Python None). A raising live
 * call is scripted {"__raise__": "<ExcClass>", ...}; the core propagates it
 * exactly where the live try/except catches it. The harness records one trace
 * event per call {"ev": name, "args": args, "ret": result} in call order; the
 * oracle's recording monkeypatch stubs emit the identical shape driving LIVE
 * Python with the same scripts. The cores perform NO I/O themselves.
 *
 * Paths are POSIX strings (the oracle normalizes Path via .as_posix()).
 * Ownership: returned json_t* / char* are malloc'd (caller frees) unless noted.
 */
#ifndef PORT_GATEWAY_WINDOWS_FLOW_H
#define PORT_GATEWAY_WINDOWS_FLOW_H

#include <stdbool.h>
#include <stddef.h>
#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Trace recorder (harness infrastructure) ── */
typedef struct gww_trace { json_t *events; } gww_trace_t;
void gww_trace_add(gww_trace_t *t, json_t *ev);
void gww_trace_free(gww_trace_t *t);

/* ── Universal seam interface ── */
typedef struct gww_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gww_seams_t;

json_t *gww_call(const gww_seams_t *s, const char *name, const json_t *args);
const char *gww_raise_class(const json_t *r);
bool gww_caught(const json_t *r, const char *cls);

/* Seam name table (name -> live call):
 *   print                       print(text)
 *   is_task_registered          is_task_registered()
 *   get_task_name               get_task_name()
 *   get_startup_entry_path      get_startup_entry_path()  (posix str)
 *   legacy_startup_entry_path   _legacy_startup_entry_path()
 *   startup_staging_path        _startup_staging_path()
 *   get_task_script_path        get_task_script_path()
 *   path_exists                 Path(path).exists()
 *   path_unlink                 Path(path).unlink(missing_ok=...)  (may raise)
 *   install_startup_entry       _install_startup_entry(script_path) -> entry str
 *   write_task_script           _write_task_script() -> cmd path str
 *   remove_startup_entries      _remove_startup_entries() -> {done,warnings}
 *   prompt_yes_no               setup.prompt_yes_no(question, default)
 *   launch_elevated_install     _launch_elevated_install(force,start_now,start_on_login)
 *   gateway_pids                _gateway_pids() -> [int]
 *   spawn_detached              _spawn_detached() -> pid
 *   report_gateway_start        _report_gateway_start(via)
 *   report_already_running      _report_already_running(pids)
 *   read_start_attestation      _read_start_attestation(home) -> data|null
 *   clear_start_attestation     _clear_start_attestation(home)
 *   within_horizon              _attestation_within_horizon(data) -> bool (time leaf)
 *   get_lifecycle_sentinel_path gateway.lifecycle_ledger.get_lifecycle_sentinel_path(home)
 *   read_text                   Path(path).read_text(encoding=...)  (may raise)
 *   scheduled_task_drift        scheduled_task_drift(task_name) -> [str]
 *   install_scheduled_task      _install_scheduled_task(name,script) -> {ok,detail}
 *   get_running_pid             gateway.status.get_running_pid(path,cleanup_stale)
 *   find_gateway_pids           hermes_cli.gateway.find_gateway_pids(all_profiles)
 *   pid_filter                  pid_filter(pids) -> [int]
 *   import_start_time           import gateway.status.get_process_start_time
 *                               ({"__raise__":"ImportError"} models the except branch)
 *   get_process_start_time      gateway.status.get_process_start_time(pid)
 */

/* ── Flow ports ── */

/* _start_or_report_running(running_pids=None): spawn unless one runs. */
/* PoP: gww_start_or_report_running @ hermes_cli/gateway_windows.py:_start_or_report_running */
void gww_start_or_report_running(const json_t *running_pids, const gww_seams_t *s);

/* _offer_elevated_install(headline, force, start_now, start_on_login) -> bool */
/* PoP: gww_offer_elevated_install @ hermes_cli/gateway_windows.py:_offer_elevated_install */
bool gww_offer_elevated_install(const char *headline, bool force, bool start_now,
                                bool start_on_login, const gww_seams_t *s);

/* reconcile_autostart_launchers() -> {done:[str], warnings:[str]} */
/* PoP: gww_reconcile_autostart_launchers @ hermes_cli/gateway_windows.py:reconcile_autostart_launchers */
json_t *gww_reconcile_autostart_launchers(const gww_seams_t *s);

/* redundant_autostart_entries() -> [path str] */
/* PoP: gww_redundant_autostart_entries @ hermes_cli/gateway_windows.py:redundant_autostart_entries */
json_t *gww_redundant_autostart_entries(const gww_seams_t *s);

/* _remove_startup_entries() -> {done:[str], warnings:[str]} */
/* PoP: gww_remove_startup_entries @ hermes_cli/gateway_windows.py:_remove_startup_entries */
json_t *gww_remove_startup_entries(const gww_seams_t *s);

/* attested_death_generation(current_pids, home) -> generation str|null */
/* PoP: gww_attested_death_generation @ hermes_cli/gateway_windows.py:attested_death_generation */
char *gww_attested_death_generation(const json_t *current_pids, const char *home,
                                    const gww_seams_t *s);

/* _attested_dead(attested, current_pids, data, home) -> bool */
/* PoP: gww_attested_dead @ hermes_cli/gateway_windows.py:_attested_dead */
bool gww_attested_dead(const json_t *attested, const json_t *current_pids,
                       const json_t *data, const char *home, const gww_seams_t *s);

/* _attested_pid_exited_cleanly(pid, create_time, home) -> bool.
 * create_time_present distinguishes None from a numeric create time. */
/* PoP: gww_attested_pid_exited_cleanly @ hermes_cli/gateway_windows.py:_attested_pid_exited_cleanly */
bool gww_attested_pid_exited_cleanly(int pid, bool create_time_present, double create_time,
                                     const char *home, const gww_seams_t *s);

/* _consume_start_attestation(generation, home): clear iff generation matches. */
/* PoP: gww_consume_start_attestation @ hermes_cli/gateway_windows.py:_consume_start_attestation */
void gww_consume_start_attestation(const char *generation, const char *home,
                                   const gww_seams_t *s);

/* reconcile_scheduled_task(task_name) -> bool */
/* PoP: gww_reconcile_scheduled_task @ hermes_cli/gateway_windows.py:reconcile_scheduled_task */
bool gww_reconcile_scheduled_task(const char *task_name, const gww_seams_t *s);

/* _live_gateway_pids(all_profiles, home, pid_filter_present) -> [int] */
/* PoP: gww_live_gateway_pids @ hermes_cli/gateway_windows.py:_live_gateway_pids */
json_t *gww_live_gateway_pids(bool all_profiles, const char *home,
                              bool pid_filter_present, const gww_seams_t *s);

/* _gateway_pid_identities(pids) -> {pid str: start_time|null} */
/* PoP: gww_gateway_pid_identities @ hermes_cli/gateway_windows.py:_gateway_pid_identities */
json_t *gww_gateway_pid_identities(const json_t *pids, const gww_seams_t *s);

/* ── Pure internal helpers (real ports of unclaimed leaves; no PoP claim) ── */
/* _attested_pids_from(data) -> [int] (fail closed on malformed). */
json_t *gww_attested_pids_from(const json_t *data);
/* _attestation_generation(data) -> str|null (caller frees). */
char *gww_attestation_generation(const json_t *data);
/* _attested_create_time(data, pid, *present) -> double (valid iff *present). */
double gww_attested_create_time(const json_t *data, int pid, bool *present);

#ifdef __cplusplus
}
#endif
#endif /* PORT_GATEWAY_WINDOWS_FLOW_H */