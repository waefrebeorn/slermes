/* port_main_desktop_flow.h — FLOW-core port of hermes_cli/main_desktop.py
 * (desktop launch/lifecycle decision chains).
 *
 * Mission: match the program's FLOW (launch decision sequences, env
 * sanitization, dependency guards, window/state branches, error/exit paths)
 * of main_desktop.py's UNCLAIMED desktop-launch functions. All OS/process I/O
 * (env, files, renames, sleeps, prints, sibling calls, ctypes, subprocess-free
 * seams) is INJECTED through a generic callback dispatcher + json_t state; no
 * real I/O happens inside the decision core.
 *
 * Seam model
 * ----------
 * Every live external call goes through `mdf_io_t.call(io, name, args)`:
 *   - appends {"name": name, "args": args} to io->trace (ordered);
 *   - returns the scripted result (a NEW json_t* the caller frees);
 *   - models a raised exception with {"__raise__": "<Type>", "message": "...",
 *     "errno": <int>?} or, for SystemExit, {"__raise__": "SystemExit",
 *     "code": <int|string|null>}.
 *
 * State-resolved seams (NOT scripted; resolved from in.os_environ, recorded):
 *   env.contains   `key in os.environ`          args {"name"} -> bool
 * Echo/effect seams (script entry required as a marker; result is the live
 * effect, not the script value):
 *   print                  builtins.print(text, end=..., file=...) — harness
 *                          appends args.text to the real stdout/stderr capture
 *   with_hermes_node_path  hermes_constants.with_hermes_node_path() — harness
 *                          returns a COPY of in.env (the child-env initial
 *                          state); the port mutates and returns it
 *
 * Live counterpart of each scripted seam name:
 *   explicit_cli_profile             hermes_cli.main.explicit_cli_profile()
 *   facts.native_arch                hermes_platform.host.facts.native_arch()
 *   platform.machine                 platform.machine()
 *   _kernel32                        main_desktop._kernel32() (ctypes.WinDLL)
 *   GetMachineTypeAttributes         kernel32.GetMachineTypeAttributes(machine,
 *                                    byref(attributes)) -> {"hresult","attributes"}
 *   _windows_user_runnable_pe_machines  sibling (seamed when driving
 *                                    _expected_windows_pe_machines)
 *   _windows_native_machine          sibling (seamed when driving
 *                                    _expected_windows_pe_machines)
 *   path.stat                        Path.stat() -> {"st_size": int}
 *   path.open                        Path.open("rb") (context manager entry)
 *   file.read                        fh.read(n) -> hex-encoded bytes string
 *   file.seek                        fh.seek(pos)
 *   path.is_file / path.is_dir / path.exists   Path predicates -> bool
 *   path.glob                        Path.glob(pattern) -> [path strings]
 *   path.rglob                       Path.rglob(pattern) -> [path strings]
 *                                    (the live next() takes the first item)
 *   path.expanduser / path.resolve   Path methods -> posix path string
 *   _packaged_resources_dir          sibling -> path string | null
 *   read_install_stamp               hermes_cli.steward.read_install_stamp(resources)
 *   _run_git                         hermes_cli.version_info._run_git(root, *args)
 *   _desktop_build_needed            sibling -> bool
 *   is_wsl                           hermes_constants.is_wsl()
 *   socket_safe_tmpdir               hermes_constants.socket_safe_tmpdir()
 *   _desktop_launch_options          sibling -> [flags, gpu, store, ozone, a11y]
 *   _detect_linux_password_store     sibling -> string | null
 *   os.getcwd                        os.getcwd()
 *   _desktop_dist_exists             sibling -> bool
 *   _electron_dir                    sibling -> path string
 *   desktop_launch_notice            hermes_cli.desktop_console.desktop_launch_notice(msg)
 *   _desktop_linux_sandbox_fixup     sibling -> bool
 *   _desktop_linux_needs_no_sandbox  sibling -> bool
 *   _desktop_linux_sandbox_helper_is_regular_file  sibling -> bool
 *   _desktop_linux_needs_disable_setuid_sandbox    sibling -> bool
 *   print_no_sandbox_fallback        hermes_cli.main_desktop_apparmor.print_no_sandbox_fallback(exe)
 *   _desktop_packaged_executable     sibling -> path string | null
 *   _desktop_macos_setup_tcc_identity  hermes_cli.main_desktop_tcc._desktop_macos_setup_tcc_identity(identity)
 *   resolve_bundle_layout            hermes_cli.bundled_app.resolve_bundle_layout(root)
 *                                    -> {"launcher": str|null, "app_root": str}
 *   install_profile                  hermes_cli.main_desktop_apparmor.install_profile(exe)
 *   launch_detached                  hermes_cli.bundled_app.launch_detached(cmd, env=..., cwd=...)
 *   os.rename                        os.rename(src, dst)
 *   logger.warning                   logger.warning(fmt, *args) — args.message is
 *                                    the %-formatted text (both sides format)
 *   time.sleep                       time.sleep(delay)
 *
 * Internal helpers ported for real (NOT seams; their sub-seams appear in the
 * trace exactly as the live nested calls emit them):
 *   _prefer_wsl_d3d12                (called by _desktop_launch_env)
 *   _packaged_desktop_launch_command (called by _launch_bundled_desktop)
 *   _explicit_profile_args           (called by _launch_bundled_desktop)
 *
 * sys.exit / raise SystemExit are NOT seams: the port returns the SystemExit
 * raise descriptor as its result (the oracle catches BaseException per case).
 *
 * Data model / representation notes
 * ---------------------------------
 * - Paths are POSIX strings in fixtures. The oracle drives the live defs with
 *   FakePath doubles (posix semantics: join/parent/parts/name) whose str() is
 *   the posix string, and os.path == posixpath. Fixture scope excludes
 *   double-leading-slash roots and non-UTF-8 texts.
 * - in.args is a plain JSON object modeling an argparse Namespace (vars(args));
 *   getattr(args, name, default) reads it with the live default.
 * - in.env is the child-env initial state (object of strings); in.os_environ
 *   is the PROCESS env consulted by `key in os.environ` (env.contains).
 * - in.platform models sys.platform ("win32" / "darwin" / "linux").
 * - Sets returned by the live defs (_windows_user_runnable_pe_machines,
 *   _expected_windows_pe_machines) are compared as SORTED ascending arrays.
 * - file.read scripts carry hex-encoded bytes (no NUL/escape ambiguity).
 * - Integral floats normalize to ints on both sides before comparison.
 * - str.strip/lower/upper and repr() fixture scope is ASCII; repr() emulates
 *   Python's quote choice (' preferred, " when the text has ' but no ").
 * - os.fsencode byte length == UTF-8 byte length (fixture texts valid UTF-8).
 *
 * Ownership: every mdf_* port returns a NEW json_t* (caller frees). Args
 * passed to mdf_call are borrowed (copied into the trace). Inputs are
 * borrowed except in.env, which stateful ports mutate in place (the harness
 * owns the pin copy).
 */
#ifndef PORT_MAIN_DESKTOP_FLOW_H
#define PORT_MAIN_DESKTOP_FLOW_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mdf_io_t {
    json_t *(*call)(struct mdf_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of {"name","args"}: ordered seam sequence */
    void   *ctx;     /* harness backend (scripted results + state) */
} mdf_io_t;

json_t *mdf_call(mdf_io_t *io, const char *name, json_t *args);
bool mdf_raised(const json_t *result);

/* Uniform port contract: json in -> json out (NEW node, caller frees).
 * A returned {"__raise__": ...} node models an exception escaping the live
 * function (SystemExit included). */
json_t *mdf_site_packages_install_kind(mdf_io_t *io, json_t *in);
json_t *mdf_explicit_profile_args(mdf_io_t *io, json_t *in);
json_t *mdf_diagnose_esbuild_ignore_scripts(mdf_io_t *io, json_t *in);
json_t *mdf_macos_signing_downgrade_error(mdf_io_t *io, json_t *in);
json_t *mdf_sandbox_helper_is_setuid_root(mdf_io_t *io, json_t *in);
json_t *mdf_windows_native_machine(mdf_io_t *io, json_t *in);
json_t *mdf_windows_user_runnable_pe_machines(mdf_io_t *io, json_t *in);
json_t *mdf_expected_windows_pe_machines(mdf_io_t *io, json_t *in);
json_t *mdf_parse_pe_machine(mdf_io_t *io, json_t *in);
json_t *mdf_packaged_node_pty_missing(mdf_io_t *io, json_t *in);
json_t *mdf_packaged_desktop_current_for_head(mdf_io_t *io, json_t *in);
json_t *mdf_prefer_wsl_d3d12(mdf_io_t *io, json_t *in);
json_t *mdf_desktop_launch_env(mdf_io_t *io, json_t *in);
json_t *mdf_check_desktop_skip_build(mdf_io_t *io, json_t *in);
json_t *mdf_packaged_desktop_launch_command(mdf_io_t *io, json_t *in);
json_t *mdf_run_desktop_one_shot(mdf_io_t *io, json_t *in);
json_t *mdf_launch_bundled_desktop(mdf_io_t *io, json_t *in);
json_t *mdf_rename_riding_out_file_lock(mdf_io_t *io, json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MAIN_DESKTOP_FLOW_H */
