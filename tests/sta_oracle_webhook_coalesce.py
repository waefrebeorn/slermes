#!/usr/bin/env python3
"""Oracle for the pure core of gateway/platforms/webhook_coalesce.py.

validate_coalesce_config is ast-extracted and run live on a real dict. The two
short text helpers (prompt_with_note, group_key core) are trivial
source-transcribed references in this file.
"""
import ast
import json
import re
import sys

SRC = "C:/Users/eman5/hermes-agent/gateway/platforms/webhook_coalesce.py"

_PLACEHOLDER = re.compile(r"\{[a-zA-Z0-9_.]+\}")


def _extract(path, want):
    ns = {}
    tree = ast.parse(open(path, encoding="utf-8").read())
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name == want:
            exec(compile(ast.Module(body=[n], type_ignores=[]), "x", "exec"), ns)
    return ns[want]


validate_coalesce_config = _extract(SRC, "validate_coalesce_config")


def _prompt_with_note(prompt, count):
    if count <= 1:
        return prompt
    return (f"{prompt}\n\n(Note: {count} webhook events for this item arrived "
            "in quick succession and were coalesced — this is the most recent "
            "one; earlier events are superseded.)")


def _group_key(route_name, rendered):
    if _PLACEHOLDER.search(rendered):
        return None
    return f"{route_name}|{rendered}"


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
        print("ORACLE webhook_coalesce: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "pn":
            want = _prompt_with_note(c["prompt"], c["count"])
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH {c['case']}: C={c['out']!r} PY={want!r}")
        elif fam == "gk":
            want = _group_key(c["route"], c["rendered"])
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH {c['case']}: C={c['out']!r} PY={want!r} "
                      f"route={c['route']!r} rendered={c['rendered']!r}")
        elif fam == "vc":
            import json as _json
            route = _json.loads(c["route"])
            try:
                validate_coalesce_config(c["route_name"], route)
                err = None
            except ValueError as exc:
                err = str(exc)
            if c["err"] != err:
                mism += 1
                print(f"MISMATCH {c['case']}: C={c['err']!r} PY={err!r} "
                      f"route={c['route']!r}")
            elif (c["code"] != 0) != (err is not None):
                mism += 1
                print(f"MISMATCH {c['case']}: C code={c['code']} but err "
                      f"present={err is not None}")
    print(f"ORACLE webhook_coalesce: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
