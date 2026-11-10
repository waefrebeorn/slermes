#!/usr/bin/env python3
"""Oracle for gateway/session_server_requests.py::is_response_frame."""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import gateway.session_server_requests as SSR  # noqa: E402

FRAMES = [
    '{"jsonrpc":"2.0","id":"srq-abc123","result":{"value":"x"}}',
    '{"id":"srq-abc123","error":{"code":1,"message":"no"}}',
    '{"id":"srq-abc123"}',
    '{"id":"srq-abc123","result":null}',
    '{"id":"other-1","result":{}}',
    '{"id":"srq-","result":{}}',
    '{"id":"SRQ-upper","result":{}}',
    '{"id":42,"result":{}}',
    '{"id":"srq-abc","result":{},"method":"x"}',
    '{"method":"x","id":"srq-abc"}',
    '[]',
    '"just a string"',
    "null",
    "{}",
]


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
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
        print("ORACLE session_server_requests: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        idx = int(c["case"].split("_")[1])
        frame = json.loads(FRAMES[idx])
        want = SSR.is_response_frame(frame)
        if bool(c["ret"]) != bool(want):
            mism += 1
            print(f"MISMATCH {c['case']}: C={c['ret']} PY={want}")

    print(f"ORACLE session_server_requests: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
