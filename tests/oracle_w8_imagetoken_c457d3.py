#!/usr/bin/env python3
"""Differential oracle for agent/image_token_cost.py (w8_imagetoken_c457d3 gate).

Collaborators (base_url_hostname, _count_image_tokens, anchored_context_tokens,
_table, atomic_json_write) are injected from fixtures; the LIVE functions are
driven with the same inputs and their return compared to the C harness.
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

STATE = {"hosts": {}, "table": {}, "text_only": 0}


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


agent = _mod("agent")
agent.__path__ = []
_mod("agent.model_metadata",
     _load_json_dict=lambda p: {},
     _count_image_tokens=lambda m, n: (int(m.get("_images", 0)) if isinstance(m, dict) else 0))
utils = _mod("utils",
             base_url_hostname=lambda u: STATE["hosts"].get(u or "", ""),
             atomic_json_write=lambda *a, **k: None)
_mod("hermes_constants",
     get_hermes_home_override=lambda: None, hermes_home_key=lambda: "")
_mod("agent.usage_anchor",
     anchored_context_tokens=lambda messages, anchor: STATE["text_only"])

SRC = Q + "/agent/image_token_cost.py"
spec = importlib.util.spec_from_file_location("w8_imagetoken_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w8_imagetoken_live"] = live
spec.loader.exec_module(live)


def set_injected(hosts, table, text_only):
    STATE["hosts"] = hosts or {}
    STATE["table"] = dict(table or {})
    STATE["text_only"] = text_only or 0
    live._table = lambda: STATE["table"]


def drive(r):
    func = r.get("func")
    if func == "key":
        set_injected(r.get("hosts"), None, None)
        return live._key(r.get("model"), r.get("base_url"))
    if func == "learned":
        set_injected(r.get("hosts"), r.get("table"), None)
        return live.learned_image_token_cost(r.get("model"), r.get("base_url"))
    if func == "current":
        tok = live._image_cost_var.set(r.get("bound"))
        try:
            return live.current_image_token_cost()
        finally:
            live._image_cost_var.reset(tok)
    if func == "calibrate":
        a = r.get("args") or {}
        set_injected(a.get("hosts"), a.get("table"), a.get("text_only"))
        ag = types.SimpleNamespace(_usage_anchor=a.get("anchor"),
                                   model=a.get("model"), base_url=a.get("base_url"))
        return live.calibrate_from_usage(ag, a.get("messages") or [], a.get("prompt_tokens"))
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

print("ORACLE w8_imagetoken_c457d3: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or cases == 0) else 0)
