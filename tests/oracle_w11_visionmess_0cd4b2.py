#!/usr/bin/env python3
"""Differential oracle for gate w11_visionmess_0cd4b2 (agent/vision_message_prep.py).

Live pure helpers run directly; base_url_host_matches / base_url_hostname are the
live functions from utils/urls.py (the same ones the module imports).
"""
import importlib.util
import json
import os
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


def _load(path, name):
    sp = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(sp)
    sys.modules[name] = m
    sp.loader.exec_module(m)
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


agent_pkg = _mod("agent")
agent_pkg.__path__ = []
_mod("agent.lazy_forward", forward_static=lambda *a, **k: (lambda f: f))
sr_pkg = _mod("session_rules")
sr_pkg.__path__ = []
_mod("session_rules.turns",
     _is_multimodal_tool_result=lambda v: isinstance(v, dict) and "content" in v,
     _multimodal_text_summary=lambda v: "[image]")
urls = _load(QUARRY + "/utils/urls.py", "w11_visionmess_urls_live")
_mod("utils",
     base_url_host_matches=urls.base_url_host_matches,
     base_url_hostname=urls.base_url_hostname)

SRC = QUARRY + "/agent/vision_message_prep.py"
spec = importlib.util.spec_from_file_location("w11_visionmess_0cd4b2_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

M = live.VisionMessagePrepMixin


def compute(r):
    global R
    R = r
    f = r.get("func")
    if f == "is_image_part":
        return bool(live._is_image_part(r.get("part")))
    if f == "salvage":
        return live._salvage_text_parts(r.get("content", []),
                                        any_dict_text=bool(r.get("any_dict_text", False)))
    if f == "content_has_image":
        return bool(M._content_has_image_parts(r.get("content")))
    if f == "provider_model_key":
        a = types.SimpleNamespace(provider=r.get("provider", "") or "", model=r.get("model", "") or "")
        return list(live._provider_model_key(a))
    if f == "preserve_dots":
        a = types.SimpleNamespace(provider=r.get("provider", "") or "", base_url=r.get("base_url", "") or "")
        return bool(M._anthropic_preserve_dots(a))
    if f == "qwen_portal":
        a = types.SimpleNamespace(_base_url_lower=r.get("base_url_lower", "") or "")
        return bool(M._is_qwen_portal(a))
    return None


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
        if not isinstance(r, dict) or "out" not in r:
            continue
        cases += 1
        try:
            py = compute(r)
        except Exception as exc:
            mism += 1
            print("LIVE ERROR case=%s %r" % (r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH case=%s\n  py=%r\n  c =%r" % (r.get("case"), py, got), file=sys.stderr)
    print("ORACLE w11_visionmess_0cd4b2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
