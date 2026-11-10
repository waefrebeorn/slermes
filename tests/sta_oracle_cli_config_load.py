#!/usr/bin/env python3
"""Oracle for hermes_cli/cli_config_load.py pure config helpers.

_merge_file_config is ast-extracted and run live on a real dict. The tier
parser delegates to agent.fast_mode: we ast-extract fast_mode's table +
parse_service_tier into a stub module seeded in sys.modules so the real body
runs.
"""
import ast
import json
import re
import sys

FMLOAD = "C:/Users/eman5/hermes-agent/agent/cli_config_load.py"
LOAD = "C:/Users/eman5/hermes-agent/hermes_cli/cli_config_load.py"


def _fm_ns():
    ns = {"re": re}
    src = "C:/Users/eman5/hermes-agent/agent/fast_mode.py"
    tree = ast.parse(open(src, encoding="utf-8").read())
    for n in tree.body:
        if isinstance(n, ast.Assign):
            ts = n.targets
            for t in ts:
                if isinstance(t, ast.Name) and t.id in (
                        "NORMAL_TIER_WORDS", "SERVICE_TIER_WORDS"):
                    exec(compile(ast.Module(body=[n], type_ignores=[]),
                                 "x", "exec"), ns)
        elif isinstance(n, ast.AnnAssign) and isinstance(n.target, ast.Name) \
                and n.target.id == "SERVICE_TIER_WORDS":
            exec(compile(ast.Module(body=[n], type_ignores=[]), "x", "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name == "parse_service_tier":
            exec(compile(ast.Module(body=[n], type_ignores=[]), "x", "exec"), ns)
    return ns


def _load_ns():
    ns = {"Any": __import__("typing", fromlist=["Any"]).Any}
    tree = ast.parse(open(LOAD, encoding="utf-8").read())
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in (
                "_merge_file_config", "_parse_service_tier_config"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), "x", "exec"), ns)
    return ns


class _Logger:
    def warning(self, *a): pass
    log = warning


def _seed_fast_mode():
    import types
    fm = _fm_ns()
    m = types.ModuleType("agent.fast_mode")
    m.NORMAL_TIER_WORDS = fm["NORMAL_TIER_WORDS"]
    m.SERVICE_TIER_WORDS = fm["SERVICE_TIER_WORDS"]
    m.parse_service_tier = fm["parse_service_tier"]
    sys.modules["agent"] = types.ModuleType("agent")
    sys.modules["agent.fast_mode"] = m


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    _seed_fast_mode()
    NS = _load_ns()
    merge = NS["_merge_file_config"]
    pst = NS["_parse_service_tier_config"]
    NS["logger"] = _Logger()

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE cli_config_load: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "pst":
            want = pst(c["in"])
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH pst {c['case']}: C={c['out']!r} PY={want!r} "
                      f"in={c['in']!r}")
        elif fam == "merge":
            defaults = c["defaults"]  # emitted raw -> already a dict
            filec = json.loads(c["file"])  # emitted via jstr -> string
            merge(defaults, filec)
            want = json.loads(json.dumps(defaults))
            got = c["out"]
            if got != want:
                mism += 1
                print(f"MISMATCH merge {c['case']}: C={got} PY={want} "
                      f"file={c['file']!r}")
    print(f"ORACLE cli_config_load: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
