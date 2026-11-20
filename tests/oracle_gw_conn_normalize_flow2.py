#!/usr/bin/env python3
"""Oracle for the gateway_connections_normalize FLOW/leaves port.

AST-extracts every live def from hermes_cli/gateway_connections_normalize.py
into a seeded namespace (real re/json/urlsplit + module constants + the four
exception classes). Drives the same fixture inputs the C harness drives and
diffs {"ret", "state"}: ret carries an uncaught exception as
{"__raise__": {"type","message"}}; state is the caller-held mutable carrier
(called out for the _Normalizer methods / *clean helpers).

No collaborator seams: this module is pure. Full py3.14 builtin exception
lineage plus the module's custom classes is modeled on the C side (gcn2_exc_isa)
and exercised by the _Normalizer.add except-family and the scripted-raise-free
propagation paths. 0 cases => exit 1.
Summary: 'ORACLE gw_conn_normalize_flow2: N cases, M mismatches'.
"""
from __future__ import annotations

import ast
import builtins
import copy
import __future__
import json
import os
import sys
import types
from pathlib import Path
from urllib.parse import urlsplit

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "gateway_connections_normalize.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "gateway_connections_normalize.py"

# func key -> live def name (defining module noted in the C PoP).
PY = {
    "js_string": "js_string",
    "js_falsy": "js_falsy",
    "js_str": "js_str",
    "label_key": "label_key",
    "unique_label": "unique_label",
    "label_slug": "label_slug",
    "connection_id_for_label": "connection_id_for_label",
    "norm_auth_mode": "norm_auth_mode",
    "stored_auth_mode": "stored_auth_mode",
    "_url_host": "_url_host",
    "normalize_remote_base_url": "normalize_remote_base_url",
    "host_label_from_base_url": "host_label_from_base_url",
    "sanitize_header_value": "sanitize_header_value",
    "normalize_remote_headers": "normalize_remote_headers",
    "_split_ssh_host": "_split_ssh_host",
    "_explicit_port": "_explicit_port",
    "normalize_ssh_config": "normalize_ssh_config",
    "_ssh_fields": "_ssh_fields",
    "_local_entry": "_local_entry",
    "_remote_like_clean": "_remote_like_clean",
    "_ssh_clean": "_ssh_clean",
    "quarantine": "_Normalizer.quarantine",
    "add": "_Normalizer.add",
    "_add_entry": "_Normalizer._add_entry",
    "normalize_registry": "normalize_registry",
    "_url_key": "_url_key",
    "_ssh_key": "_ssh_key",
    "_check_label": "_check_label",
    "_ssh_input": "_ssh_input",
    "_remote_input": "_remote_input",
    "normalize_connection_input": "normalize_connection_input",
    "merge_connection_input": "merge_connection_input",
    "_same_url": "_same_url",
    "connection_dial_fields_changed": "connection_dial_fields_changed",
    "upsert_connection": "upsert_connection",
    "remove_connection": "remove_connection",
    "_require": "_require",
    "set_primary_connection": "set_primary_connection",
    "set_last_used_connection": "set_last_used_connection",
    "set_connection_launch_mode": "set_connection_launch_mode",
}
ANN = __future__.annotations.compiler_flag

STATE_FUNCS = {"quarantine", "add", "_add_entry", "_remote_like_clean", "_ssh_clean"}


def make_exc(spec):
    spec = spec or {}
    t = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if t == "SystemExit":
        return SystemExit(spec.get("code", 1))
    cls = getattr(builtins, t, None)
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
    import re
    mod = types.ModuleType("gcn2_oracle")
    sys.modules["gcn2_oracle"] = mod
    ns = mod.__dict__
    ns.update({"__name__": "gcn2_oracle", "__future__": __future__,
               "re": re, "json": json, "urlsplit": urlsplit})
    src = str(SRC)
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    # Assign/AnnAssign (constants, compiled regexes, sets) + ClassDef (4
    # exception classes + _Normalizer) first, then the functions.
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign, ast.ClassDef)):
            try: _exec_node(node, ns, src)
            except Exception: pass
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try: _exec_node(node, ns, src)
            except Exception: pass
    # _Normalizer.quarantine's cap constant and js helpers live in ns.
    missing = []
    for v in PY.values():
        base = v.split(".")[0]
        if base not in ns:
            missing.append(base)
    if missing:
        print("FATAL gcn2: extraction missing names:", sorted(set(missing)))
        sys.exit(2)
    return ns


NS = extract()


def proj(v):
    if isinstance(v, dict): return {k: proj(x) for k, x in v.items()}
    if isinstance(v, (tuple, list)): return [proj(x) for x in v]
    if isinstance(v, (set, frozenset)): return sorted(proj(x) for x in v)
    if isinstance(v, BaseException): return exc_spec(v)
    return v


def _norm_from_state(ctx):
    n = NS["_Normalizer"]()
    n.labels = set(ctx.state.get("labels", []))
    n.ids = set(ctx.state.get("ids", []))
    n.connections = list(ctx.state.get("connections", []))
    n.quarantined = list(ctx.state.get("quarantined", []))
    return n


def _state_from_norm(ctx, n):
    ctx.state["labels"] = sorted(n.labels)
    ctx.state["ids"] = sorted(n.ids)
    ctx.state["connections"] = n.connections
    ctx.state["quarantined"] = n.quarantined


def drive(func, inp, ctx):
    ns = NS
    v = inp.get
    if func in ("quarantine", "add", "_add_entry"):
        for k in ("labels", "ids", "connections", "quarantined"):
            ctx.state.setdefault(k, [])
        n = _norm_from_state(ctx)
        if func == "quarantine":
            n.quarantine(v("reason", ""), v("entry"))
        elif func == "add":
            n.add(v("item"))
        else:
            n._add_entry(v("entry") or {})
        _state_from_norm(ctx, n)
        return None
    if func == "_remote_like_clean":
        clean = ctx.state.setdefault("clean", {})
        return ns["_remote_like_clean"](clean, v("entry") or {}, v("kind", ""))
    if func == "_ssh_clean":
        clean = ctx.state.setdefault("clean", {})
        return ns["_ssh_clean"](clean, v("entry") or {})

    name = PY[func]
    fn = ns[name]
    if func == "js_string": return fn(v("value"))
    if func == "js_falsy": return fn(v("value"))
    if func == "js_str": return fn(v("value"))
    if func == "label_key": return fn(v("value"))
    if func == "unique_label": return fn(v("candidate"), v("taken", []) or [])
    if func == "label_slug": return fn(v("label"))
    if func == "connection_id_for_label": return fn(v("label"), v("taken", []) or [])
    if func == "norm_auth_mode": return fn(v("mode"))
    if func == "stored_auth_mode": return fn(v("kind", ""), v("auth_mode"), v("token"))
    if func == "_url_host": return fn(urlsplit(v("url", "") or ""))
    if func == "normalize_remote_base_url": return fn(v("raw"))
    if func == "host_label_from_base_url": return fn(v("base_url"))
    if func == "sanitize_header_value": return fn(v("value"))
    if func == "normalize_remote_headers": return fn(v("raw"))
    if func == "_split_ssh_host":
        h, u, p = fn(v("host", "") or "")
        return {"host": h, "user": u, "port": p}
    if func == "_explicit_port": return fn(v("raw"))
    if func == "normalize_ssh_config": return fn(v("entry"))
    if func == "_ssh_fields": return fn(v("ssh") or {})
    if func == "_local_entry":
        return fn() if "label" not in inp else fn(v("label"))
    if func == "normalize_registry": return fn(v("raw"))
    if func == "_url_key": return fn(v("value", "") or "")
    if func == "_ssh_key": return fn(v("conn") or {})
    if func == "_check_label": return fn(v("label", ""), v("input_id"), v("registry") or {})
    if func == "_ssh_input": return fn(v("data") or {}, v("cid", ""), v("registry") or {})
    if func == "_remote_input": return fn(v("data") or {}, v("cid", ""), v("kind", ""), v("registry") or {})
    if func == "normalize_connection_input": return fn(v("data") or {}, v("registry") or {})
    if func == "merge_connection_input": return fn(v("data") or {}, v("existing"))
    if func == "_same_url": return fn(v("a"), v("b"))
    if func == "connection_dial_fields_changed": return fn(v("before") or {}, v("after") or {})
    if func == "upsert_connection": return fn(v("registry") or {}, v("entry") or {})
    if func == "remove_connection": return fn(v("registry") or {}, v("cid", ""))
    if func == "_require": return fn(v("registry") or {}, v("cid", ""))
    if func == "set_primary_connection": return fn(v("registry") or {}, v("cid", ""))
    if func == "set_last_used_connection": return fn(v("registry") or {}, v("cid", ""))
    if func == "set_connection_launch_mode": return fn(v("registry") or {}, v("mode", ""))
    raise KeyError(func)


def run_case(func, inp):
    state = copy.deepcopy(inp.get("state") or {})
    if isinstance(state, dict):
        pass
    else:
        state = {}
    out = {"ret": None, "state": {}}
    ret = None
    try:
        class Ctx: pass
        ctx = Ctx(); ctx.state = state
        ret = drive(func, inp, ctx)
    except BaseException as e:
        ret = {"__raise__": exc_spec(e)}
    out["ret"] = proj(ret)
    st = {}
    for k, val in state.items():
        if k in ("labels", "ids") and isinstance(val, (list, tuple, set)):
            st[k] = sorted(proj(val))
        else:
            st[k] = proj(val)
    out["state"] = st
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
    print(f"ORACLE gw_conn_normalize_flow2: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
