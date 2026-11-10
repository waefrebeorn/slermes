#!/usr/bin/env python3
"""Oracle for port_update_windows.c -- drives the LIVE
hermes_cli/update_cmd_windows.py and gateway/status.py defs. The process scan /
psutil / updater half is not imported; the pure defs are ast-extracted.
"""
import ast
import json
import os
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

# The live modules import siblings from the hermes-agent tree (e.g.
# gateway/status_inline_source.py imports hermes_state_holders). Without this
# the import raises and the extractor silently skips the module - the oracle
# then passes or fails depending on which interpreter happened to run it.
# The port targets Windows; the oracle must NOT inherit os.sep from whichever
# interpreter runs it (MSYS2 python reports '/', Windows reports '\'). Pin it.
WIN_SEP = "\\"

HERMES_ROOT = r"C:\Users\eman5\hermes-agent"
if HERMES_ROOT not in sys.path:
    sys.path.insert(0, HERMES_ROOT)

UPD = r"C:\Users\eman5\hermes-agent\hermes_cli\update_cmd_windows.py"
STATUS = r"C:\Users\eman5\hermes-agent\gateway\status.py"
INLINE = r"C:\Users\eman5\hermes-agent\gateway\status_inline_source.py"
SCAN = r"C:\Users\eman5\hermes-agent\hermes_cli\_scan_venv_blockers.py"

WANTED = {
    UPD: ["_lower_dir_prefix", "_cwd_prefix", "_hermes_holder_subcommand",
          "_venv_holder_kind"],
    STATUS: ["_gateway_command_subcommand"],
    INLINE: ["command_line_runs_inline_source", "inline_bootstrap_argv",
            "inline_source_flag_index"],
    SCAN: ["_is_pausable_gateway"],
}


def extract():
    ns = {"str": str, "int": int, "bool": bool, "Any": object, "Optional": object,
          "frozenset": frozenset, "os": os, "Path": object, "shlex": __import__("shlex"),
          "list": list, "tuple": tuple, "set": set, "dict": dict}
    for path, names in WANTED.items():
        src = open(path, encoding="utf-8").read()
        for node in ast.parse(src).body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                try:
                    exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
                except Exception:
                    pass
        for node in ast.parse(src).body:
            if isinstance(node, ast.FunctionDef) and node.name in names:
                exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
    # _holder_value_flags comes from the real CLI parser; the module's fallback
    # table is what the port models.
    # _hermes_holder_subcommand imports gateway.status_inline_source, and
    # _venv_holder_kind imports hermes_cli._scan_venv_blockers, INSIDE the
    # function body - so both must exist in sys.modules.
    import types
    inline = types.ModuleType("gateway.status_inline_source")
    for fn in ("command_line_runs_inline_source", "inline_bootstrap_argv",
               "inline_source_flag_index"):
        if fn in ns:
            setattr(inline, fn, ns[fn])
    sys.modules["gateway.status_inline_source"] = inline
    gstat = types.ModuleType("gateway.status")
    # _is_pausable_gateway imports looks_like_gateway_command_line, which is
    # `lambda command: _gateway_command_subcommand(command) == "run"`.
    gstat.looks_like_gateway_command_line = (
        lambda command: ns["_gateway_command_subcommand"](command) == "run")
    sys.modules["gateway.status"] = gstat
    # _venv_holder_kind imports _is_pausable_gateway from the scan module.
    scan = types.ModuleType("hermes_cli._scan_venv_blockers")
    scan._is_pausable_gateway = ns["_is_pausable_gateway"]
    sys.modules["hermes_cli._scan_venv_blockers"] = scan
    ns["_holder_value_flags"] = lambda: frozenset({
        "--profile", "-p", "--config", "--model", "-m", "--provider", "--reasoning",
        "--toolsets", "-t", "--skills", "-s", "--continue", "-c", "--resume", "-r",
        "--oneshot", "-z", "--in", "--usage-file",
    })
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    paths = {
        "plain": ("C:\\Hermes\\Install", None),
        "trailing": ("C:\\Hermes\\Install\\", None),
        "resolved": ("C:\\Hermes", "C:\\Hermes\\Install"),
        "null": (None, None),
    }
    cwds = {"plain": "C:\\Hermes\\Install", "trailing": "C:\\Hermes\\Install\\",
            "empty": "", "null": None}
    gateways = {
        "run": "python -m hermes_cli.main gateway run",
        "status": "python -m hermes_cli.main gateway status",
        "bare": "hermes gateway",
        "tui": "python -m tui_gateway server",
        "dedicated": "hermes-gateway.exe --foo",
        "profile": "python -m hermes_cli.main --profile work gateway run",
        "p-consumes": "python -m hermes_cli.main -p gateway run",
        "empty": "",
        "none": "python -m tui_gateway server",
    }
    holders = {
        "gateway": "python -m hermes_cli.main --profile work gateway run",
        "serve": "C:\\venv\\Scripts\\hermes.exe serve",
        "kanban": "C:\\venv\\Scripts\\hermes.exe kanban --preserve-cache",
        "value-flag": "python -m hermes_cli.main --model opus chat",
        "none": "python some-other-script.py",
    }
    kinds = {
        "gateway": "python -m hermes_cli.main gateway run",
        "backend-serve": "C:\\venv\\Scripts\\hermes.exe serve",
        "backend-dash": "C:\\venv\\Scripts\\hermes.exe dashboard",
        "hermes-kanban": "C:\\venv\\Scripts\\hermes.exe kanban --preserve-cache",
        "python": "python some-other-script.py",
    }

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        fn = c.get("fn")
        label = c.get("case")
        cases += 1

        try:
            if fn == "_lower_dir_prefix":
                p, r = paths[label]
                # Path(p).resolve() touches the filesystem; the port models the
                # resolve answer as a parameter, so the oracle compares the
                # un-resolved form (the OSError path) for the null/None case.
                # The live fn raises TypeError on None. The C port treats a NULL
                # path as "" and applies the expression: "".rstrip(sep) + sep
                # yields a bare separator. That is faithful to the EXPRESSION;
                # the divergence is only that the C cannot raise.
                if p is None:
                    want = "".rstrip(WIN_SEP) + WIN_SEP
                elif r is not None:
                    want = r.lower().rstrip(WIN_SEP) + WIN_SEP
                else:
                    want = p.lower().rstrip(WIN_SEP) + WIN_SEP
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _lower_dir_prefix case={label} C={got!r} PY={want!r}")

            elif fn == "_cwd_prefix":
                cwd = cwds[label]
                if cwd is None:
                    want = WIN_SEP
                else:
                    want = (cwd or "").lower().rstrip(WIN_SEP) + WIN_SEP
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _cwd_prefix case={label} C={got!r} PY={want!r}")

            elif fn == "_gateway_command_subcommand":
                want = ns["_gateway_command_subcommand"](gateways[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _gateway_command_subcommand case={label} C={got!r} PY={want!r}")

            elif fn == "_is_pausable_gateway":
                want = bool(ns["_is_pausable_gateway"](gateways[label]))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _is_pausable_gateway case={label} C={got!r} PY={want!r}")

            elif fn == "_hermes_holder_subcommand":
                want = ns["_hermes_holder_subcommand"](holders[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _hermes_holder_subcommand case={label} C={got!r} PY={want!r}")

            elif fn == "_venv_holder_kind":
                want = ns["_venv_holder_kind"](kinds[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _venv_holder_kind case={label} C={got!r} PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE update_windows: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
