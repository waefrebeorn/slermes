#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugin_validate_core_override.py
(gate w32_pluginvali_b2daaa_p1).

The live admission lint is imported by file path (the ``hermes_cli`` package
__init__ drags the CLI stack) and the plugin it would scan is a seam: no
directory walk, no ``ast.parse``.  What is compared is the value layer those
scans lean on --

  * the file-admission predicates (``_is_test_file`` / ``_python_files``);
  * the core-name predicates (``_Analysis._core_module_name``,
    ``_lookup_is_core`` over a literal / a module constant / an f-string head);
  * the core-root set algebra (``_hermes_top_level`` + ``_Analysis.__init__``);
  * the container exemption (``_Analysis.is_container``);
  * the binding-name collector (``_bound_names``);
  * the module constants the whole pass is tuned with.

Every case builds a real ``ast`` node (or a real ``_Analysis``) and calls the
live function: the node construction is the fixture's job, the decision is the
truth module's.
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
GATE = "w32_pluginvali_b2daaa_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))
try:  # host-blocked compiled extensions behind the CLI stack
    from hermes_seam_stubs import install as _install_seams

    _install_seams()
except Exception:  # noqa: BLE001
    pass

_spec = importlib.util.spec_from_file_location("pvc_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["pvc_live"] = live
_spec.loader.exec_module(live)

WANTED = {
    "is_test_file", "should_skip", "core_module_name", "core_roots",
    "lookup_is_core", "is_container", "bound_names", "module_constants",
}

DISPLAYS = {
    "dict": lambda: ast.Dict(keys=[], values=[]),
    "list": lambda: ast.List(elts=[], ctx=ast.Load()),
    "set": lambda: ast.Set(elts=[]),
    "dictcomp": lambda: ast.DictComp(key=ast.Constant(0), value=ast.Constant(0), generators=[]),
    "listcomp": lambda: ast.ListComp(elt=ast.Constant(0), generators=[]),
    "setcomp": lambda: ast.SetComp(elt=ast.Constant(0), generators=[]),
    "tuple": lambda: ast.Tuple(elts=[], ctx=ast.Load()),
}


def chain(spec):
    """A name / attribute / subscript / call chain, or a plain atom."""
    kind = spec.get("kind") if isinstance(spec, dict) else None
    if kind == "name":
        return ast.Name(id=str(spec.get("id")), ctx=ast.Load())
    if kind == "attr":
        return ast.Attribute(value=chain(spec.get("value") or {}), attr=str(spec.get("attr")), ctx=ast.Load())
    if kind == "sub":
        return ast.Subscript(value=chain(spec.get("value") or {}), slice=ast.Constant(0), ctx=ast.Load())
    if kind == "call":
        return ast.Call(func=chain(spec.get("func") or {}), args=[], keywords=[])
    if kind == "const":
        return ast.Constant(spec.get("value"))
    if kind == "binop":
        return ast.BinOp(left=ast.Constant(1), op=ast.Add(), right=ast.Constant(2))
    return ast.Constant(None)


def node(spec):
    """Any node ``_Analysis.is_container`` / ``_bound_names`` may be handed."""
    kind = spec.get("kind") if isinstance(spec, dict) else None
    if kind in ("tuple", "list") and "elts" in (spec or {}):
        cls = ast.Tuple if kind == "tuple" else ast.List
        return cls(elts=[node(e) for e in (spec.get("elts") or ())], ctx=ast.Load())
    if kind in DISPLAYS:
        return DISPLAYS[kind]()
    if kind == "call":
        return ast.Call(func=chain(spec.get("func") or {}), args=[], keywords=[])
    return chain(spec)


def lookup_arg(spec):
    """The module-name expression ``_lookup_is_core`` judges."""
    kind = spec.get("kind") if isinstance(spec, dict) else None
    if kind == "literal":
        return ast.Constant(spec.get("value"))
    if kind == "name":
        return ast.Name(id=str(spec.get("id")), ctx=ast.Load())
    if kind == "fstring":  # JoinedStr([Constant(head), FormattedValue])
        if "head" not in spec:
            return ast.JoinedStr(values=[ast.FormattedValue(value=ast.Name(id="p", ctx=ast.Load()),
                                                           conversion=-1)])
        return ast.JoinedStr(values=[ast.Constant(spec.get("head")),
                                     ast.FormattedValue(value=ast.Name(id="p", ctx=ast.Load()),
                                                        conversion=-1)])
    if kind == "fstring_empty":
        return ast.JoinedStr(values=[])
    return ast.Constant(None)


def analysis(roots, local=()):
    return live._Analysis(frozenset(roots or ()), set(local or ()))


def expected(r):
    f = r.get("func")
    if f == "is_test_file":
        return live._is_test_file(Path(r.get("rel") or ""))
    if f == "should_skip":
        rel = Path(r.get("rel") or "")
        return bool(live._SKIP_DIRS.intersection(rel.parts[:-1]) or live._is_test_file(rel))
    if f == "core_module_name":
        return analysis(r.get("roots"))._core_module_name(r.get("name"))
    if f == "core_roots":
        names = set(r.get("packages") or ()) | set(r.get("modules") or ())
        names |= {n.split(".")[0] for n in (r.get("moves") or ())}
        roots = frozenset((names | {"hermes_plugins"}) - {"setup", "conftest"})
        return sorted(analysis(roots, r.get("local")).core_roots)
    if f == "lookup_is_core":
        a = analysis(r.get("roots"))
        a.constants = dict(r.get("constants") or {})
        return a._lookup_is_core(lookup_arg(r.get("spec") or {}))
    if f == "is_container":
        a = live._Analysis.__new__(live._Analysis)
        a.rebound_ctors = frozenset(r.get("rebound") or ())
        return a.is_container(node(r.get("spec") or {}))
    if f == "bound_names":
        return sorted(live._bound_names([node(t) for t in (r.get("targets") or ())]))
    if f == "module_constants":
        return {
            "skip_dirs": sorted(live._SKIP_DIRS),
            "container_ctors": sorted(live._CONTAINER_CTORS),
            "namespace_escapes": sorted(live._NAMESPACE_ESCAPES),
            "mutators": sorted(live._MUTATORS),
        }
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
