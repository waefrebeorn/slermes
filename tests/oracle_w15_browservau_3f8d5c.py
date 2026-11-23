#!/usr/bin/env python3
"""Differential oracle for tools/browser_vault_context.py (gate w15_browservau_3f8d5c)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_browservau_3f8d5c"
sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# agent parent-package bootstrap + the two lazily-imported vault backend modules
_ag = _mod("agent")
_ag.__path__ = []
_vb = _mod("agent.vault_backends")
_vb.__path__ = []


class LoginBackend:
    def fill_context(self):
        return None


_mod("agent.vault_backends.base", LoginBackend=LoginBackend)
_mod("agent.vault_backends.plugin",
     PluginBackendError=type("PluginBackendError", (Exception,), {}))

SRC = QUARRY + "/tools/browser_vault_context.py"
_spec = importlib.util.spec_from_file_location("browser_vault_context_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["browser_vault_context_live"] = live
_spec.loader.exec_module(live)


def _meta(d):
    if d is None:
        return None
    return types.SimpleNamespace(allowed_origins=d.get("allowed_origins", []),
                                 origin=d.get("origin"), kind=d.get("kind"))


def _backend(custom, fresh):
    if custom:
        class B:
            def fill_context(self):
                return None
    else:
        class B:
            fill_context = LoginBackend.fill_context

    def get_meta(self, handle):
        return fresh
    B.get_meta = get_meta
    return B()


def live_out(rec):
    func = rec.get("func")
    if func == "saved_origins":
        return live.saved_origins(_meta(rec.get("meta") or {}))
    if func == "snapshot_refusal":
        custom = bool(rec.get("custom_context", False))
        fresh = _meta(rec.get("fresh"))
        meta = _meta(rec.get("meta") or {})
        return live.snapshot_refusal(_backend(custom, fresh), "handle", meta, rec.get("page_origin", ""))
    return None


def main():
    cases = mism = 0
    allowed = ("saved_origins", "snapshot_refusal")
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in allowed:
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
