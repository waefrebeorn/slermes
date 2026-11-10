#!/usr/bin/env python3
"""Oracle for store_error_response's status decision — the pure branch logic
transcribed from gateway/platforms/api_server_authority_runs.py as the
independent reference (the HTTP response body construction is aiohttp IO).
"""
import json
import sys

_STORE_ERROR_STATUS = {'invalid_params': 400, 'runtime_draining': 503,
                       'storage_unavailable': 503, 'chain_moved': 503}


def expected(reason, is_runtime):
    reason = reason if is_runtime else 'storage_unavailable'
    if reason == 'admission_queued':
        return {'reason': reason, 'status': 503, 'queued': True,
                'x_should_retry_false': True, 'retry_after_1': False}
    status = _STORE_ERROR_STATUS.get(reason, 409)
    return {'reason': reason, 'status': status, 'queued': False,
            'x_should_retry_false': False, 'retry_after_1': status == 503}


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
        print("ORACLE api_store_error: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        want = expected(c["reason"], bool(c["rt"]))
        if c["out"] != want:
            mism += 1
            print(f"MISMATCH asr {c['case']}: C={c['out']} PY={want}")
    print(f"ORACLE api_store_error: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
