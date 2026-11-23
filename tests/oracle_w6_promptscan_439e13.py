#!/usr/bin/env python3
"""Differential oracle for the C11 port of cron/prompt_scan.py (w6_promptscan_439e13).

The LIVE scanner is loaded by path with a `cron` parent-package bootstrap and
the genuine tools/threat_patterns module registered in sys.modules (it only
needs re/unicodedata). Compares _strip_invisible_unicode() on each echoed
fixture text against the harness output.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# Genuine threat-pattern constants (EMOJI_TAG_SEQUENCE_RE, INVISIBLE_CHARS).
tools_pkg = types.ModuleType("tools")
tools_pkg.__path__ = [Q + "/tools"]
sys.modules["tools"] = tools_pkg
tp_spec = importlib.util.spec_from_file_location("tools.threat_patterns", Q + "/tools/threat_patterns.py")
tp = importlib.util.module_from_spec(tp_spec)
sys.modules["tools.threat_patterns"] = tp
tp_spec.loader.exec_module(tp)

cron_pkg = types.ModuleType("cron")
cron_pkg.__path__ = []
sys.modules["cron"] = cron_pkg

SRC = Q + "/cron/prompt_scan.py"
spec = importlib.util.spec_from_file_location("w6_promptscan_439e13_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_promptscan_439e13_live"] = live
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
    if r.get("func") != "strip":
        continue
    text = r.get("text")
    try:
        cleaned, removed = live._strip_invisible_unicode(text)
        py = {"cleaned": cleaned, "removed": removed}
    except Exception as exc:
        print("LIVE ERROR case=%s: %r" % (r.get("case"), exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH case=%s C=%r PY=%r" % (r.get("case"), got, py), file=sys.stderr)

if cases == 0:
    print("ORACLE w6_promptscan_439e13: 0 cases (link failure in disguise)")
    sys.exit(1)
print("ORACLE w6_promptscan_439e13: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
