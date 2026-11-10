#!/usr/bin/env python3
"""
sta_oracle_gateway_wake.py — Oracle for the pure shape helpers of gateway/wake.py.

adapter_supports_push / _runner_draining take an object in Python, so the
harness's (present, value) pairs are rebuilt as a SimpleNamespace with or
without the attribute. _delegation_display_metadata is compared directly, with
warning_notifications_enabled stubbed to the caller-supplied flag.
"""
import json
import sys
import types

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.path.insert(0, "C:/Users/eman5/hermes-agent/gateway")

_wn = types.ModuleType("gateway.warning_notifications")
_wn.warning_notifications_enabled = lambda platform: True
sys.modules.setdefault("gateway.warning_notifications", _wn)

import gateway.wake as WAKE  # noqa: E402

EVTS = [
    "{}",
    '{"delegation_id":"dq1","results":[]}',
    '{"delegation_id":"dq2","results":[{"status":"completed"},{"status":"success"}]}',
    '{"delegation_id":"dq3","results":[{"status":"failed"},{"status":"error"},{"status":"completed"}]}',
    '{"delegation_id":"dq4","results":"nope","duration_seconds":2.5}',
    '{"delegation_id":"dq5","results":[{"status":"failed","task_index":3}],"task_failure_notice":true}',
    '{"delegation_id":"dq6","results":[{"status":"failed","task_index":0}],"task_failure_notice":"yes","total_duration_seconds":9}',
    '{"delegation_id":null,"results":[7,"x",{"status":"failed"}],"task_failure_notice":1}',
    '{"delegation_id":"dq8","results":[{"status":"weird"}],"duration_seconds":"3"}',
]


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE gateway_wake: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam, case = c["family"], c["case"]
        if fam == "push":
            adapter = types.SimpleNamespace()
            if c["has"]:
                adapter.supports_async_delivery = bool(c["val"])
            want = WAKE.adapter_supports_push(adapter)
            got = bool(c["ret"])
        elif fam == "draining":
            idx = int(case.split("_")[1])
            runner, present, draining = [
                (0, 1, 1), (1, 0, 1), (1, 1, 1), (1, 1, 0)][idx]
            adapter = types.SimpleNamespace()
            if runner:
                gw = types.SimpleNamespace()
                if present:
                    gw._draining = bool(draining)
                adapter.gateway_runner = gw
            want = WAKE._runner_draining(adapter)
            got = bool(c["ret"])
        elif fam == "metadata":
            parts = case.split("_")
            evt = json.loads(EVTS[int(parts[1])])
            enabled = bool(int(parts[2]))
            _wn.warning_notifications_enabled = lambda platform, _e=enabled: _e
            want = WAKE._delegation_display_metadata(evt)
            got = c["out"]
        else:
            continue

        if got != want:
            mism += 1
            print(f"MISMATCH {case} [{fam}]:\n  C ={got}\n  PY={want}")

    print(f"ORACLE gateway_wake: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
