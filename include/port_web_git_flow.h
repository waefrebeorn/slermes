/* port_web_git_flow.h — FLOW-core port of hermes_cli/web_git.py.
 *
 * Mission: match the program's FLOW (git command decision chains, repo state
 * resolution ordering, branch/diff projection branches, auth/error paths) of
 * the web dashboard git-operations module: the non-interactive subprocess
 * runner chain (_run -> _git_in -> _git/_git_write -> _git_out/_git_line/
 * _git_ok), porcelain-v2 status projection (repo_status, review_list),
 * single-file diff gating (review_diff), the commit identity/env flow
 * (_review_commit_env, review_commit, review_commit_context) and the worktree
 * creation decision chains (worktree_add, _worktree_for_existing).
 * Subprocess / filesystem / registry I/O is INJECTED through seams (a generic
 * callback dispatcher + json_t state), never performed inside the decision
 * core. libjson-only; no real I/O, no logger.
 *
 * Seam model
 * ----------
 * Every live external call goes through `wgf_io_t.call(io, name, args)`:
 *   - wgf_call appends {"name": name, "args": args} to io->trace (ordered);
 *   - returns the scripted result (a NEW json_t* the caller frees);
 *   - models a raised exception with {"__raise__": "<Type>", "message": "..."}
 *     where message == str(exc) EXACTLY (no errno component is scripted).
 *   - args passed to wgf_call are BORROWED (the caller frees them); the seam
 *     result is OWNED by the caller. NULL result == Python None; a JSON_NULL
 *     node is a real null value, distinct from a missing script.
 *
 * Live counterpart of each seam name:
 *   subprocess.run              subprocess.run(argv, cwd=..., capture_output=
 *                               True, text=True, encoding='utf-8', errors=
 *                               'replace', timeout=..., stdin=DEVNULL, env=...,
 *                               check=False) inside _run. Args projection:
 *                               {argv, cwd, timeout, env} (the fixed kwargs are
 *                               identical on every call and not recorded).
 *                               ret {"returncode","stdout","stderr"} | null.
 *                               _run catches (OSError, SubprocessError) -> None.
 *   harden_git_argv             hermes_cli._subprocess_compat.harden_git_argv(args)
 *                               args {args: [...]} -> [str, ...]
 *   noninteractive_repo_git_env _subprocess_compat.noninteractive_repo_git_env(cwd)
 *                               args {cwd} -> env object | null (null =>
 *                               (1, "", FILTER_DISCOVERY_FAILED) in _git)
 *   noninteractive_git_env      _subprocess_compat.noninteractive_git_env(base?)
 *                               args {} (no base) or {base: env} -> env object
 *   environ                     dict(os.environ) in _review_commit_env
 *                               args {} -> env object
 *   os.urandom                  os.urandom(4).hex() in worktree_add; bytes are
 *                               projected as their .hex() string on BOTH sides
 *                               args {n} -> hex string
 *   path.is_dir                 Path(cwd).is_dir() in _is_dir; args {path} with
 *                               path = Path(cwd).as_posix() (host Path model).
 *                               _is_dir catches OSError -> False.
 *   path.is_file                (Path(cwd)/file_path).is_file() in _all_add_diff;
 *                               args {path} = the joined Path.as_posix(). The
 *                               seam models the METHOD outcome (stdlib-internal
 *                               OSError ignoring already applied); raises
 *                               propagate (no live try at this call site).
 *   path.resolve                Path.resolve() in _single_file_diff; args
 *                               {path} = self.as_posix(); ret string is wrapped
 *                               back into Path() by the oracle. Path equality
 *                               is case-insensitive on Windows (the C compare
 *                               folds ASCII case under _WIN32; fixtures avoid
 *                               non-ASCII case-only differences).
 *   walk_entries                _walk_entries(raw) — CLAIMED port
 *                               (web_git_branch.c); seamed here. args {raw} ->
 *                               [[tag, xy, path], ...] (yielded tuples).
 *   untracked_insertions        _untracked_insertions(cwd, rel) — CLAIMED port;
 *                               seamed. args {cwd, rel} -> int
 *   sanitize_branch             _sanitize_branch(name) — CLAIMED port; seamed.
 *                               args {name} -> str
 *   unique_dir                  _unique_dir(base) — CLAIMED port; seamed.
 *                               args {base} -> str
 *   tracking_refspec            hermes_cli.update_cmd_check.tracking_refspec(
 *                               remote, branch) (in-function import in
 *                               _fetch_tracking_ref). args {remote, branch} -> str
 *   record_worktree_repo        hermes_cli.worktree_registry.record_worktree_repo(
 *                               root) (in-function import in worktree_add).
 *                               args {root} -> null
 *   path_allowed                the path_allowed(Path) callback parameter of
 *                               review_commit_context (only when the case
 *                               supplies it). args {path} = (Path(repo)/name)
 *                               .as_posix() -> bool
 *
 * Claimed siblings NOT re-ported here (seamed): _walk_entries,
 * _untracked_insertions, _sanitize_branch, _unique_dir (web_git_branch.c),
 * review_pr_list (web_git_gaps3.c), _gh (env_passthrough.c claim). The gh ship
 * flow is therefore out of scope for this core. Unclaimed internal helpers
 * (_git_line, _status_z, _origin_head*, _ref_exists, _is_dir, _entry_staged,
 * _classify, _status_letter, _has_staged, _review_result, _all_add_diff,
 * _single_file_diff, _review_push, _main_root, worktree_list, _slugify,
 * _default_branch, _ensure_repo, _remote_of_ref, _fetch_tracking_ref) are
 * ported REAL inside the core (no PoP credit) and extracted REAL by the
 * oracle, so their internal seam calls trace identically on both sides.
 *
 * Data model / representation notes
 * ---------------------------------
 * - Tuples project to JSON arrays: _git_in/_git/_git_write -> [code, out, err];
 *   _numstat values -> [added, removed]; walk_entries yields -> [tag, xy, path].
 * - None == JSON null. A scripted proc is {"returncode","stdout","stderr"};
 *   a missing/null field reads as None (live attribute access on the double).
 * - Exception descriptors {"__raise__": kind, "message": str(exc)} propagate
 *   verbatim. OSError family (OSError, FileNotFoundError, PermissionError,
 *   NotADirectoryError, IsADirectoryError, FileExistsError, BlockingIOError,
 *   InterruptedError, ProcessLookupError, TimeoutError, WindowsError) is caught
 *   by _run/_is_dir; SubprocessError family (SubprocessError, TimeoutExpired,
 *   CalledProcessError) by _run. BaseException kinds (SystemExit,
 *   KeyboardInterrupt, GeneratorExit) pass through EVERY catch site.
 * - Paths: the C core models the HOST Path (WindowsPath under _WIN32,
 *   PurePosixPath otherwise): drive/root parse, "."/empty component collapse,
 *   ".." kept, join replace rules; every Path reaches a seam via as_posix().
 *   os.path.join sites model ntpath.join under _WIN32 (mixed-separator output
 *   is faithful: ntpath.join("/r", ".worktrees", "s") == "/r\\.worktrees\\s"),
 *   posixpath.join otherwise. Fixture path domain: POSIX-absolute ("/r"),
 *   drive-absolute ("C:/r"), relative and empty strings; no UNC/drive-relative
 *   inputs (documented).
 * - str.strip() strips the full Unicode White_Space set (UTF-8 decoded).
 *   str.splitlines() splits on \n \r \r\n \v \f \x1c \x1d \x1e \x85 U+2028 U+2029.
 * - int() parsing: ASCII digits with optional sign, surrounding Unicode
 *   whitespace and between-digit underscores; ValueError message is the exact
 *   CPython 3.14 "invalid literal for int() with base 10: '<repr>'" (ASCII
 *   repr domain). The identity regex \d and .lower() are ASCII-domain
 *   (fixtures avoid Unicode digits / non-ASCII case mappings).
 * - Integral doubles serialize int-like (libjson %.0f); fixtures use integer
 *   literals so int/float identity never diverges.
 * - Dict key order is insertion order (libjson json_set keeps position on
 *   replace); _numstat iteration order therefore matches Python dict order.
 * - _review_result sorts STABLY by path (UTF-8 byte order == codepoint order).
 *
 * Ownership: every wgf_* target returns a NEW json_t* (caller frees). Inputs
 * are borrowed. Internal helpers return NEW nodes; a raise descriptor IS the
 * return value (wgf_raised detects it) and propagates up the call chain.
 */
#ifndef PORT_WEB_GIT_FLOW_H
#define PORT_WEB_GIT_FLOW_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wgf_io_t {
    json_t *(*call)(struct wgf_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of {"name","args"}: ordered seam sequence */
    void   *ctx;     /* harness backend (scripted results) */
} wgf_io_t;

json_t *wgf_call(wgf_io_t *io, const char *name, json_t *args);
bool wgf_raised(const json_t *result);

/* Uniform port contract: json in -> json out (NEW node, caller frees).
 * PoP-claimed targets (18): */
json_t *wgf__run(wgf_io_t *io, json_t *in);
json_t *wgf__git_in(wgf_io_t *io, json_t *in);
json_t *wgf__git(wgf_io_t *io, json_t *in);
json_t *wgf__git_write(wgf_io_t *io, json_t *in);
json_t *wgf__git_out(wgf_io_t *io, json_t *in);
json_t *wgf__git_ok(wgf_io_t *io, json_t *in);
json_t *wgf_resolve_rename_path(wgf_io_t *io, json_t *in);
json_t *wgf__numstat(wgf_io_t *io, json_t *in);
json_t *wgf__branch_base(wgf_io_t *io, json_t *in);
json_t *wgf__default_branch_name(wgf_io_t *io, json_t *in);
json_t *wgf_repo_status(wgf_io_t *io, json_t *in);
json_t *wgf_review_list(wgf_io_t *io, json_t *in);
json_t *wgf_review_diff(wgf_io_t *io, json_t *in);
json_t *wgf__review_commit_env(wgf_io_t *io, json_t *in);
json_t *wgf_review_commit(wgf_io_t *io, json_t *in);
json_t *wgf_review_commit_context(wgf_io_t *io, json_t *in);
json_t *wgf_worktree_add(wgf_io_t *io, json_t *in);
json_t *wgf__worktree_for_existing(wgf_io_t *io, json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* PORT_WEB_GIT_FLOW_H */
