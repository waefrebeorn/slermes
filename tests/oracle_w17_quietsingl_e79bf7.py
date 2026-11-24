#!/usr/bin/env python3
"""Differential oracle for hermes_cli/quiet_single_query.py (w17_quietsingl_e79bf7).

All three ported functions are pure over explicit inputs. The two imported
predicates used by _diagnostic_only_wake_muted are stubbed and injected per case;
the LIVE functions are called directly.
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# Parent-package bootstrap + stubs for the two call-time imports.
for pname, sub in (("agent", "agent/notification_presentation"),
                   ("gateway", "gateway/warning_notifications")):
    pkg = types.ModuleType(pname)
    pkg.__path__ = [os.path.join(Q, pname)]
    sys.modules.setdefault(pname, pkg)

ap = types.ModuleType("agent.notification_presentation")
ap.diagnostic_process_event = lambda e: bool(e.get("diagnostic"))
sys.modules["agent.notification_presentation"] = ap

gw = types.ModuleType("gateway.warning_notifications")
gw.warning_notifications_enabled = lambda channel: False
sys.modules["gateway.warning_notifications"] = gw

SRC = Q + "/hermes_cli/quiet_single_query.py"
spec = importlib.util.spec_from_file_location("w17_quietsingl_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w17_quietsingl_live"] = live
spec.loader.exec_module(live)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "take_report_path":
            m = dict(r.get("environ") or {})
            val = live.take_turn_report_path(m)
            py = {"value": val if val else None, "remaining": m}
        elif func == "exit_trailer":
            py = "\n" + live.KANBAN_WORKER_EXIT_TRAILER + str(int(r.get("code", "0")))
        elif func == "diagnostic_muted":
            gw.warning_notifications_enabled = lambda channel: bool(r.get("warnings_enabled", False))
            py = bool(live._diagnostic_only_wake_muted(r.get("events")))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w17_quietsingl_e79bf7: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
