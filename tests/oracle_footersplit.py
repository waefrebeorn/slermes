#!/usr/bin/env python3
"""Oracle for upstream Hermes hermes_cli/cli_footer_split.py (footersplit gate).

Forces the base HSplit._divide_heights to decline (returning None), then calls
the LIVE FooterSplit._divide_heights with controlled child minimum heights so
its overflow allocation can be compared against the C port.
"""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/hermes_cli/cli_footer_split.py"
spec = importlib.util.spec_from_file_location("footersplit_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

import prompt_toolkit.layout as ptl
ptl.HSplit._divide_heights = lambda self, wp: None  # force overflow path
class _App:
    def invalidate(self):
        pass
live.get_app = lambda: _App()
class _Kid:
    def __init__(self, m):
        self.m = m
    def preferred_height(self, width, height):
        return types.SimpleNamespace(min=self.m)
inst = object.__new__(live.FooterSplit)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") != "div":
        continue
    mins = r.get("mins")
    if not isinstance(mins, list):
        continue
    kids = [_Kid(int(m)) for m in mins]
    live.FooterSplit._all_children = property(lambda self, kids=kids: kids)
    wp = types.SimpleNamespace(height=int(r.get("budget", 0)), width=80)
    py = list(live.FooterSplit._divide_heights(inst, wp))
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH div case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE footersplit: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
