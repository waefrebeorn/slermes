#!/usr/bin/env python3
"""
sta_oracle_hosted_room_execution_policy.py — Oracle for
gateway/hosted_room_execution_policy.py (validation + digest half).

Compares the canonical sha256 and the from_mapping/as_mapping round trip
against the LIVE dataclass.
"""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import gateway.hosted_room_execution_policy as HEP  # noqa: E402


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
        print("ORACLE hosted_room_execution_policy: 0 cases (link failure)")
        return 1

    mism = 0
    for c in cases:
        if c["family"] == "digest":
            unsigned = {
                "version": HEP.POLICY_VERSION,
                "target_profile": c["profile"],
                "enabled_toolsets": sorted(c["toolsets"]),
                "approval_mode": c["mode"],
                "max_iterations": c["iters"],
            }
            want = HEP._policy_digest(unsigned)
            if c["digest"] != want:
                mism += 1
                print(f"MISMATCH {c['case']}.digest: C={c['digest']} PY={want}")
            continue

        doc = json.loads(c["doc"])
        try:
            policy = HEP.RoomExecutionPolicy.from_mapping(doc)
        except HEP.RoomExecutionPolicyError as exc:
            if c["ok"] != 0:
                mism += 1
                print(f"MISMATCH {c['case']}: C accepted, PY raised {exc}")
            elif c["err"] != str(exc):
                mism += 1
                print(f"MISMATCH {c['case']}.err: C={c['err']!r} PY={str(exc)!r}")
            continue
        if c["ok"] != 1:
            mism += 1
            print(f"MISMATCH {c['case']}: C rejected ({c['err']!r}), PY accepted")
            continue
        want = policy.as_mapping()
        got = c["mapping"]
        if got != want:
            mism += 1
            print(f"MISMATCH {c['case']}.mapping:\n  C ={got}\n  PY={want}")

    print(f"ORACLE hosted_room_execution_policy: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
