#!/usr/bin/env python3
"""Differential oracle for agent/turn_author.py (noc_w6_turnauthor_9c0bab.h)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/agent/turn_author.py"
sys.path.insert(0, QUARRY)

spec = importlib.util.spec_from_file_location("turn_author_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["turn_author_live"] = live
spec.loader.exec_module(live)


def _canon(x):
    if x is None:
        return ("null",)
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", round(float(x), 9))
    if isinstance(x, str):
        return ("str", x)
    if isinstance(x, (list, tuple)):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def compute(r):
    f = r["func"]
    if f == "env_key":
        return live.TURN_AUTHOR_ENV
    if f == "clean_text":
        return live._clean_text(r.get("value"))
    if f == "bot_flag":
        return live._bot_flag(r.get("value"))
    if f == "bot_author_id":
        return live.bot_author_id(r.get("profile"), r.get("origin"))
    if f == "parse":
        return live.parse_turn_author(r.get("raw"))
    if f == "env":
        return live.turn_author_env(r.get("author"))
    if f == "a2a_key":
        return live.a2a_key(r.get("author"))
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
        print(f"ORACLE turnauthor: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    if _canon(py) != _canon(r.get("out")):
        mism += 1
        print(f"MISMATCH turnauthor {r.get('func')} case={r.get('case')} "
              f"py={py!r} c={r.get('out')!r}", file=sys.stderr)
print(f"ORACLE turnauthor: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
