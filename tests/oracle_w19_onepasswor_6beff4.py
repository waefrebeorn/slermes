#!/usr/bin/env python3
"""Differential oracle for agent/vault_backends/onepassword.py (gate w19_onepasswor_6beff4).

The live module's imports pull the whole agent stack, so the oracle execs the two
module-level origin helpers, the backend ClassDef, and the normalize_origin they
depend on (agent/vault_store.py) against stub globals, then drives the real
_item_selector / _env methods with injected filesystem-free seams.
"""
import ast, json, sys, types, __future__
from pathlib import Path
from urllib.parse import urlsplit

for _stream in (sys.stdin, sys.stdout):
    try:
        _stream.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
GATE = "w19_onepasswor_6beff4"
Q = "C:/Users/eman5/hermes-agent"
SRC = Path(Q) / "agent/vault_backends/onepassword.py"
STORE = Path(Q) / "agent/vault_store.py"
SECRETS = Path(Q) / "agent/secret_sources/onepassword.py"
_ANN = __future__.annotations.compiler_flag


def _exec(nodes, g, path):
    for node in nodes:
        exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                     flags=_ANN, dont_inherit=True), g)


def extract():
    g = {"__name__": "op_live", "urlsplit": urlsplit}
    store = ast.parse(STORE.read_text(encoding="utf-8"))
    for node in store.body:
        if isinstance(node, ast.ClassDef) and node.name == "VaultError":
            _exec([node], g, STORE)
        elif isinstance(node, ast.Assign) and any(getattr(t, "id", "") == "_DEFAULT_PORTS"
                                                  for t in node.targets):
            _exec([node], g, STORE)
        elif isinstance(node, ast.FunctionDef) and node.name == "normalize_origin":
            _exec([node], g, STORE)

    sec = ast.parse(SECRETS.read_text(encoding="utf-8"))
    for node in sec.body:
        if isinstance(node, ast.Assign) and any(getattr(t, "id", "") == "_OP_ENV_ALLOWLIST"
                                                for t in node.targets):
            _exec([node], g, SECRETS)

    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in ("_all_origins", "_web_origins"):
            _exec([node], g, SRC)
        elif isinstance(node, ast.ClassDef) and node.name == "OnePasswordLoginBackend":
            g.setdefault("LoginBackend", type("LoginBackend", (object,), {}))
            _exec([node], g, SRC)
    return g


G = extract()
BACKEND = G["OnePasswordLoginBackend"]


def _refresh(obj, listed):
    def _list():
        obj._item_vaults = dict(listed)
        return []
    return _list


def run_case(r):
    f = r.get("func")
    if f == "all_origins":
        return G["_all_origins"](r.get("urls") or [])
    if f == "web_origins":
        return list(G["_web_origins"](r.get("origins") or []))
    if f == "item_selector":
        obj = BACKEND.__new__(BACKEND)
        obj.cfg = {}
        obj._service_token = ""
        obj._item_vaults = dict(r.get("item_vaults") or {})
        obj.list_items = _refresh(obj, r.get("listed_vaults") or {})
        try:
            return {"args": BACKEND._item_selector(obj, r.get("handle", ""))}
        except RuntimeError as exc:
            return {"error": str(exc)}
    if f == "env":
        obj = BACKEND.__new__(BACKEND)
        obj.cfg = r.get("cfg") or {}
        obj._service_token = r.get("service_token") or ""
        G["os"] = types.SimpleNamespace(environ=dict(r.get("environ") or {}))
        secrets = r.get("secrets") or {}
        m = types.ModuleType("agent.secret_scope")
        m.get_secret = lambda k, d="": secrets.get(k, d) or ""
        sys.modules["agent.secret_scope"] = m
        token = r.get("session_token")
        return BACKEND._env(obj, token or None)
    return None


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
        f = r.get("func")
        if f not in {"all_origins", "web_origins", "item_selector", "env"}:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
