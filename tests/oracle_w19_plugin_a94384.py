#!/usr/bin/env python3
"""Differential oracle for agent/vault_backends/plugin.py (gate w19_plugin_a94384)."""
import ast
import contextlib
import json
import sys
import types
import __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "agent" / "vault_backends" / "plugin.py"
_ANN = __future__.annotations.compiler_flag
GATE = "w19_plugin_a94384"
VAULT_KINDS = ("login", "payment", "address")


class VaultItemMeta:
    """The core-owned metadata object the live module copies plugin results into."""

    def __init__(self, *, id, kind, label, origin, created_at, identifier_type=None,
                 identifier=None, has_otp=False, allowed_origins=()):
        self.id, self.kind, self.label, self.origin = id, kind, label, origin
        self.created_at, self.identifier_type, self.identifier = created_at, identifier_type, identifier
        self.has_otp, self.allowed_origins = has_otp, allowed_origins


class LoginBackend:
    """Stub base for the abstract adapter."""

    name = display_name = prefix = ""
    needs_unlock = False
    own_declared = None

    def fill_context(self):
        return contextlib.nullcontext()


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    mod = types.ModuleType("plugin_live")
    sys.modules["plugin_live"] = mod
    g = mod.__dict__
    g.update(dict(contextmanager=contextlib.contextmanager, LoginBackend=LoginBackend,
                  VAULT_KINDS=VAULT_KINDS, VaultItemMeta=VaultItemMeta, Any=object,
                  Callable=object, Optional=object, TYPE_CHECKING=False))
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.ClassDef)) and node.name in (
                "_text", "_materialize", "PluginBackendError", "PluginLoginBackend"):
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
            if names and names[0] in ("_FAILED", "_INVALID", "_TEXT_FIELDS", "_OPTIONAL_TEXT_FIELDS"):
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                             flags=_ANN, dont_inherit=True), g)
    return g


G = extract()
INVALID_MARK = {"invalid": True}


def _enc(v):
    return INVALID_MARK if v is G["_INVALID"] else v


def build_meta(d):
    if not isinstance(d, dict):
        return d
    origins = d.get("allowed_origins", [])
    origins = list(origins) if d.get("_origins_list") else tuple(origins)
    return VaultItemMeta(id=d.get("id"), kind=d.get("kind"), label=d.get("label"),
                         origin=d.get("origin"), created_at=d.get("created_at"),
                         identifier_type=d.get("identifier_type"),
                         identifier=d.get("identifier"), has_otp=d.get("has_otp", False),
                         allowed_origins=origins)


def meta_out(m):
    return {"id": m.id, "kind": m.kind, "label": m.label, "origin": m.origin,
            "created_at": m.created_at, "identifier_type": m.identifier_type,
            "identifier": m.identifier, "has_otp": m.has_otp,
            "allowed_origins": list(m.allowed_origins)}


class Ctx:
    def __init__(self, spec):
        self.spec = spec

    def __enter__(self):
        if self.spec.get("enter_raise"):
            raise RuntimeError("enter boom")
        return self

    def __exit__(self, *exc):
        if self.spec.get("exit_raise"):
            raise RuntimeError("exit boom")
        return False


class PluginStub:
    def __init__(self, spec):
        self.spec = spec or {}

    def _s(self, verb):
        v = self.spec.get(verb)
        return v if isinstance(v, dict) else {}

    def _guard(self, verb):
        s = self._s(verb)
        if s.get("raise"):
            raise RuntimeError("plugin boom")
        return s

    def list_items(self):
        s = self._guard("list_items")
        return [build_meta(m) for m in (s.get("items") or [])]

    def get_meta(self, handle):
        s = self._guard("get_meta")
        v = s.get("value")
        return None if v is None else build_meta(v)

    def resolve_password(self, handle):
        return self._guard("resolve_password").get("value")

    def resolve_otp(self, handle):
        return self._guard("resolve_otp").get("value")

    def resolve_secret(self, handle):
        return self._guard("resolve_secret").get("value")

    def fill_context(self):
        s = self._s("fill_context")
        if s.get("raise"):
            raise RuntimeError("plugin boom")
        return Ctx(s)


def run_case(r):
    f = r.get("func")
    if f == "text":
        return _enc(G["_text"](r.get("value")))
    if f == "materialize":
        v = G["_materialize"](build_meta(r.get("meta")))
        return meta_out(v) if isinstance(v, VaultItemMeta) else _enc(v)
    b = G["PluginLoginBackend"](r.get("name") or "acme", PluginStub(r.get("plugin")))
    handle = r.get("handle")
    try:
        if f == "own":
            return b.owns(handle)
        if f == "list_items":
            return {"status": "ok", "value": [meta_out(m) for m in b.list_items()]}
        if f == "get_meta":
            m = b.get_meta(handle)
            return {"status": "ok", "value": None if m is None else meta_out(m)}
        if f == "resolve_password":
            return {"status": "ok", "value": b.resolve_password(handle)}
        if f == "resolve_otp":
            return {"status": "ok", "value": b.resolve_otp(handle)}
        if f == "resolve_secret":
            return {"status": "ok", "value": b.resolve_secret(handle)}
        if f == "fill_context":
            with b.fill_context():
                pass
            return {"status": "ok", "value": True}
    except Exception as exc:
        return {"status": "error", "message": str(exc)}
    return None


FUNCS = {"text", "materialize", "own", "list_items", "get_meta", "resolve_password",
         "resolve_otp", "resolve_secret", "fill_context"}


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
        if f not in FUNCS:
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
