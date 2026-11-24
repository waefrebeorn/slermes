#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/status_public.py
(gate w21_statuspubl_e796de).

The live module's own source runs: `_anonymous_platform_entry` and
`redact_status_for_unverified` are lifted as FunctionDefs, together with the
module's `_PUBLIC_PLATFORM_STATES` / `_PUBLIC_CODE_RE` assignments, so the
allowlist, the code regex and the in-place body rewrite are the real ones.
`normalize_updated_at` is the real gateway.status function (imported, not
restated); the local `classify_exit_reason` import inside the lifted function
resolves to the real agent.monitoring.gateway_health module.
"""
import ast
import importlib.util
import json
import re
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "web_routers" / "status_public.py"
GATE = "w21_statuspubl_e796de"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway.status import normalize_updated_at  # noqa: E402 - live dependency

G = {"__name__": "status_public_helpers", "re": re, "Any": object,
     "normalize_updated_at": normalize_updated_at}
WANT_FUNCS = {"_anonymous_platform_entry", "redact_status_for_unverified"}
WANT_ASSIGNS = {"_PUBLIC_PLATFORM_STATES", "_PUBLIC_CODE_RE"}

_tree = ast.parse(SRC.read_text(encoding="utf-8"))
for _node in _tree.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANT_FUNCS:
        exec(compile(ast.Module(body=[_node], type_ignores=[]), str(SRC), "exec"), G)
for _node in _tree.body:
    if isinstance(_node, (ast.Assign, ast.AnnAssign)):
        _targets = _node.targets if isinstance(_node, ast.Assign) else [_node.target]
        if any(isinstance(t, ast.Name) and t.id in WANT_ASSIGNS for t in _targets):
            exec(compile(ast.Module(body=[_node], type_ignores=[]), str(SRC), "exec"), G)

missing = (WANT_FUNCS | WANT_ASSIGNS) - set(G)
if missing:
    raise SystemExit("oracle setup failed, missing %r" % (sorted(missing),))


def expected(r):
    if r.get("func") == "entry":
        return G["_anonymous_platform_entry"](r.get("value"))
    status = json.loads(json.dumps(r.get("status")))
    G["redact_status_for_unverified"](status)
    return status


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
        if not isinstance(r, dict) or r.get("func") not in {"entry", "redact"}:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001 - a live raise is a harness bug
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
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
