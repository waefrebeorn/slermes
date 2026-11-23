#!/usr/bin/env python3
"""Differential oracle for session_rules/media.py (gate w7_media_2c731e)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

_pkg = types.ModuleType("session_rules")
_pkg.__path__ = [QUARRY + "/session_rules"]
sys.modules.setdefault("session_rules", _pkg)

spec = importlib.util.spec_from_file_location(
    "w7_media_live", QUARRY + "/session_rules/media.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w7_media_live"] = live
spec.loader.exec_module(live)


def _norm(value):
    if isinstance(value, set):
        return sorted(value)
    return value


def compute(r):
    func = r.get("func")
    texts = tuple(r.get("texts") or [])
    if func == "hosted":
        return live.hosted_document_references(
            r.get("request_id"), r.get("payload") or {}, r.get("media_root"))
    if func == "admission":
        return live.admission_media_references(r.get("payload") or {})
    if func == "archive":
        return live.archive_refs_in_text(*texts)
    if func == "retirement":
        return live.retirement_candidates_in_text(*texts)
    if func == "spillover":
        return live.spillover_names_in_text(*texts)
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
    try:
        py = _norm(compute(r))
    except Exception as exc:
        print("ORACLE w7_media_2c731e: live raised %r case %r" % (exc, r.get("case")),
              file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH w7_media_2c731e case=%s func=%s py=%r c=%r" %
              (r.get("case"), r.get("func"), py, got), file=sys.stderr)

print("ORACLE w7_media_2c731e: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
