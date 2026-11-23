#!/usr/bin/env python3
"""Differential oracle for hermes_cli/setup_summary.py (gate w10_setupsumma_2652f3).

Pure tool-status row builders only; print/terminal and plugin discovery effects
are out of scope (the provider probe is injected as a `backend` seam).
"""
import importlib.util, json, sys, types
from types import SimpleNamespace

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
PKG = "w10_setupsumma_live_pkg"
_pkg = types.ModuleType(PKG); _pkg.__path__ = [Q + "/hermes_cli"]; sys.modules[PKG] = _pkg
spec = importlib.util.spec_from_file_location(PKG + ".setup_summary", Q + "/hermes_cli/setup_summary.py")
live = importlib.util.module_from_spec(spec); sys.modules[spec.name] = live; spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    feature = SimpleNamespace(**r.get("feature", {}))
    if f == "managed_or_provider_row":
        v = live._managed_or_provider_row(feature, r.get("name", ""), r.get("managed_label", ""),
                                          r.get("missing_hint", ""))
        return list(v)
    if f == "web_row":
        return list(live._web_row(None, SimpleNamespace(web=feature)))
    if f == "browser_row":
        hint = live._BROWSER_MISSING_HINTS.get(getattr(feature, "current_provider", None),
                                              live._BROWSER_MISSING_DEFAULT)
        return list(live._managed_or_provider_row(feature, "Browser Automation", "Nous Browser Use", hint))
    if f == "image_gen_row":
        backend = r.get("backend") or None
        original = live._first_available_plugin_provider
        live._first_available_plugin_provider = lambda reg, skip=None: backend
        try:
            v = live._image_gen_row(None, SimpleNamespace(image_gen=feature))
            return list(v) if v is not None else None
        finally:
            live._first_available_plugin_provider = original
    if f == "video_gen_row":
        backend = r.get("backend") or None
        original = live._first_available_plugin_provider
        live._first_available_plugin_provider = lambda reg, skip=None: backend
        try:
            v = live._video_gen_row(None, SimpleNamespace(video_gen=feature))
            return list(v) if v is not None else None
        finally:
            live._first_available_plugin_provider = original
    if f == "always_on_rows":
        return [list(x) for x in live._always_on_rows(None, None)]
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
        print("MISMATCH %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)
print("ORACLE w10_setupsumma_2652f3: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
