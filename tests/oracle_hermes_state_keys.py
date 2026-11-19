#!/usr/bin/env python3
"""Oracle for persistence.sqlite._state.keys.py + the two key builders in
hermes_state_runtime.py (replacement_binding, mutation_receipt_key).

The harness (t_port_hermes_state_keys.c) emits one JSON line per case with a
"fn" discriminator. The fingerprints are SHA-256 over canonical JSON, so a
single byte of difference shows up as a different digest.
"""
import json
import os
import sys

for _quarry in ("C:/Users/eman5/hermes-agent", os.path.expanduser("~/hermes-agent")):
    if os.path.isdir(os.path.join(_quarry, "persistence")):
        sys.path.insert(0, _quarry)
        break

try:
    from persistence.sqlite._state.keys import (                      # noqa: E402
        admission_fingerprint, cron_request_id, cron_request_range,
        cron_run_from_request, legacy_cron_run_job, local_identity,
        prefix_range, profile_from_session_key,
        profile_from_session_key_namespace,
    )
    from persistence.authority.runtime import mutation_receipt_key
    from persistence.sqlite.authority._runtime import replacement_binding
except ImportError:
    # The suite interpreter is the quarry venv; re-exec under it when the
    # current interpreter lacks the quarry import chain (stdin survives execv).
    _venv = "C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe"
    if not os.path.isfile(_venv):
        _venv = os.path.expanduser("~/hermes-agent/.venv/bin/python")
    if os.path.isfile(_venv) and \
            os.path.abspath(sys.executable) != os.path.abspath(_venv):
        os.execv(_venv, [_venv, os.path.abspath(__file__)] + sys.argv[1:])
    raise

sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


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
        print("ORACLE persistence.sqlite._state.keys: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE persistence.sqlite._state.keys: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
