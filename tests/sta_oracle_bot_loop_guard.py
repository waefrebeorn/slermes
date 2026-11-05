#!/usr/bin/env python3
"""Oracle for gateway/bot_loop_guard.py pure coercion + settings helpers.

The three _as_* coercions are ast-extracted and run live; settings_from_config
is reproduced from them with the BotLoopGuardSettings defaults (which is
exactly what its body does).
"""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/gateway/bot_loop_guard.py"

# BotLoopGuardSettings defaults
DEF_ENABLED = True
DEF_MAX = 20
DEF_WINDOW = 300.0
DEF_COOLDOWN = 600.0


def _extract(wants):
    ns = {}
    tree = ast.parse(open(SRC, encoding="utf-8").read())
    # module-global frozensets _TRUTHY / _FALSY (plain Assigns)
    for n in tree.body:
        if isinstance(n, ast.Assign):
            for t in n.targets:
                if isinstance(t, ast.Name) and t.id in ("_TRUTHY", "_FALSY"):
                    exec(compile(ast.Module(body=[n], type_ignores=[]),
                                 "x", "exec"), ns)
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in wants:
            exec(compile(ast.Module(body=[n], type_ignores=[]), "x", "exec"), ns)
    return {w: ns[w] for w in wants}


NS = _extract({"_as_bool", "_as_positive", "_as_positive_int"})
_as_bool = NS["_as_bool"]
_as_positive = NS["_as_positive"]
_as_positive_int = NS["_as_positive_int"]


def _settings(block):
    if not isinstance(block, dict):
        return (DEF_ENABLED, DEF_MAX, DEF_WINDOW, DEF_COOLDOWN)
    return (_as_bool(block.get("enabled"), DEF_ENABLED),
            _as_positive_int(block.get("max_events"), DEF_MAX),
            _as_positive(block.get("window_seconds"), DEF_WINDOW),
            _as_positive(block.get("cooldown_seconds"), DEF_COOLDOWN))


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
        print("ORACLE bot_loop_guard: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "bool":
            want = _as_bool(c["raw"], c["def"])
            if c["out"] != (1 if want else 0):
                mism += 1
                print(f"MISMATCH bool {c['case']}: C={c['out']} PY={want} "
                      f"raw={c['raw']!r}")
        elif fam == "pos":
            want = _as_positive(c["raw"], c["def"])
            if c["out"] is None or c["out"] != want:
                mism += 1
                print(f"MISMATCH pos {c['case']}: C={c['out']!r} PY={want} "
                      f"raw={c['raw']!r}")
        elif fam == "pint":
            want = _as_positive_int(c["raw"], c["def"])
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH pint {c['case']}: C={c['out']} PY={want} "
                      f"raw={c['raw']!r}")
        elif fam == "set":
            e, m, w, co = _settings(c["block"])
            want = {"enabled": e, "max_events": m, "window_seconds": w,
                    "cooldown_seconds": co}
            got = c["out"]
            if got != want:
                mism += 1
                print(f"MISMATCH set {c['case']}: C={got} PY={want} "
                      f"block={c['block']!r}")
    print(f"ORACLE bot_loop_guard: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
