#!/usr/bin/env python3
"""Differential oracle for gate w14_auxiliaryu_c855f2 (agent/auxiliary_unavailable.py).

Loads the LIVE module via importlib.spec_from_file_location with a stubbed
hermes_time, then drives _sentence and missing_provider_credentials_message
with injected PROVIDER_REGISTRY / LLAMACPP_ALIASES / pool_cooldown_message.
"""
import importlib.util
import json
import os
import sys
import types
from pathlib import Path
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "auxiliary_unavailable.py").is_file():
    QUARRY = Path("C:/Users/eman5/hermes-agent")
sys.path.insert(0, str(QUARRY))


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


_pkg("agent")
_pkg("hermes_cli")
_pkg("hermes_cli.local_runtime")
sys.modules["hermes_time"] = SimpleNamespace(safe_strftime=lambda *a, **k: "")
_auth = types.ModuleType("hermes_cli.auth")
_auth.PROVIDER_REGISTRY = {}
sys.modules["hermes_cli.auth"] = _auth
sys.modules["hermes_cli"].auth = _auth
_ep = types.ModuleType("hermes_cli.local_runtime.endpoint")
_ep.LLAMACPP_ALIASES = set()
sys.modules["hermes_cli.local_runtime.endpoint"] = _ep
sys.modules["hermes_cli.local_runtime"].endpoint = _ep

_SRC = str(QUARRY / "agent" / "auxiliary_unavailable.py")
_SPEC = importlib.util.spec_from_file_location("w14_auxiliaryu_live", _SRC)
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


def expected(rec):
    f = rec.get("func")
    if f == "sentence":
        return mod._sentence(rec.get("text"))
    if f == "missing_provider_credentials_message":
        pid = rec.get("provider_id") or ""
        cooldown = rec.get("cooldown")
        mod.pool_cooldown_message = lambda _pid: cooldown if cooldown else None
        pcspec = rec.get("pconfig")
        if pcspec is None:
            _auth.PROVIDER_REGISTRY = {}
        else:
            pc = SimpleNamespace(
                auth_type=pcspec.get("auth_type"),
                api_key_env_vars=tuple(pcspec.get("api_key_env_vars") or ()),
            )
            _auth.PROVIDER_REGISTRY = {pid: pc}
        _ep.LLAMACPP_ALIASES = {pid} if rec.get("llama_alias") else set()
        return mod.missing_provider_credentials_message(pid)
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        want = expected(rec)
        got = rec.get("out")
        if got != want:
            mism += 1
            if mism <= 10:
                print("MISMATCH %s\n  got : %s\n  want: %s" %
                      (rec.get("func"), json.dumps(got, ensure_ascii=False),
                       json.dumps(want, ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w14_auxiliaryu_c855f2: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
