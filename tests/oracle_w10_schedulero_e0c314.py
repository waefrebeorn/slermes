#!/usr/bin/env python3
"""Differential oracle for cron/scheduler_ownership.py (gate w10_schedulero_e0c314)."""
import importlib.util
import json
import socket
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

SRC = QUARRY + "/cron/scheduler_ownership.py"
NAME = "scheduler_ownership_live_w10_schedulero"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

import gateway.status as gs  # noqa: E402  (late: quarry path + live module set up first)


def compute(r):
    func = r.get("func")
    if func == "claim_dead":
        live_pids = set(str(x) for x in (r.get("live_pids") or []))
        old_host, old_exists = socket.gethostname, gs._pid_exists
        try:
            socket.gethostname = lambda: r.get("hostname", "")
            gs._pid_exists = lambda pid: str(pid) in live_pids
            return bool(live._claim_owner_is_dead({"by": r.get("by")}))
        finally:
            socket.gethostname, gs._pid_exists = old_host, old_exists
    if func == "record_serves":
        old_ph, old_sh, old_lbl = (gs._get_process_hermes_home,
                                  gs._same_hermes_home,
                                  gs._profile_label_for_home)
        try:
            gs._get_process_hermes_home = lambda: r.get("process_home", "")
            gs._same_hermes_home = lambda a, b: bool(r.get("same", False))
            gs._profile_label_for_home = lambda h: r.get("label")
            return bool(live.record_serves_profile(r.get("record"), r.get("home")))
        finally:
            (gs._get_process_hermes_home, gs._same_hermes_home,
             gs._profile_label_for_home) = old_ph, old_sh, old_lbl
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
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
              file=sys.stderr)
print("ORACLE w10_schedulero_e0c314: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
