#!/usr/bin/env python3
"""Differential oracle for agent/vault_keychain.py (gate w7_vaultkeycha_33b68a).

keyring/pathlib/uuid are injected collaborators: the live module is loaded
with importlib.util.spec_from_file_location, then its module-level Path/uuid are
replaced with deterministic seams and sys.modules['keyring'] is set per case.
The four deterministic helpers themselves run from the live file.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

R = {}


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


agent_pkg = _mod("agent")
agent_pkg.__path__ = []
_mod("agent.vault_store", VaultError=type("VaultError", (Exception,), {}))

SRC = QUARRY + "/agent/vault_keychain.py"
spec = importlib.util.spec_from_file_location("w7_vaultkeycha_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


class FakePath:
    def __init__(self, value):
        self._value = str(value)

    def resolve(self):
        return self

    def __str__(self):
        return self._value


live.Path = FakePath


def make_backend(desc):
    if not isinstance(desc, dict):
        desc = {}
    attrs = {"__module__": desc.get("module", "keyring.backends.macOS")}
    if "backends" in desc and isinstance(desc["backends"], list):
        attrs["backends"] = [make_backend(c) for c in desc["backends"]]
    if "name" in desc:
        attrs["name"] = desc["name"]
    return type(desc.get("type_name", "Backend"), (), attrs)()


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func not in ("is_os_keychain", "backend_label", "new_ref", "owns"):
        continue
    cases += 1
    R = r
    sys.modules.pop("keyring", None)

    # deterministic uuid.uuid4().hex seam
    live.uuid = types.SimpleNamespace(
        uuid4=lambda: types.SimpleNamespace(hex=r.get("account_hex", "")))

    try:
        if func == "is_os_keychain":
            backend = make_backend(r.get("backend"))
            py = bool(live._is_os_keychain(backend))
        elif func == "backend_label":
            if r.get("import_error"):
                sys.modules["keyring"] = None  # `import keyring` raises ImportError
            elif r.get("backend_error"):
                def bad_get_keyring():
                    raise RuntimeError("x")
                _mod("keyring", get_keyring=bad_get_keyring)
            else:
                b = make_backend(r.get("backend"))
                _mod("keyring", get_keyring=lambda: b)
            py = live.backend_label()
        elif func == "new_ref":
            py = live.new_ref(r.get("vault_dir", ""))
        else:  # owns
            sys.modules.pop("keyring", None)
            py = bool(live.owns(r.get("ref") or {}, r.get("vault_dir", "")))
    except Exception as exc:
        mism += 1
        print("LIVE ERROR case=%s %r" % (r.get("case"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH case=%s\n  py=%r\n  c =%r" % (r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_vaultkeycha_33b68a: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
