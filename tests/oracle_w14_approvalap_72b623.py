#!/usr/bin/env python3
'''Differential oracle for gate w14_approvalap_72b623 (tools/approval_applescript.py).'''
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_approvalap_72b623"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    return mod


if "tools" not in sys.modules:
    _seed("tools")
_seed("tools.approval_ansi_c", expand_ansi_c_quotes=lambda s: s)
_seed("tools.approval_detection",
      _deobfuscate_shell_word_for_detection=lambda w: w,
      _iter_shell_command_word_spans=lambda t: [],
      _shell_command_segment=lambda t, s: "",
      _interpreter_family=lambda n: None,
      _scan_shell=lambda t: [],
      _shell_segment_tokens=lambda t, i: [])

SRC = QUARRY + "/tools/approval_applescript.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    f = r.get("func")
    if f == "mentions":
        return live._mentions_osascript(r.get("text", ""))
    if f == "strip_layout":
        return live._strip_layout(r.get("program", ""))
    if f == "heredoc_body":
        body, span = live._heredoc_body(r.get("text", ""), int(r.get("after", 0)),
                                        r.get("delim", ""), bool(r.get("strip_tabs", False)))
        return {"body": body, "start": span[0], "end": span[1]}
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
