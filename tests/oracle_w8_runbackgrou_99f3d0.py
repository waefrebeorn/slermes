#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/run_background.py
(gate w8_runbackgrou_99f3d0).

_bg_prompt_preview is compared directly. The two inline expressions from
_run_background_task_inner (complete header preview and image-path selection)
are compared against the source semantics; the module's heavy imports are
stubbed so it loads in isolation.
"""
import importlib.util
import json
import sys
import types

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
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

agent = _mod("agent"); agent.__path__ = []
_mod("agent.i18n", t=lambda *a, **kw: "")
gateway = _mod("gateway"); gateway.__path__ = []
_mod("gateway.media_repair", repair_explicit_computer_use_media_paths=lambda s, m: s)
_mod("gateway.platforms.base", BasePlatformAdapter=type("BasePlatformAdapter", (), {}))
_mod("gateway.session", SessionSource=object)

spec = importlib.util.spec_from_file_location("run_background_live", Q + "/gateway/run_background.py")
live = importlib.util.module_from_spec(spec)
sys.modules["run_background_live"] = live
spec.loader.exec_module(live)

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
    try:
        if func == "bg_preview":
            py = live._bg_prompt_preview(r.get("prompt"), r.get("limit", 60))
        elif func == "complete_preview":
            p = r.get("prompt") or ""
            py = p[:60] + ("..." if len(p) > 60 else "")
        elif func == "image_paths":
            urls = r.get("urls") or []
            types_ = r.get("media_types") or []
            py = [p for i, p in enumerate(urls)
                  if (types_[i] if i < len(types_) else "").startswith("image/")]
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        mism += 1
        cases += 1
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w8_runbackgrou_99f3d0: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
