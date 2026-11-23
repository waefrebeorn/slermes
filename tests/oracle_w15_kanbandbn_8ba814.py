#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/kanban_db_notify.py (gate w15_kanbandbn_8ba814).

The late-bound origin namespace (hermes_cli.kanban_db) is stubbed: the ported
helpers (_sub_key, encode/decode metadata, profile filter) are pure and do not use it.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


hcli = _mod("hermes_cli")
hcli.__path__ = [QUARRY + "/hermes_cli"]
kb = _mod("hermes_cli.kanban_db", DEFAULT_BOARD="default")
hcli.kanban_db = kb

SRC = QUARRY + "/hermes_cli/kanban_db_notify.py"
spec = importlib.util.spec_from_file_location("w15_kanbandbn_8ba814_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w15_kanbandbn_8ba814_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "sub_key":
        return list(live._sub_key(r.get("task_id"), r.get("platform"),
                                  r.get("chat_id"), r.get("thread_id")))
    if func == "encode_meta":
        return live._encode_notify_delivery_metadata(r.get("metadata"))
    if func == "decode_meta":
        return live._decode_notify_delivery_metadata(r.get("raw"))
    if func == "profile_filter":
        return list(live._notify_profile_filter(
            r.get("notifier_profiles"),
            include_unowned=bool(r.get("include_unowned", False))))
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
        print(f"ORACLE w15_kanbandbn_8ba814: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w15_kanbandbn_8ba814: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
