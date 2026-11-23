#!/usr/bin/env python3
"""Differential oracle for tools/bot_live_delivery.py (gate w9_botlivedel_dad0de).

The two pure validators are called on the LIVE module; gateway/filesystem
imports are stubbed so those layers are never reached.
"""
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

_hda = types.ModuleType("hermes_dir_acl")
_hda.make_user_dir = lambda *a, **k: None
_hda.unusable_dir_remedy = lambda *a, **k: ""
sys.modules["hermes_dir_acl"] = _hda
_ut = types.ModuleType("utils")
_ut.atomic_json_write = lambda *a, **k: None
_ut.fsync_directory = lambda *a, **k: None
sys.modules["utils"] = _ut
if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg
_as = types.ModuleType("hermes_cli.active_sessions")
_as._FileLock = object
sys.modules["hermes_cli.active_sessions"] = _as


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


live = load("w9_botlivedel_dad0de_live", QUARRY + "/tools/bot_live_delivery.py")


def compute(r):
    f = r.get("func")
    if f == "delivery_id":
        try:
            return live._delivery_id(r.get("value", ""))
        except Exception as exc:
            return {"error": str(exc)}
    if f == "ticket_shape_error":
        return live._ticket_shape_error(
            types.SimpleNamespace(stem=r.get("stem", "")), r.get("record", {}))
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
        print("ORACLE w9_botlivedel_dad0de: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w9_botlivedel_dad0de %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w9_botlivedel_dad0de: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
