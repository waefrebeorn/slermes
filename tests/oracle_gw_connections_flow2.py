#!/usr/bin/env python3
"""Oracle for the gateway_connections FLOW/leaves port.

AST-extracts every live def from hermes_cli/gateway_connections.py into a seeded
namespace (real re/json + module constants + the custom exception classes) and
drives the same fixture inputs the C harness drives. Diffs {"ret","state"}; ret
carries an uncaught exception as {"__raise__":{"type","message"}}.

The two URL-binding leaves (_entry_url/_bound_url) consume an INJECTED result
for the imported normalize_remote_base_url via the fixture "norm" seam; a
{"__raise__":...} norm scripted with a ConnectionStoreError-family type is
caught (returns None), anything else propagates. Full py3.14 builtin exception
lineage plus the module's custom classes is modeled in C (gco2_exc_isa).
0 cases => exit 1. Summary: 'ORACLE gw_connections_flow2: N cases, M mismatches'.
"""
from __future__ import annotations

import ast
import builtins
import copy
import __future__
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "gateway_connections.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "gateway_connections.py"

# func key -> live def name (defining module noted in the C PoP).
PY = {
    "revision": "_revision",
    "plain": "_plain",
    "kept_envelope": "_kept_envelope",
    "envelope": "_envelope",
    "token_preview": "_token_preview",
    "public_connection": "_public_connection",
    "public_registry": "public_registry",
    "incoming_headers": "_incoming_headers",
    "signed_in_ids_gone": "_signed_in_ids_gone",
    "entry_url": "_entry_url",
    "bound_url": "_bound_url",
}
ANN = __future__.annotations.compiler_flag

CUR = {"norm": None}


def make_exc(spec):
    spec = spec or {}
    t = spec.get("type", "Exception")
    msg = spec.get("message", "")
    # module-local classes first, then builtins
    cls = (NS.get(t) if globals().get("NS") else None) or getattr(builtins, t, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        try:
            return cls(msg)
        except TypeError:
            e = cls.__new__(cls); BaseException.__init__(e, msg); return e
    return type(t, (Exception,), {})(msg)


def exc_spec(e):
    spec = {"type": type(e).__name__, "message": str(e)}
    if isinstance(e, SystemExit):
        spec["code"] = e.code
    return spec


def _exec_node(node, ns, src):
    if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
        node = copy.copy(node); node.decorator_list = []
    seg = ast.Module(body=[node], type_ignores=[])
    ast.fix_missing_locations(seg)
    exec(compile(seg, src, "exec", flags=ANN), ns)


def extract():
    mod = types.ModuleType("gc2_oracle")
    sys.modules["gc2_oracle"] = mod
    ns = mod.__dict__
    ns.update({"__name__": "gc2_oracle", "__future__": __future__,
               "json": json})
    src = str(SRC)
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign, ast.ClassDef)):
            try: _exec_node(node, ns, src)
            except Exception: pass
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try: _exec_node(node, ns, src)
            except Exception: pass
    # gateway_connections.py imports its exception classes; seed them here with
    # the exact hierarchy so `except ConnectionStoreError` resolves live.
    ns["ConnectionStoreError"] = type("ConnectionStoreError", (ValueError,), {})
    ns["ConnectionStoreBusy"] = type("ConnectionStoreBusy", (RuntimeError,), {})
    ns["NoSuchConnection"] = type("NoSuchConnection", (KeyError,), {})
    ns["DuplicateConnection"] = type("DuplicateConnection", (ns["ConnectionStoreError"],), {})
    ns["PreferencesBusy"] = type("PreferencesBusy", (Exception,), {})
    missing = [v for v in PY.values() if v not in ns]
    if missing:
        print("FATAL gc2: extraction missing names:", sorted(set(missing)))
        sys.exit(2)
    return ns


NS = extract()


def _seam_norm(x):
    """Injected normalize_remote_base_url: returns the fixture 'norm' value or
    raises its scripted exception."""
    r = CUR["norm"]
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r["__raise__"])
    return r


def _drive_norm(name, argval, norm):
    ns = NS
    CUR["norm"] = norm
    had = "normalize_remote_base_url" in ns
    old = ns.get("normalize_remote_base_url")
    ns["normalize_remote_base_url"] = _seam_norm
    try:
        return ns[name](argval)
    finally:
        if had: ns["normalize_remote_base_url"] = old
        else: ns.pop("normalize_remote_base_url", None)


def proj(v):
    if isinstance(v, dict): return {k: proj(x) for k, x in v.items()}
    if isinstance(v, (tuple, list)): return [proj(x) for x in v]
    if isinstance(v, (set, frozenset)): return sorted(proj(x) for x in v)
    if isinstance(v, BaseException): return exc_spec(v)
    return v


def drive(func, inp):
    ns = NS
    v = inp.get
    if func == "revision": return ns["_revision"](v("doc"))
    if func == "plain": return ns["_plain"](v("secret"))
    if func == "kept_envelope": return ns["_kept_envelope"](v("token"), v("existing"))
    if func == "envelope": return ns["_envelope"](v("value"))
    if func == "token_preview": return ns["_token_preview"](v("value"))
    if func == "public_connection": return ns["_public_connection"](v("entry") or {})
    if func == "public_registry": return ns["public_registry"](v("reg") or {})
    if func == "incoming_headers": return ns["_incoming_headers"](v("raw") or {}, v("existing"))
    if func == "signed_in_ids_gone": return ns["_signed_in_ids_gone"](v("before") or {}, v("after") or {})
    if func == "entry_url": return _drive_norm("_entry_url", v("entry") or {}, v("norm"))
    if func == "bound_url": return _drive_norm("_bound_url", v("token_set") or {}, v("norm"))
    raise KeyError(func)


def run_case(func, inp):
    state = copy.deepcopy(inp.get("state") or {})
    if not isinstance(state, dict): state = {}
    out = {"ret": None, "state": state}
    ret = None
    try:
        ret = drive(func, inp)
    except BaseException as e:
        ret = {"__raise__": exc_spec(e)}
    out["ret"] = proj(ret)
    out["state"] = {k: proj(val) for k, val in state.items()}
    return out


def main():
    cases = 0; mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"): continue
        try: c = json.loads(line)
        except json.JSONDecodeError: continue
        cases += 1
        func = c.get("func", "")
        if func not in PY:
            mism += 1; print(f"PYERR {func}: unknown func"); continue
        try: want = run_case(func, c.get("in") or {})
        except Exception as e:
            mism += 1; print(f"PYERR {func}: {type(e).__name__}: {e}"); continue
        got = c.get("out") or {}
        ok = (got.get("ret") == want.get("ret") and got.get("state") == want.get("state"))
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE gw_connections_flow2: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
