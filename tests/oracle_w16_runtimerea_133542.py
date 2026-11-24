#!/usr/bin/env python3
"""Differential oracle for hermes_cli/runtime_readiness.py (w16_runtimerea_133542).

The live module keeps all heavy dependencies behind function-local imports;
this oracle registers small deterministic stand-ins for those boundaries
(provider profiles, the usable-secret predicate, and the custom runtime error)
before replaying the live functions.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)

# --- stubs for the function-local dependency imports ---
_providers = types.ModuleType("providers")
_cfg = {"present": False, "hint": "", "kind": 0}


class _Profile:
    def missing_credentials_hint(self):
        if _cfg["kind"] == 1:
            raise RuntimeError("plugin hook down")
        if _cfg["kind"] == 2:
            return 12345  # non-string answer
        return _cfg["hint"]


_providers.get_provider_profile = lambda name: _Profile() if _cfg["present"] and name else None
sys.modules["providers"] = _providers

_cli_pkg = types.ModuleType("hermes_cli")
_cli_pkg.__path__ = []
sys.modules["hermes_cli"] = _cli_pkg

_auth = types.ModuleType("hermes_cli.auth")
_usable = set()
_auth.has_usable_secret = lambda text: text in _usable
sys.modules["hermes_cli.auth"] = _auth

_rpc = types.ModuleType("hermes_cli.runtime_provider_custom")


class LocalRuntimeUnavailableError(Exception):
    def __init__(self, message, error_code=None):
        super().__init__(message)
        self.error_code = error_code


_rpc.LocalRuntimeUnavailableError = LocalRuntimeUnavailableError
sys.modules["hermes_cli.runtime_provider_custom"] = _rpc

SRC = PYROOT + "/hermes_cli/runtime_readiness.py"
spec = importlib.util.spec_from_file_location("w16_runtimerea_133542_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w16_runtimerea_133542_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "suffix":
        _cfg["present"] = bool(r.get("profile_present", False))
        _cfg["hint"] = r.get("hint", "") or ""
        _cfg["kind"] = int(r.get("hint_kind", 0) or 0)
        return live.missing_credentials_suffix(r.get("provider"))
    if func == "creds":
        _usable.clear()
        _usable.update(r.get("usable_secrets", []) or [])
        runtime = {}
        if r.get("api_key_callable"):
            runtime["api_key"] = lambda: "unused"
        else:
            runtime["api_key"] = r.get("api_key")
        if r.get("command"):
            runtime["command"] = r.get("command")
        return bool(live.runtime_has_credentials(runtime))
    if func == "failure":
        text = r.get("error_text", "") or ""
        if r.get("has_error_code"):
            exc = LocalRuntimeUnavailableError(text, r.get("error_code", ""))
        else:
            exc = Exception(text)
        return live.readiness_failure(exc)
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w16_runtimerea_133542: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
