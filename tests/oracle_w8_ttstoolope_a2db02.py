#!/usr/bin/env python3
"""Differential oracle for tools/tts_tool_openai.py (gate w8_ttstoolope_a2db02).

The two ported helpers are pure; the four heavy sibling imports are stubbed so
the LIVE module's own function bodies (from the quarry) are what actually run.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def pkg(name, path):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = [path]
        sys.modules[name] = m
    return sys.modules[name]


pkg("tools", QUARRY + "/tools")


def stub(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m


stub("tools.managed_tool_gateway", resolve_managed_tool_gateway=lambda *a, **k: None)
stub("tools.tool_backend_helpers",
     NOUS_MANAGED_PROVIDER="nous",
     managed_nous_tools_enabled=lambda: False,
     nous_tool_gateway_unavailable_message=lambda *a: "",
     read_selection=lambda *a: None,
     resolve_openai_audio_api_key=lambda: None,
     selection_error=lambda *a, **k: "error")
stub("tools.tts_tool_delivery",
     _origin=lambda: None,
     _section=lambda c, s: (c.get(s, {}) if isinstance(c, dict) else {}))
stub("tools.tts_tool_providers", _tts_response_format_from_path=lambda p: "mp3")


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


live = load("w8_ttstoolope_a2db02_live", QUARRY + "/tools/tts_tool_openai.py")


class _Exc(Exception):
    pass


def compute(r):
    func = r.get("func")
    if func == "openai_extra_body":
        return live._openai_extra_body(r.get("oai_config") or {})
    if func == "is_response_format_rejection":
        exc = _Exc(r.get("message", ""))
        status = r.get("status_code")
        if status is not None:
            exc.status_code = status
        return live._is_response_format_rejection(exc)
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
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print("ORACLE w8_ttstoolope_a2db02: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w8_ttstoolope_a2db02 %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w8_ttstoolope_a2db02: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
