#!/usr/bin/env python3
"""Oracle for upstream Hermes agent/image_replay.py (gate w6_imagereplay_7778b4).

_retained_data_url is monkeypatched with the fixture's injected URL map so no
filesystem access occurs; session_rules.turns is stubbed (leaf import).
"""
import copy
import importlib.util
import json
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

# parent-package bootstrap + stub for the leaf import
sr = types.ModuleType("session_rules")
sr.__path__ = [QUARRY + "/session_rules"]
sys.modules["session_rules"] = sr
srt = types.ModuleType("session_rules.turns")
srt._IMAGE_PART_TYPES = {"image", "image_url", "input_image"}
sys.modules["session_rules.turns"] = srt

spec = importlib.util.spec_from_file_location(
    "live_image_replay", QUARRY + "/agent/image_replay.py")
live = importlib.util.module_from_spec(spec)
sys.modules["live_image_replay"] = live
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
    if func == "resolve":
        py = live.resolve_replay_recent_images(r.get("cfg"))
    elif func == "frontier":
        py = live.replay_frontier(r.get("total", 0), r.get("keep", 1))
    elif func == "split":
        py = live._split_rebuilt_row(r.get("content", ""))
        if py is not None:
            parts = []
            for kind, value in py:
                if kind == "text":
                    parts.append(["text", value])
                else:
                    ref, detail, marker = value
                    parts.append(["image", ref, detail, marker])
            py = parts
    elif func == "replay":
        urls = r.get("urls") or {}
        live._retained_data_url = lambda ref: urls.get(ref)
        msgs = copy.deepcopy(r.get("messages") or [])
        py = live.replay_recent_images(msgs, r.get("keep", 3))
        py = {"attached": py, "messages": msgs}
    else:
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w6_imagereplay_7778b4: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
