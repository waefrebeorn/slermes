#!/usr/bin/env python3
"""
sta_oracle_credential_entry_to_json.py — Python oracle for the DISK-SAFE entry
serializer credential_entry_to_json() in src/agent/credential_pool_persistence.c
(port of agent/credential_pool.py:PooledCredential.to_dict() + sanitize).

Builds a real agent.credential_pool.PooledCredential from the same spec the C
harness uses, calls .to_dict() (which runs sanitize_borrowed_credential_payload),
and emits the result filtered to the field subset credential_entry_t actually
carries (the behavioral parity that matters for the disk boundary). Output
contract matches tests/t_port_credential_entry_to_json.c.
"""

import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from agent.credential_pool import PooledCredential  # noqa: E402

# Fields carried by credential_entry_t — the behavioral parity subset.
_C_FIELDS = {
    "label", "source", "scope", "base_url", "inference_base_url",
    "agent_key_expires_at", "expires_at_ms", "api_key", "access_token",
    "refresh_token", "agent_key", "extra", "secret_fingerprint",
}


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def main():
    for raw in sys.stdin:
        line = raw.rstrip("\n")
        if not line.strip() or line.startswith("#"):
            continue
        spec = json.loads(line)
        provider = spec.get("provider", "")
        entry = spec.get("entry", {}) or {}

        # Python models api_key/scope as extra keys (via __getattr__/_EXTRA_KEYS),
        # not dataclass fields. Pass them through `extra` so to_dict() hoists
        # them to top level, matching the C top-level emission.
        extra = dict(entry.get("extra") or {})
        if entry.get("api_key") is not None:
            extra["api_key"] = entry["api_key"]
        if entry.get("scope") is not None:
            extra["scope"] = entry["scope"]

        ce = PooledCredential(
            provider=provider,
            id=entry.get("id", ""),
            label=entry.get("label", ""),
            auth_type=entry.get("auth_type", ""),
            priority=entry.get("priority", 0),
            source=entry.get("source", ""),
            access_token=entry.get("access_token"),
            refresh_token=entry.get("refresh_token"),
            base_url=entry.get("base_url"),
            inference_base_url=entry.get("inference_base_url"),
            agent_key=entry.get("agent_key"),
            agent_key_expires_at=entry.get("agent_key_expires_at"),
            expires_at_ms=entry.get("expires_at_ms"),
            extra=extra or None,
        )
        result = ce.to_dict()

        # Behavioral parity: keep only the fields credential_entry_t persists.
        filtered = {k: v for k, v in result.items() if k in _C_FIELDS}

        emit({"provider": provider, "out": json.dumps(filtered, separators=(",", ":"), ensure_ascii=False)})


if __name__ == "__main__":
    sys.exit(main())
