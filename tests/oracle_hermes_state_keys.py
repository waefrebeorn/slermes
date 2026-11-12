#!/usr/bin/env python3
"""Oracle for hermes_state_keys.py + the two key builders in
hermes_state_runtime.py (replacement_binding, mutation_receipt_key).

The harness (t_port_hermes_state_keys.c) emits one JSON line per case with a
"fn" discriminator. The fingerprints are SHA-256 over canonical JSON, so a
single byte of difference shows up as a different digest.
"""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

from hermes_state_keys import (                          # noqa: E402
    admission_fingerprint, cron_request_id, cron_request_range,
    cron_run_from_request, legacy_cron_run_job, local_identity,
    prefix_range, profile_from_session_key,
    profile_from_session_key_namespace,
)
from hermes_state_runtime import (                       # noqa: E402
    mutation_receipt_key, replacement_binding,
)


def main() -> int:
    total = mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("fn")
        i = c.get("in") or {}
        try:
            if fn == "cron_request_id":
                want = cron_request_id(i["job_id"], i["execution_id"])
            elif fn == "prefix_range":
                lo, hi = prefix_range(i["prefix"])
                want = {"lo": lo, "hi": hi}
            elif fn == "cron_request_range":
                lo, hi = cron_request_range(i["job_id"])
                want = {"lo": lo, "hi": hi}
            elif fn == "cron_run_from_request":
                v = cron_run_from_request(i["request_id"], i.get("job_id"))
                want = None if v is None else {"job": v[0], "exec": v[1]}
            elif fn == "legacy_cron_run_job":
                want = legacy_cron_run_job(i.get("session_id"))
            elif fn == "admission_fingerprint":
                want = admission_fingerprint(canonical_target=i["target"],
                                             payload=i["payload"])
            elif fn == "local_identity":
                want = local_identity(i["profile_id"], i["principal_id"],
                                      i["request_id"])
            elif fn == "profile_from_session_key_namespace":
                want = profile_from_session_key_namespace(i["namespace"])
            elif fn == "profile_from_session_key":
                want = profile_from_session_key(i.get("session_key"))
            elif fn == "replacement_binding":
                want = replacement_binding(i["principal_id"], i["session_id"],
                                           i["request_id"])
            elif fn == "mutation_receipt_key":
                k, d = mutation_receipt_key(
                    i["principal_id"], i["session_id"], i["request_id"],
                    operation=i["operation"], payload=i["payload"],
                    expected_revision=i["expected_revision"],
                    expected_generation=i.get("expected_generation"))
                want = {"key": k, "digest": d}
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"MISMATCH {c.get('case')} ({fn}): python raised {exc!r}")
            mism += 1
            continue
        raw = c["out"]
        got = raw["str"] if isinstance(raw, dict) and set(raw) == {"str"} else raw
        if got != want:
            mism += 1
            print(f"MISMATCH {c.get('case')} ({fn}): C={str(c['out'])[:200]} "
                  f"PY={want}")
    if total == 0:
        print("ORACLE hermes_state_keys: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE hermes_state_keys: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
