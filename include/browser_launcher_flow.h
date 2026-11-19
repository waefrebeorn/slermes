/*
 * browser_launcher_flow.h — launch-decision FLOW cores of
 * tools/browser_launcher.py (quarry: C:/Users/eman5/hermes-agent).
 *
 * Ports the DECISION sequences of the browser launch path: host-fact
 * detection chains (docker / apparmor / euid sandbox bypass), executable
 * resolution and readiness fallback, headed/headless selection, profile
 * (user-data-dir) naming, DevTools readiness retry/timeout, singleton-lock
 * ownership reads, running-instance attach probes, profile-dir preparation
 * error paths, and the top-level launch_chromium dispatch (reuse / packaged
 * resolution / ephemeral vs shared). All live OS/process I/O enters through
 * ONE universal seam (blf_io_t.call); the cores perform no I/O.
 *
 * Already claimed elsewhere (NOT re-ported here): _has_display and
 * build_chromium_argv (src/tools/port_browser_launcher.c),
 * _maybe_autoinstall_chromium (src/tools/browser_tool_install.c). They appear
 * below only as scripted seam names.
 *
 * ── Universal seam contract ────────────────────────────────────────────────
 *   json_t *call(void *ctx, const char *name, const json_t *args)
 * args is a BORROWED object (None-valued keys already dropped). The result is
 * a NEW json_t owned by the caller: NULL == Python None; an object carrying a
 * string "__raise__" key ({"__raise__": cls, "msg": str}) == an exception of
 * that class with str(exc) == msg, which the core propagates or catches
 * exactly where the live code does. The harness stub records one
 * {ev, args, ret} event per call (ret = the RAW scripted value) and the
 * oracle's recording monkeypatches emit the identical shape.
 *
 * Script shapes (fixture "io" object, per seam name):
 *   absent / null        -> None on every call
 *   {"seq": [v1, v2..]}  -> vi on the i-th call (0-based); None when exhausted
 *   any other value      -> reused on every call
 *
 * ── Seam name table (name -> live call) ────────────────────────────────────
 *   exists                          os.path.exists
 *   open                            builtins.open (ret = file TEXT; the core
 *                                   models readline/read + universal newlines
 *                                   + utf-8-sig BOM strip internally)
 *   read_bytes                      Path.read_bytes (ret = {"b64": ...})
 *   mkdir                           Path.mkdir (parents/exist_ok kwargs)
 *   makedirs                        os.makedirs
 *   unlink                          os.unlink
 *   rmtree                          shutil.rmtree (ignore_errors=True swallows
 *                                   a scripted raise AFTER recording it)
 *   readlink                        os.readlink
 *   isfile                          os.path.isfile
 *   getpid                          os.getpid
 *   geteuid                         os.geteuid (only when has_geteuid)
 *   monotonic                       time.monotonic
 *   sleep                           time.sleep
 *   proc.poll                       subprocess.Popen.poll (no args)
 *   chromium_executable             hermes_cli.browser_runtime.chromium_executable
 *                                   (allow_override kwarg when passed)
 *   locate_command                  hermes_platform.resolver.locate_command
 *                                   (ret = {"command": [...]})
 *   command_timeout                 tools.browser_session_cloud.command_timeout
 *   is_headed_mode                  tools.browser_session_cloud.is_headed_mode
 *   windows_hide_flags              hermes_cli._subprocess_compat.windows_hide_flags
 *   ns_tag                          tools.browser_launcher_holders.ns_tag
 *   hermes_home_key                 hermes_constants.hermes_home_key
 *   running_in_docker               module sibling (scripted in composite flows)
 *   apparmor_restricts_unprivileged_userns   module sibling
 *   _resolve_executable             module sibling
 *   _maybe_autoinstall_chromium     module sibling (claimed port elsewhere)
 *   _chromium_missing_hint          module sibling
 *   _has_display                    module sibling (claimed port elsewhere)
 *   _read_devtools_file             module sibling
 *   _log_tail                       module sibling
 *   _singleton_owner                module sibling
 *   _singleton_pid                  module sibling
 *   _pid_naming_dir                 module sibling
 *   _pid_alive                      module sibling
 *   _devtools_endpoint_live         module sibling
 *   _start_time                     module sibling
 *   get_browser                     module sibling
 *   chromium_ready                  module sibling
 *   _default_user_data_dir          module sibling
 *   _launch_owned                   module sibling (kwargs object)
 *   _start_shared                   module sibling (prepare callable recorded
 *                                   as the token "<callable>")
 *   _key_lock                       module sibling _key_lock (ret ignored; the
 *                                   oracle always returns a no-op lock double)
 *
 * Host facts injected as case inputs (not seams): os_name ("posix"/"nt"),
 * platform (sys.platform), has_geteuid, create_new_process_group (the
 * subprocess attribute; absent -> getattr default 0).
 *
 * ── Calling convention ─────────────────────────────────────────────────────
 * Every core: json_t *blf_<fn>(const json_t *in, const blf_io_t *io,
 *                              json_t *state, json_t **exc)
 *   in    borrowed case input object.
 *   state borrowed mutable state object {"served_homes": [...]} — only
 *         launch_chromium writes it (_served_homes.add under _lock).
 *   exc   out-param: on a propagating exception, set to the NEW raise-spec
 *         object and the core returns NULL. *exc is NULL on entry.
 *   ret   NEW json_t: the Python return value (tuple -> array, None ->
 *         json_null(), str -> string, bool -> bool, float -> number).
 *
 * Fixture scope: paths are POSIX-spelled with no "." / ".." / duplicate-slash
 * segments (the oracle records Path.as_posix() and posixpath.join; the C
 * join is byte-identical to posixpath.join, the C parent matches Path.parent
 * on that domain); str.strip()/lower() model ASCII whitespace + U+0085 /
 * U+00A0 and ASCII case; isdigit() models ASCII digits (unicode-digit inputs
 * where int() would diverge are out of scope); read_bytes scripts carry
 * base64 bytes; poll/start_time/pid scripts are integral.
 */
#ifndef BROWSER_LAUNCHER_FLOW_H
#define BROWSER_LAUNCHER_FLOW_H

#include <stdbool.h>
#include <stddef.h>

#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct blf_io {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} blf_io_t;

/* Host facts: running_in_docker / apparmor_restricts_unprivileged_userns /
 * needs_chromium_sandbox_bypass. */
json_t *blf_running_in_docker(const json_t *in, const blf_io_t *io,
                              json_t *state, json_t **exc);
json_t *blf_apparmor_restricts(const json_t *in, const blf_io_t *io,
                               json_t *state, json_t **exc);
json_t *blf_needs_sandbox_bypass(const json_t *in, const blf_io_t *io,
                                 json_t *state, json_t **exc);

/* Executable resolution / readiness: _explicit_executable_configured /
 * _resolve_executable / chromium_ready. */
json_t *blf_explicit_executable_configured(const json_t *in, const blf_io_t *io,
                                           json_t *state, json_t **exc);
json_t *blf_resolve_executable(const json_t *in, const blf_io_t *io,
                               json_t *state, json_t **exc);
json_t *blf_chromium_ready(const json_t *in, const blf_io_t *io,
                           json_t *state, json_t **exc);

/* Launch decisions: _launch_timeout / _resolve_headed / _spawn_kwargs. */
json_t *blf_launch_timeout(const json_t *in, const blf_io_t *io,
                           json_t *state, json_t **exc);
json_t *blf_resolve_headed(const json_t *in, const blf_io_t *io,
                           json_t *state, json_t **exc);
json_t *blf_spawn_kwargs(const json_t *in, const blf_io_t *io,
                         json_t *state, json_t **exc);

/* DevTools readiness: _read_devtools_file / _log_tail / _wait_for_devtools. */
json_t *blf_read_devtools_file(const json_t *in, const blf_io_t *io,
                               json_t *state, json_t **exc);
json_t *blf_log_tail(const json_t *in, const blf_io_t *io,
                     json_t *state, json_t **exc);
json_t *blf_wait_for_devtools(const json_t *in, const blf_io_t *io,
                              json_t *state, json_t **exc);

/* Singleton / running-instance detection: _singleton_owner / _singleton_pid /
 * running_instance. */
json_t *blf_singleton_owner(const json_t *in, const blf_io_t *io,
                            json_t *state, json_t **exc);
json_t *blf_singleton_pid(const json_t *in, const blf_io_t *io,
                          json_t *state, json_t **exc);
json_t *blf_running_instance(const json_t *in, const blf_io_t *io,
                             json_t *state, json_t **exc);

/* Profile dir decisions: _default_user_data_dir / _prepare_dir. */
json_t *blf_default_user_data_dir(const json_t *in, const blf_io_t *io,
                                  json_t *state, json_t **exc);
json_t *blf_prepare_dir(const json_t *in, const blf_io_t *io,
                        json_t *state, json_t **exc);

/* Top-level dispatch: launch_chromium (writes state.served_homes). */
json_t *blf_launch_chromium(const json_t *in, const blf_io_t *io,
                            json_t *state, json_t **exc);

#ifdef __cplusplus
}
#endif

#endif /* BROWSER_LAUNCHER_FLOW_H */
