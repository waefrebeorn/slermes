#!/usr/bin/env python3
"""Differential oracle for gateway/host_rendezvous.py (gate w9_hostrendezv_37dd94)."""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
import gateway  # parent-package bootstrap

SRC = QUARRY + "/gateway/host_rendezvous.py"
NAME = "host_rendezvous_live_w9_hostrendezv"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def payload(r):
    pid = r.get("pid")
    port = r.get("port")
    return {
        "pid": int(pid) if isinstance(pid, str) and pid.lstrip("-").isdigit() else pid,
        "role": r.get("role", "gateway"),
        "host": r.get("host", ""),
        "port": int(port) if isinstance(port, str) and port.lstrip("-").isdigit() else None,
        "protocolVersion": 1,
        "profiles": list(r.get("profiles") or []),
        "tokenFingerprint": "",
        "updatedAt": "",
        "home": "",
        "createTime": None,
        "startTime": None,
        "bootEpoch": "",
    }


def compute(r):
    func = r.get("func")
    if func == "role":
        try:
            return live._validated_role(r.get("role", ""))
        except ValueError:
            return None
    if func == "from_json_ok":
        return live.HostRecord.from_json(payload(r)) is not None
    rec = live.HostRecord.from_json(payload(r))
    if func == "describe":
        return live.describe(rec)
    if func == "dial":
        return live.dial_host(rec)
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
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w9_hostrendezv_37dd94: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
