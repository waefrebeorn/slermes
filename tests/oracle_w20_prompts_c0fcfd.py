#!/usr/bin/env python3
"""Differential oracle for agent/pet/generate/prompts.py (w20_prompts_c0fcfd).

The live module has no import-time dependencies: it is loaded straight from the
quarry and every harness line is replayed through the real helper. Compared
values: the style-hint table, the state-action table, _spacing_spec's rounding,
BASE_VARIATIONS and both prompt builders, character for character.
"""
import importlib.util
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w20_prompts_c0fcfd_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "agent", "pet", "generate", "prompts.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def expected(r):
    f = r.get("func")
    if f == "style_hint":
        return live.style_hint(r.get("style"))
    if f == "state_action":
        return live.STATE_ACTIONS.get(r.get("state"), "a simple idle pose")
    if f == "spacing_spec":
        return list(live._spacing_spec(r.get("frame_count")))
    if f == "base_variations":
        return list(live.BASE_VARIATIONS)
    if f == "base_prompt":
        return live.build_base_prompt(r.get("concept"), style=r.get("style", "auto"),
                                      variation=r.get("variation", ""))
    if f == "row_prompt":
        return live.build_row_prompt(r.get("state"), r.get("frame_count"), r.get("concept"),
                                     style=r.get("style", "auto"))
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_prompts_c0fcfd: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
