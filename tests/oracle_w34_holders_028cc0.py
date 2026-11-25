#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/_state/holders.py (gate w34_holders_028cc0).

holders.py answers one question -- does a foreign process still hold state.db
or a WAL sidecar? -- and the answer is a pure decision over three kinds of
value: an argv, the install layout of the store's home, and a holder list.

Every I/O seam of the live module is replaced with the case's own value, so the
live decision is what is compared, never the host:

  argv (/proc/<pid>/cmdline, psutil)      <- live._read_proc_argv
  install layout (hermes_constants)       <- live._store_install_layout
  the holder scan (lsof/psutil/RM)        <- live.foreign_state_db_holders
  profile_cli_selector() / docs URL       <- hermes_constants, _state.errors

Path semantics are the host's own on both sides (ntpath here), which is why the
same fixtures describe Windows and POSIX behaviour without special-casing.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w34_holders_028cc0"

# tests/hermes_cli and tests/tools shadow the quarry: quarry first, always.
if sys.path[:1] != [str(QUARRY)]:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

from persistence.sqlite._state import holders as live  # noqa: E402

WANTED = {
    "looks_like_python_executable",
    "python_execution_target",
    "looks_like_hermes",
    "argv_flag_value",
    "argv_env_home",
    "sqlite_family",
    "norm_path",
    "canonical_sqlite_path",
    "argv_path_tokens",
    "names_other_profile",
    "argv_home_selection",
    "argv_scoped_to_other_home",
    "describe_holder_pid",
    "held_store_refusal",
}


def _argv(req):
    raw = req.get("argv")
    return list(raw) if isinstance(raw, list) else []


def looks_like_python_executable(req):
    program = req.get("program")
    return live._looks_like_python_executable("" if program is None else str(program))


def python_execution_target(req):
    target = live._python_execution_target(_argv(req))
    if target is None:
        return None
    kind, value = target
    return {"kind": kind, "value": value}


def looks_like_hermes(req):
    return live._looks_like_hermes(_argv(req))


def argv_flag_value(req):
    flags = req.get("flags")
    return live._argv_flag_value(_argv(req), tuple(flags) if isinstance(flags, list) else ())


def argv_env_home(req):
    return live._argv_env_home(_argv(req))


def sqlite_family(req):
    base = req.get("base")
    return list(live._sqlite_family("" if base is None else str(base)))


def norm_path(req):
    value = req.get("value")
    return live._norm_path("" if value is None else str(value))


def canonical_sqlite_path(req):
    path = req.get("path")
    return live.canonical_sqlite_path("" if path is None else str(path))


def argv_path_tokens(req):
    return [[int(index), normalized] for index, normalized in live._argv_path_tokens(_argv(req))]


def names_other_profile(req):
    return live._names_other_profile(
        req.get("normalized") or "",
        req.get("install_root"),
        req.get("our_profile"),
    )


def argv_home_selection(req):
    return live._argv_home_selection(
        _argv(req),
        req.get("this_home") or "",
        req.get("install_root"),
        req.get("our_profile"),
    )


def argv_scoped_to_other_home(req):
    # the install-layout seam: the fixture declares what the live resolver returns
    install_root = req.get("install_root")
    our_profile = req.get("our_profile")
    live._store_install_layout = lambda this_home: (install_root, our_profile)
    return live._argv_scoped_to_other_home(_argv(req), Path(req.get("db_path") or ""))


def describe_holder_pid(req):
    # the argv seam: /proc and psutil are replaced by the fixture's argv
    argv = req.get("argv")
    live._read_proc_argv = lambda pid: None if argv is None else list(argv)
    live.psutil = None
    return live.describe_holder_pid(int(req.get("pid") or 0))


def held_store_refusal(req):
    import hermes_constants
    import persistence.sqlite._state.errors as errors

    holders = req.get("holders")
    pairs = [(int(pid), str(target)) for pid, target in (holders if isinstance(holders, list) else [])]
    live.foreign_state_db_holders = lambda db_path: list(pairs)
    live._read_proc_argv = lambda pid: None
    live.psutil = None
    hermes_constants.profile_cli_selector = lambda: req.get("profile_arg") or ""
    errors.STORAGE_RECOVERY_DOCS_URL = req.get("docs_url") or ""
    return live.held_store_refusal(
        Path(req.get("db_path") or ""),
        command=req.get("command") or "",
        force_hint=req.get("force_hint"),
    )


CASES = {
    "looks_like_python_executable": looks_like_python_executable,
    "python_execution_target": python_execution_target,
    "looks_like_hermes": looks_like_hermes,
    "argv_flag_value": argv_flag_value,
    "argv_env_home": argv_env_home,
    "sqlite_family": sqlite_family,
    "norm_path": norm_path,
    "canonical_sqlite_path": canonical_sqlite_path,
    "argv_path_tokens": argv_path_tokens,
    "names_other_profile": names_other_profile,
    "argv_home_selection": argv_home_selection,
    "argv_scoped_to_other_home": argv_scoped_to_other_home,
    "describe_holder_pid": describe_holder_pid,
    "held_store_refusal": held_store_refusal,
}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = CASES[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
