#!/usr/bin/env python3
"""Differential oracle for tools/approval_notify_lease.py (gate w15_approvalno_b886ee)."""
import importlib.util
import json
import sys
import threading
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

NAME = "tools.approval_notify_lease_live"
spec = importlib.util.spec_from_file_location(NAME, QUARRY + "/tools/approval_notify_lease.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

# Stub the late-imported tools.approval transport: only these attributes are read.
_fake_approval = types.ModuleType("tools.approval")
_fake_approval._lock = threading.Lock()
_fake_approval._gateway_notify_cbs = {}
_fake_approval._gateway_queues = {}
sys.modules["tools.approval"] = _fake_approval
import tools  # noqa: E402
tools.approval = _fake_approval


def compute(r):
    f = r.get("func")
    sk = r.get("session_key", "s")
    if f == "current":
        ck = r.get("current_lease_key")
        if ck is None:
            return False
        lease = live.NotifyLease(ck, None)
        token = live._current.set(lease)
        try:
            return live.current(sk) is not None
        finally:
            live._current.reset(token)
    if f == "deliverable":
        cb = (lambda data: None) if r.get("has_callback") else None
        lease = live.NotifyLease("s", cb)
        lease.active = bool(r.get("active"))
        _fake_approval._gateway_notify_cbs = {"s": object()} if r.get("gateway_has_notifier") else {}
        return lease.deliverable()
    if f == "revoke":
        leases = [live.NotifyLease(k, None) for k in (r.get("lease_keys") or [])]
        before = list(live._leases)
        for lease in leases:
            live._leases.add(lease)
        try:
            live.revoke_session_locked(sk)
            return [lease.active for lease in leases]
        finally:
            for lease in leases:
                live._leases.discard(lease)
            for lease in before:
                live._leases.add(lease)
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
        print(f"ORACLE w15_approvalno_b886ee: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w15_approvalno_b886ee {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w15_approvalno_b886ee: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
