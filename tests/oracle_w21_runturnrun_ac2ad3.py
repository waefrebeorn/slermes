#!/usr/bin/env python3
"""Differential oracle for gateway/run_turn_runner_approval_transport.py
(gate w21_runturnrun_ac2ad3).

The module's entry point builds live turn plumbing (a cloned `TurnContext`, a
detached `TurnRunner`, an adapter resolver), so the oracle executes the two
pieces the decision is actually made of, live:

* `NON_MESSAGING_SESSION_SURFACES` is lifted out of
  `gateway/session_context.py` by AST and evaluated as the frozenset it is;
* the attribute walk and the predicate are the module's own expressions, run
  here against duck-typed objects built from the fixture's JSON (`SimpleNamespace`
  carries `.platform` / `.value` exactly like the adapter does, and a bare string
  or number has no `.value`, which is the trap the C port has to reproduce).
"""
import ast
import json
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w21_runturnrun_ac2ad3"
SURFACE_SRC = QUARRY / "gateway" / "session_context.py"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

G = {"__name__": "w21_session_context_live", "frozenset": frozenset}
tree = ast.parse(SURFACE_SRC.read_text(encoding="utf-8"))
_found = False
for node in ast.walk(tree):
    if isinstance(node, ast.Assign):
        names = [t.id for t in node.targets if isinstance(t, ast.Name)]
        if "NON_MESSAGING_SESSION_SURFACES" in names:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SURFACE_SRC), "exec"), G)
            _found = True
if not _found:
    raise SystemExit("NON_MESSAGING_SESSION_SURFACES not found in %s" % SURFACE_SRC)
SURFACES = G["NON_MESSAGING_SESSION_SURFACES"]


def make_source(j):
    """The JSON the C side receives, as the object graph the module walks."""
    src = SimpleNamespace()
    if not isinstance(j, dict):
        return src
    if "platform" in j:
        p = j["platform"]
        if isinstance(p, dict):
            inner = SimpleNamespace()
            if "value" in p:
                inner.value = p["value"]
            src.platform = inner
        else:
            src.platform = p
    return src


def platform_value(j):
    """`str(getattr(getattr(source, "platform", None), "value", "") or "")`."""
    src = make_source(j)
    return str(getattr(getattr(src, "platform", None), "value", "") or "")


def expected(r):
    f = r.get("func")
    if f == "surfaces":
        return sorted(SURFACES)
    if f == "platform_value":
        return platform_value(r.get("source"))
    if f == "transport":
        platform = platform_value(r.get("source"))
        attached = platform not in SURFACES and not bool(r.get("local_viewer"))
        return {"platform": platform, "attached": attached}
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"surfaces", "platform_value", "transport"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
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
