#!/usr/bin/env python3
"""Differential oracle for w13_methodsvau_26674a (tui_gateway/methods_vault.py)."""
import importlib.util
import json
import re
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/tui_gateway/methods_vault.py"
sys.path.insert(0, QUARRY)
if "tui_gateway" not in sys.modules:
    _pkg = types.ModuleType("tui_gateway")
    _pkg.__path__ = [QUARRY + "/tui_gateway"]
    sys.modules["tui_gateway"] = _pkg

spec = importlib.util.spec_from_file_location("tui_gateway.methods_vault", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["tui_gateway.methods_vault"] = live
spec.loader.exec_module(live)
BY = dict(live._registry._pending)   # raw handler functions by method name
live._ok = lambda rid, result: result
live._err = lambda rid, code, message, data=None: {"code": code, "error": message}


def scrub(text, secret):
    s = text
    for value in secret.values():
        if isinstance(value, str) and len(value) >= 3 and value in s:
            s = s.replace(value, "[REDACTED]")
    return re.sub(r"(password['\"]?\s*[:=]\s*)\S+", r"\1[REDACTED]", s)


class VaultError(Exception):
    pass


def mod(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


def compute(r):
    f = r.get("func")
    params = dict(r.get("params") or {})
    keys = []
    if f == "remove":
        vs = mod("agent.vault_store")
        vs.get_vault_store = lambda: types.SimpleNamespace(
            remove_item=lambda item_id: r.get("removed"))
        keys = [("agent", mod("agent")), ("agent.vault_store", vs)]
        fn = BY["vault.remove"]
    elif f == "lock":
        ul = mod("agent.vault_backends.unlock")
        ul.lock = lambda *a, **k: None
        vb = mod("agent.vault_backends")
        keys = [("agent", mod("agent")), ("agent.vault_backends", vb),
                ("agent.vault_backends.unlock", ul)]
        fn = BY["vault.lock"]
    elif f == "add":
        ar = r.get("add_result") or {}
        def add_item(kind, label, origin, secret):
            if "error" in ar:
                raise VaultError(ar["error"])
            return types.SimpleNamespace(id=ar.get("id"))
        vs = mod("agent.vault_store")
        vs.VaultError = VaultError
        vs.get_vault_store = lambda: types.SimpleNamespace(add_item=add_item)
        vs.scrub_secret_from_text = scrub
        keys = [("agent", mod("agent")), ("agent.vault_store", vs)]
        fn = BY["vault.add"]
    elif f == "unlock":
        exc = r.get("exc_msg")
        backends = []
        for b in (r.get("backends") or []):
            def mk(e=exc):
                def unlock(pw):
                    if isinstance(e, str):
                        raise Exception(e)
                return unlock
            backends.append(types.SimpleNamespace(name=b.get("name"),
                                                  needs_unlock=bool(b.get("needs_unlock")),
                                                  unlock=mk()))
        vb = mod("agent.vault_backends")
        vb.enabled_backends = lambda: backends
        keys = [("agent", mod("agent")), ("agent.vault_backends", vb)]
        fn = BY["vault.unlock"]
    else:
        return None
    saved = {k: sys.modules.get(k) for k, _ in keys}
    try:
        for k, m in keys:
            sys.modules[k] = m
        return fn("rid1", params)
    finally:
        for k, v in saved.items():
            if v is None:
                sys.modules.pop(k, None)
            else:
                sys.modules[k] = v


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w13_methodsvau_26674a: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w13_methodsvau_26674a {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_methodsvau_26674a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
