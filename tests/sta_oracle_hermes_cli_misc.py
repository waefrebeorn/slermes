#!/usr/bin/env python3
"""Oracle for the three thin pure helpers.

_display_source and _safe_int come from modules that cannot import (auth_commands,
context_compressor_telemetry) so they are ast-extracted from source. configured_backend
is compared against the LIVE delegation_registry module.
"""
import ast
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")


def _extract(path, wanted):
    ns = {}
    tree = ast.parse(open(path, encoding="utf-8").read())
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in wanted:
            exec(compile(ast.fix_missing_locations(ast.Module(body=[n], type_ignores=[])),
                         "x", "exec"), ns)
    return {k: ns[k] for k in wanted}


DC_N = _extract("C:/Users/eman5/hermes-agent/hermes_cli/auth_commands.py",
                {"_display_source"})
SI_N = _extract("C:/Users/eman5/hermes-agent/agent/context_compressor_telemetry.py",
                {"_safe_int"})

import agent.delegation_registry as DR  # noqa: E402


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

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
        print("ORACLE cli_misc: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam, case = c["family"], c["case"]
        if fam == "displaysrc":
            src = "" if c["in"] is None else c["in"]
            want = DC_N["_display_source"](src)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH {case}: C={c['out']!r} PY={want!r} in={c['in']!r}")
        elif fam == "backend":
            cfg = c["in"]  # already decoded by the outer json.loads
            want = DR.configured_backend(cfg)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH {case}: C={c['out']!r} PY={want!r} in={c['in']!r}")
        elif fam == "safeint":
            v = c["in"]  # already decoded
            want = SI_N["_safe_int"](v)
            got = None if c["ret"] == -1 else c["ret"]
            if got != want:
                mism += 1
                print(f"MISMATCH {case}: C={got} PY={want} in={c['in']!r}")

    print(f"ORACLE cli_misc: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
