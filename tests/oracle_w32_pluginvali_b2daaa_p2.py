#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugin_validate_core_override.py
(gate w32_pluginvali_b2daaa_p2).

Second half of the same lint: the AST->text and AST->decision helpers that turn
a receiver chain into a finding and decide whether a call site is the plugin's
own container or a same-named helper, plus the report message shaping.

  * ``_render`` / ``_dotted`` -- how a chain names itself in a finding;
  * ``_has_private_segment`` / ``_has_mutator_call`` -- the private-state sink
    tests a receiver chain has to pass;
  * ``_Analysis._func_key`` / ``_Analysis.helper_indices`` -- which helper a
    call site is, and the container-receiver exemption;
  * ``check_core_override`` -- the verdict label and its 8-hit truncation.

The plugin directory scan is the seam: ``core_override_findings`` is replaced
per case with the fixture's hit list, so the live function shapes the message
exactly as it ships.
"""
import ast
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "plugin_validate_core_override.py"
GATE = "w32_pluginvali_b2daaa_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))
try:  # host-blocked compiled extensions behind the CLI stack
    from hermes_seam_stubs import install as _install_seams

    _install_seams()
except Exception:  # noqa: BLE001
    pass

_spec = importlib.util.spec_from_file_location("pvc_live_p2", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["pvc_live_p2"] = live
_spec.loader.exec_module(live)

WANTED = {"render", "dotted", "has_private_segment", "has_mutator_call",
          "func_key", "helper_indices", "report"}

DISPLAYS = {
    "dict": lambda: ast.Dict(keys=[], values=[]),
    "list": lambda: ast.List(elts=[], ctx=ast.Load()),
    "set": lambda: ast.Set(elts=[]),
    "tuple": lambda: ast.Tuple(elts=[], ctx=ast.Load()),
}


def chain(spec):
    kind = spec.get("kind") if isinstance(spec, dict) else None
    if kind in DISPLAYS:  # a container display can be a receiver (``[...].append``)
        return DISPLAYS[kind]()
    if kind == "name":
        return ast.Name(id=str(spec.get("id")), ctx=ast.Load())
    if kind == "attr":
        return ast.Attribute(value=chain(spec.get("value") or {}), attr=str(spec.get("attr")),
                             ctx=ast.Load())
    if kind == "sub":
        return ast.Subscript(value=chain(spec.get("value") or {}), slice=ast.Constant(0),
                             ctx=ast.Load())
    if kind == "call":
        return ast.Call(func=chain(spec.get("func") or {}), args=[], keywords=[])
    if kind == "const":
        return ast.Constant(spec.get("value"))
    if kind == "binop":
        return ast.BinOp(left=ast.Constant(1), op=ast.Add(), right=ast.Constant(2))
    return ast.Constant(None)


def node(spec):
    kind = spec.get("kind") if isinstance(spec, dict) else None
    if kind in DISPLAYS:
        return DISPLAYS[kind]()
    return chain(spec)


def dotted_node(path):
    """The func node of a call written as a dotted source string."""
    path = path or ""
    if not path:
        return ast.Constant(None)
    parts = path.split(".")
    cur = ast.Name(id=parts[0], ctx=ast.Load())
    for part in parts[1:]:
        cur = ast.Attribute(value=cur, attr=part, ctx=ast.Load())
    return cur


class FakeReport:
    def __init__(self):
        self.adds = []

    def add(self, label, ok, detail):
        self.adds.append((label, bool(ok), detail))


def expected(r):
    f = r.get("func")
    if f == "render":
        return live._render(chain(r.get("chain") or {}))
    if f == "dotted":
        return live._dotted(chain(r.get("chain") or {}))
    if f == "has_private_segment":
        return live._has_private_segment(chain(r.get("chain") or {}))
    if f == "has_mutator_call":
        return live._has_mutator_call(chain(r.get("chain") or {}))
    if f == "func_key":
        a = live._Analysis.__new__(live._Analysis)
        a.aliases = dict(r.get("aliases") or {})
        call = ast.Call(func=dotted_node(r.get("dotted")), args=[], keywords=[])
        return a._func_key(call)
    if f == "helper_indices":
        a = live._Analysis.__new__(live._Analysis)
        a.aliases = dict(r.get("aliases") or {})
        a.containers = frozenset(r.get("containers") or ())
        a.rebound_ctors = frozenset(r.get("rebound") or ())
        table = {name: {int(i) for i in (indexes or ())}
                 for name, indexes in (r.get("table") or {}).items()}
        return sorted(a.helper_indices(table, node(r.get("call") or {})))
    if f == "report":
        hits = list(r.get("hits") or ())
        live.core_override_findings = lambda plugin_dir, _h=hits: list(_h)
        report = FakeReport()
        live.check_core_override(report, Path("plugin"))
        label, ok, detail = report.adds[0]
        return {"label": label, "ok": ok, "detail": detail}
    raise AssertionError("unknown func %r" % (f,))


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
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
