#!/usr/bin/env python3
"""Differential oracle for gate w16_vaultbacke_84fa80 (agent/vault_backend_registry.py).

Loads the LIVE module via spec_from_file_location with stubbed provider_registry /
vault_backends.base dependencies. Builtin namespaces are injected per case, and
static_name is driven by real dynamically-built provider classes so inspect and
issubclass branches exercise the LIVE code.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if not (Path(QUARRY) / "agent" / "vault_backend_registry.py").is_file():
    QUARRY = str(Path(__file__).resolve().parent.parent.parent / "hermes-agent")
sys.path.insert(0, QUARRY)
for _pkg in ("agent", "hermes_cli"):
    if _pkg not in sys.modules:
        _m = types.ModuleType(_pkg)
        _m.__path__ = [str(Path(QUARRY) / _pkg)]
        sys.modules[_pkg] = _m

_pr = types.ModuleType("agent.provider_registry")
class _PR:
    def __init__(self, **kw): pass
    def export(self, ns): pass
    def merged(self, scope=None): return {}
    def register(self, provider, scope=None): pass
_pr.ProviderRegistry = _PR
sys.modules["agent.provider_registry"] = _pr

_vb = types.ModuleType("agent.vault_backends"); _vb.__path__ = []
_base = types.ModuleType("agent.vault_backends.base")
class _LB: pass
_base.LoginBackend = _LB
_base.external_backend_classes = lambda: ()
sys.modules["agent.vault_backends"] = _vb
sys.modules["agent.vault_backends.base"] = _base

_SPEC = importlib.util.spec_from_file_location(
    "w16_vault_backend_registry_live", str(Path(QUARRY) / "agent" / "vault_backend_registry.py"))
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, list): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def _patch_names(p):
    mod._builtin_namespaces = lambda: (set(p.get("builtin_names") or ()),
                                       tuple(p.get("builtin_prefixes") or ()))


def _make_provider(p):
    base = mod.LoginBackend if p.get("is_subclass") else object
    attrs = {}
    if p.get("overrides_getattribute"):
        attrs["__getattribute__"] = lambda self, k: object.__getattribute__(self, k)
    if "name" in p: attrs["name"] = p["name"]
    if "prefix" in p: attrs["prefix"] = p["prefix"]
    if p.get("needs_unlock") is True: attrs["needs_unlock"] = True
    return type("PluginProbe", (base,), attrs)()


def expected(rec):
    f = rec.get("func")
    if f == "namespace_error":
        p = {"builtin_names": rec.get("builtin_names"), "builtin_prefixes": rec.get("builtin_prefixes")}
        _patch_names(p)
        return mod._namespace_error(rec.get("name"))
    if f == "static_name":
        p = rec.get("provider") or {}
        _patch_names(p)
        try:
            return {"ok": True, "name": mod.static_name(_make_provider(p)), "error": None}
        except ValueError as e:
            return {"ok": False, "name": None, "error": str(e)}
    if f == "enabled_plugin_backends":
        p = {"builtin_names": rec.get("builtin_names"), "builtin_prefixes": rec.get("builtin_prefixes")}
        _patch_names(p)
        reg = {r["name"]: object() for r in (rec.get("registry") or [])}
        class _FakeReg:
            def merged(self, scope=None): return reg
        mod._registry = _FakeReg()
        plugin = types.ModuleType("agent.vault_backends.plugin")
        class _Plugin:
            def __init__(self, name, provider): self.name = name
        plugin.PluginLoginBackend = _Plugin
        sys.modules["agent.vault_backends.plugin"] = plugin
        return [b.name for b in mod.enabled_plugin_backends(rec.get("cfg") or {})]
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        rec = json.loads(line)
        n += 1
        want = expected(rec)
        got = rec.get("out")
        if _canon(got) != _canon(want):
            mism += 1
            if mism <= 10:
                print("MISMATCH %s\n  got : %s\n  want: %s" %
                      (rec.get("func"), json.dumps(got, ensure_ascii=False),
                       json.dumps(want, ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w16_vaultbacke_84fa80: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
