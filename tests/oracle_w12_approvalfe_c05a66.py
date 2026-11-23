#!/usr/bin/env python3
"""Differential oracle for tools/approval_fetch_exec.py (gate w12_approvalfe_c05a66).

The live module is loaded from the quarry with its two stdlib-only dependencies
loaded for real.  `base` is compared against the live _base; `fetch_targets`
replays search()'s step 1 with the LIVE compiled regex objects
(_FETCH_RE / _SEGMENT_END_RE / _TARGET_RE / _base), so the C scanner is checked
against the genuine patterns.  Caveat: search()'s curl --remote-name / URL
basename extension is not ported; fixtures stay inside the -o/--output/redirect
subset.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def _load(name, rel):
    spec = importlib.util.spec_from_file_location(name, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


tools = types.ModuleType("tools")
tools.__path__ = [QUARRY + "/tools"]
sys.modules["tools"] = tools
_load("tools.approval_pipe_launchers", "tools/approval_pipe_launchers.py")
_load("tools.shell_heredoc", "tools/shell_heredoc.py")
live = _load("approval_fetch_exec_live", "tools/approval_fetch_exec.py")


def fetch_targets(command):
    targets = {}
    pos = 0
    while True:
        fetch = live._FETCH_RE.search(command, pos)
        if fetch is None:
            break
        end_match = live._SEGMENT_END_RE.search(command, fetch.end())
        seg_end = end_match.start() if end_match else len(command)
        for target in live._TARGET_RE.finditer(command, fetch.end(), seg_end):
            targets.setdefault(live._base(target.group(1)), target.end())
        pos = seg_end
        if seg_end >= len(command):
            break
    return targets


def compute(r):
    func = r.get("func")
    if func == "base":
        return live._base(r.get("path", ""))
    if func == "fetch_targets":
        return fetch_targets(r.get("command", ""))
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_approvalfe_c05a66: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
