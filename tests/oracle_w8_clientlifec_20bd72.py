#!/usr/bin/env python3
"""Differential oracle for agent/client_lifecycle.py (w8_clientlifec_20bd72 gate).

Drives LIVE defs with injected collaborator state; compares to C harness out.
"""
import importlib.util
import json
import sys
import types
from unittest.mock import Mock

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


agent = _mod("agent")
agent.__path__ = []
_mod("agent.lazy_forward",
     forward=lambda *a, **k: (lambda self, *aa, **kk: None),
     forward_static=lambda *a, **k: (lambda *aa, **kk: None),
     lazy_attr=lambda *a, **k: (lambda: None))
hc = _mod("hermes_cli")
hc.__path__ = []
_mod("hermes_cli.timeouts", get_provider_request_timeout=lambda p, m: None)
_mod("utils", base_url_host_matches=lambda u, h: False,
     env_float=lambda n, d: d)

SRC = Q + "/agent/client_lifecycle.py"
spec = importlib.util.spec_from_file_location("w8_clientlifec_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w8_clientlifec_live"] = live
spec.loader.exec_module(live)
Mixin = live.ClientLifecycleMixin


def build_client(spec_in):
    if spec_in.get("mock"):
        return Mock()
    o = types.SimpleNamespace()
    if "is_closed" in spec_in:
        v = spec_in["is_closed"]
        if isinstance(v, dict) and "$call" in v:
            o.is_closed = lambda: v["$call"]
        else:
            o.is_closed = v
    if "_client" in spec_in:
        o._client = types.SimpleNamespace(is_closed=spec_in["_client"].get("is_closed", False))
    return o


def drive(r):
    func = r.get("func")
    if func == "valid_pair":
        return live._valid_credential_pair(r.get("api_key"), r.get("base_url"))
    if func == "reset_slot":
        d = dict(r.get("cache") or {})
        live._reset_slot(d, in_use=bool(r.get("in_use", False)))
        return d
    if func == "image_parts":
        return Mixin._api_kwargs_have_image_parts(r.get("api_kwargs"))
    if func == "client_closed":
        return Mixin._is_openai_client_closed(build_client(r.get("client") or {}))
    if func == "should_adopt":
        a = r.get("args") or {}
        st = a.get("state") or {}
        seen = st.get("_env_creds_seen")
        ns = types.SimpleNamespace(
            base_url=st.get("base_url"), api_key=st.get("api_key"),
            _env_creds_seen=tuple(seen) if isinstance(seen, list) else None,
            _credential_pool=st.get("_credential_pool"),
            _credential_pool_entry_id=st.get("_credential_pool_entry_id"))
        return Mixin._should_adopt_env_credentials(
            ns, a.get("api_key"), a.get("base_url"), a.get("default_base"))
    if func == "log_context":
        return Mixin._anthropic_log_context(types.SimpleNamespace(**(r.get("agent") or {})))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    try:
        py = drive(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s: %r" % (r.get("func"), exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)

print("ORACLE w8_clientlifec_20bd72: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or cases == 0) else 0)
