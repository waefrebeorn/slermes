#!/usr/bin/env python3
"""Differential oracle for tools/web_tools_truncate.py (gate w11_webtoolst_dae278)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/tools/web_tools_truncate.py"
spec = importlib.util.spec_from_file_location("w11_webtoolst_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_webtoolst_live"] = live
spec.loader.exec_module(live)

# _get_extract_char_limit resolves tools.web_tools._load_web_config lazily; inject a
# deterministic config seam so the fixture's config_value drives that branch.
tools_pkg = types.ModuleType("tools")
tools_pkg.__path__ = []
sys.modules["tools"] = tools_pkg
web_tools = types.ModuleType("tools.web_tools")
def _load_web_config():
    return {"extract_char_limit": CURRENT.get("config_value")}
web_tools._load_web_config = _load_web_config
sys.modules["tools.web_tools"] = web_tools

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    got = r.get("out")
    try:
        if func == "clamp":
            try:
                want = live._clamp_char_limit(r.get("value"))
            except (TypeError, ValueError):
                want = "__ERR__"
        elif func == "clamp_or_default":
            want = live._clamp_or_default(r.get("value"))
        elif func == "effective":
            CURRENT = r
            want = live._effective_char_limit(r.get("char_limit"))
        elif func == "convert":
            want = live.convert_base64_images_to_links(r.get("text", ""))
        elif func == "trim":
            want = live._trim_results(r.get("results"))
        else:
            continue
    except BaseException as ex:
        print("MISMATCH %s oracle raised %s: %s" % (func, type(ex).__name__, ex), file=sys.stderr)
        cases += 1
        mism += 1
        continue
    cases += 1
    if want != got:
        mism += 1
        print("MISMATCH in=%s py=%r c=%r" % (json.dumps(r, ensure_ascii=False), want, got), file=sys.stderr)
print("ORACLE w11_webtoolst_dae278: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or cases == 0 else 0)
