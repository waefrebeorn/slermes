/* port_main_cli_flow.h — FLOW-core port of hermes_cli/main.py (CLI entry/dispatch).
 *
 * Mission: match the program's FLOW (argument-parse dispatch chains, subcommand
 * routing, startup decision sequences, error/exit paths) of main.py's unclaimed
 * CLI dispatch functions. All I/O (env, files, prompts, prints, sibling calls,
 * imports, subprocess-free seams) is INJECTED through a generic callback
 * dispatcher + json_t state; no real I/O happens inside the decision core.
 *
 * Seam model
 * ----------
 * Every live external call goes through `mcl_io_t.call(io, name, args)`:
 *   - appends {"name": name, "args": args} to io->trace (ordered);
 *   - returns the scripted result (a NEW json_t* the caller frees);
 *   - models a raised exception with {"__raise__": "<Type>", "message": "...",
 *     "errno": <int>?} or, for SystemExit, {"__raise__": "SystemExit",
 *     "code": <int|string|null>}. `code` keeps its JSON type because the live
 *     code compares `exc.code == 0` (identity of int 0 vs str "0" matters).
 *
 * State-resolved seams (NOT scripted; resolved from in.env, recorded in trace):
 *   env.get   os.environ.get(name[, default])  args {"name","default"}
 *   env.pop   os.environ.pop(name, None)       args {"name","default"}
 * Echo/effect seams (script entry required as a marker; result is the live
 * effect, not the script value):
 *   print             builtins.print(text, end=..., file=...) — harness appends
 *                     args.text to the real stdout/stderr capture buffer
 *   stringio.getvalue speculative StringIO.getvalue() — returns the harness
 *                     speculative buffer (parser stdout written during the
 *                     speculative parse region)
 *   parser.parse_args a __script__ entry may carry "stdout"/"stderr": text the
 *                     parser writes; routed to the speculative buffer when
 *                     args.speculative is true, else to the real capture
 *                     (speculative stderr is discarded, as redirect_stderr
 *                     discards it live).
 *
 * Live counterpart of each scripted seam name:
 *   expanduser                     os.path.expanduser("~")
 *   sudo_invoker_default_home      hermes_constants.sudo_invoker_default_home()
 *   named_profile_is_live          hermes_constants.named_profile_is_live(path)
 *   path.read_text                 Path.read_text(encoding=..., errors=...)
 *   path.exists                    Path.exists()
 *   _resolve_last_session          main._resolve_last_session(source=...)
 *   input                          builtins.input(prompt)
 *   is_interactive_stdin           hermes_cli.setup.is_interactive_stdin()
 *   print_noninteractive_setup_guidance  hermes_cli.setup.print_... (message)
 *   cmd_setup                      main.cmd_setup(args)
 *   finalize_pending_update_receipt  hermes_cli.update_receipt.finalize_...(code, reason)
 *   handle_metadata_args           hermes_cli.update_channel.handle_metadata_args(args, root)
 *   is_managed / managed_error     hermes_cli.config.is_managed() / managed_error(action)
 *   _record_stop                   hermes_cli.update_cmd_common._record_stop(reason, without_receipt=...)
 *   collect_runtime_inventory      hermes_cli.update_inventory.collect_runtime_inventory()
 *   print_update_plan              hermes_cli.update_inventory.print_update_plan(inventory)
 *   list_venv_holders              hermes_cli.update_cmd_windows.list_venv_holders()
 *   json.dumps                     json.dumps(holders, indent=2)
 *   evaluate_update_admission      hermes_cli.update_contract.evaluate_update_admission(root)
 *   record_refusal_receipt         hermes_cli.update_contract.record_refusal_receipt(refusal)
 *   _resolve_update_branch         main._resolve_update_branch(args)
 *   _cmd_update_check              hermes_cli.update_cmd._cmd_update_check(**kwargs)
 *   _report_dashboard_status       main._report_dashboard_status()
 *   get_hermes_home                hermes_constants.get_hermes_home()
 *   _find_stale_dashboard_pids     main._find_stale_dashboard_pids(scope_home=...)
 *   _kill_stale_dashboard_processes  hermes_cli.dashboard_procs._kill_...(reason=..., scope_home=...)
 *   _is_desktop_owned_backend      main._is_desktop_owned_backend()
 *   _is_electron_packaged_web_dist main._is_electron_packaged_web_dist(value)
 *   import.fastapi / import.uvicorn  `import fastapi` / `import uvicorn`
 *   smart_app_control_block_message  hermes_cli.main_dep_hints.smart_...(exc)
 *   missing_optional_deps_message  hermes_cli.main_dep_hints.missing_...(surface, what, extra)
 *   require_parseable_user_config  hermes_cli.config.require_parseable_user_config(ignore_user_config=...)
 *   _bypass_chat_launch            main._bypass_chat_launch(args)
 *   _resolve_use_tui               main._resolve_use_tui(args)
 *   _promote_top_level_resume      main._promote_top_level_resume(args)
 *   _set_chat_arg_defaults         main._set_chat_arg_defaults(args)
 *   cmd_chat                       main.cmd_chat(args)
 *   _coalesce_session_name_args    main._coalesce_session_name_args(argv)
 *   _s6_running                    hermes_cli.service_manager._s6_running()
 *   gateway_run_no_supervise       hermes_cli._parser.gateway_run_no_supervise(argv)
 *
 * sys.exit / raise SystemExit are NOT seams: the port returns the SystemExit
 * raise descriptor as its result (the oracle catches BaseException per case).
 *
 * Data model / representation notes
 * ---------------------------------
 * - Paths are POSIX strings in fixtures. The oracle drives the live defs with
 *   os.path == posixpath and FakePath doubles whose str() is the posix string
 *   (as_posix normalization — str(Path) flips slashes on Windows).
 * - args is a plain JSON object modeling an argparse Namespace (vars(args));
 *   ports mutate in.args in place where the live code sets attributes.
 * - in.env is the environment state (object of strings); env.get/env.pop read
 *   and mutate it. in.subparsers = {"has_choices","choices","required"} models
 *   the argparse subparsers action (choices keys as an array).
 * - refusal spec = {"message": str}; kill result = {"failed": [...]}.
 * - read_text seams strip a leading U+FEFF when encoding == "utf-8-sig" on
 *   BOTH sides (the codec does it live). Fixture texts are valid UTF-8;
 *   errors="replace" grouping of truncated multi-byte sequences is out of
 *   fixture scope (documented).
 * - str.strip()/lower() fixture scope is ASCII whitespace/letters.
 * - Integral floats normalize to ints on both sides before comparison.
 * - None-valued arg keys are kept as JSON null on both sides (env.get default
 *   null vs "" is semantic).
 *
 * Ownership: every mcl_* port returns a NEW json_t* (caller frees). Args
 * passed to mcl_call are borrowed (copied into the trace). Inputs are
 * borrowed except in.args/in.env/in.subparsers, which stateful ports mutate
 * in place (the harness owns the pin copy).
 */
#ifndef PORT_MAIN_CLI_FLOW_H
#define PORT_MAIN_CLI_FLOW_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mcl_io_t {
    json_t *(*call)(struct mcl_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of {"name","args"}: ordered seam sequence */
    void   *ctx;     /* harness backend (scripted results + state) */
} mcl_io_t;

json_t *mcl_call(mcl_io_t *io, const char *name, json_t *args);
bool mcl_raised(const json_t *result);

/* Uniform port contract: json in -> json out (NEW node, caller frees).
 * A returned {"__raise__": ...} node models an exception escaping the live
 * function (SystemExit included). */
json_t *mcl_early_interface_config_path(mcl_io_t *io, json_t *in);
json_t *mcl_resolve_sudo_user_profile_env(mcl_io_t *io, json_t *in);
json_t *mcl_under_gateway_supervisor(mcl_io_t *io, json_t *in);
json_t *mcl_s6_supervised_gateway_run(mcl_io_t *io, json_t *in);
json_t *mcl_explicit_cli_profile(mcl_io_t *io, json_t *in);
json_t *mcl_read_packed_ref(mcl_io_t *io, json_t *in);
json_t *mcl_dotenv_has_provider_key(mcl_io_t *io, json_t *in);
json_t *mcl_latest_session_id(mcl_io_t *io, json_t *in);
json_t *mcl_first_run_setup_guard(mcl_io_t *io, json_t *in);
json_t *mcl_finalize_update_receipt(mcl_io_t *io, json_t *in);
json_t *mcl_update_preflight_handled(mcl_io_t *io, json_t *in);
json_t *mcl_dashboard_lifecycle_flags(mcl_io_t *io, json_t *in);
json_t *mcl_dashboard_sanitize_desktop_env(mcl_io_t *io, json_t *in);
json_t *mcl_require_dashboard_web_deps(mcl_io_t *io, json_t *in);
json_t *mcl_guard_noninteractive_user_config(mcl_io_t *io, json_t *in);
json_t *mcl_is_tui_chat_launch(mcl_io_t *io, json_t *in);
json_t *mcl_default_to_chat(mcl_io_t *io, json_t *in);
json_t *mcl_parse_cli_args(mcl_io_t *io, json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MAIN_CLI_FLOW_H */
