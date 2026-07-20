#!/usr/bin/env python3
"""
sta_oracle_credential_sanitize.py — Python oracle for the DISK-SAFETY boundary
sanitize_borrowed_credential_payload() in agent/credential_persistence.py,
ported in src/agent/credential_pool_persistence.c.

Imports the REAL agent.credential_persistence module and calls the genuine
function. Output contract matches tests/t_port_credential_sanitize.c: one JSON
object per line with provider, in (payload), and out (sanitized payload,
compact, sorted-key serialization).
"""

import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from agent.credential_persistence import (  # noqa: E402
    sanitize_borrowed_credential_payload,
)


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def main():
    for raw in sys.stdin:
        line = raw.rstrip("\n")
        if not line.strip() or line.startswith("#"):
            continue
        # Format: sani <provider_id> <payload_json>
        # Skip the leading op token, then split provider from payload.
        rest = line.split(" ", 1)[1] if len(line.split(" ", 1)) > 1 else ""
        parts = rest.split(" ", 1)
        provider = parts[0]
        payload_str = parts[1] if len(parts) > 1 else ""
        try:
            payload = json.loads(payload_str) if payload_str else None
        except Exception:
            payload = None
        result = sanitize_borrowed_credential_payload(payload, provider) if payload is not None else None
        emit({
            "provider": provider,
            "in": payload_str,
            "out": json.dumps(result, separators=(",", ":"), ensure_ascii=False)
                     if result is not None else None,
        })


if __name__ == "__main__":
    sys.exit(main())
