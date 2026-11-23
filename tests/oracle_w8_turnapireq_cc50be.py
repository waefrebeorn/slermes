#!/usr/bin/env python3
"""Differential oracle for agent/turn_api_request.py (w8_turnapireq_cc50be gate).

Drives the LIVE module with stubbed collaborators; the only injected value is
route_answer("request_headers"), supplied by each fixture case. Compares the
harness "out" object to the live result.
"""
import importlib.util
import json
import sys
import types

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
_mod("agent.message_sanitization",
     sanitize_outbound_kwargs=lambda *a, **k: None,
     strip_images_for_rejecting_model=lambda *a, **k: None)
_mod("agent.prompt_cache_lifecycle", cancel_prompt_cache_warming=lambda *a, **k: None)
hc = _mod("hermes_cli")
hc.__path__ = []
obs = _mod("hermes_cli.observability")
obs.__path__ = []
_mod("hermes_cli.observability.shared_metrics_efficiency",
     observe_request_tools=lambda *a, **k: None)
_mod("utils", env_var_enabled=lambda n: False)
proute = _mod("agent.provider_route_hooks", route_answer=lambda *a, **k: None)
agent.provider_route_hooks = proute

SRC = Q + "/agent/turn_api_request.py"
spec = importlib.util.spec_from_file_location("w8_turnapireq_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w8_turnapireq_live"] = live
spec.loader.exec_module(live)


def drive(r):
    func = r.get("func")
    if func == "set_extra_header":
        kw = dict(r.get("api_kwargs") or {})
        live._set_extra_header(kw, r.get("key", ""), r.get("value"))
        return kw
    if func == "apply_headers":
        spec_in = r.get("agent") or {}
        obj = types.SimpleNamespace(
            _is_user_initiated_turn=spec_in.get("_is_user_initiated_turn", False),
            provider=spec_in.get("provider", ""), base_url=spec_in.get("base_url", ""),
            model=spec_in.get("model", ""))
        hdrs = r.get("request_headers") or {}

        def ra(name, **kw):
            return hdrs
        proute.route_answer = ra
        kw = dict(r.get("api_kwargs") or {})
        live._apply_user_turn_request_headers(obj, kw)
        return {"api_kwargs": kw, "_is_user_initiated_turn": obj._is_user_initiated_turn}
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
        print("LIVE ERROR %s: %r" % (r.get("func"), exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)

print("ORACLE w8_turnapireq_cc50be: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or cases == 0) else 0)
