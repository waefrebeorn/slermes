"""twin for threatpatterns_first_match.

Traces the LIVE tools/threat_patterns.py pattern sweep with the SAME
first-match rung order as the C harness: B(matched) then VSTR(pattern_id).
The LIVE scan_for_threats() result is asserted against the mirrored pid.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_spec = ilu.spec_from_file_location("wc_live_threat_patterns",
                                    os.path.join(DEV, "tools", "threat_patterns.py"))
_live = ilu.module_from_spec(_spec)
sys.modules["wc_live_threat_patterns"] = _live
_spec.loader.exec_module(_live)


def trace(scope, content):
    c = Cup("threatpatterns_first_match")
    first_pid = None
    for compiled, pid in _live._COMPILED[scope]:
        if compiled.search(content):
            first_pid = pid
            break
    matched = first_pid is not None
    c.branch(matched)
    if matched:
        c.valstr(first_pid)

    findings = [f for f in _live.scan_for_threats(content, scope=scope)
                if not f.startswith("invisible_unicode_")]
    live_first = findings[0] if findings else None
    assert live_first == first_pid, (live_first, first_pid, scope, content)
    return c.finalize()


def run(inputs):
    out = []
    for inp in inputs:
        tag, content = inp[3:].split("|", 1)
        out.append(trace(tag, content))
    return out
