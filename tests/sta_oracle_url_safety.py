#!/usr/bin/env python3
"""
sta_oracle_url_safety.py — Python oracle for the PURE, deterministic SSRF-safety
helpers in tools/url_safety.py (mirrored by src/cli/port_tools_url_safety.c).

The oracle imports the REAL module and exercises the genuine functions:
  - normalize_url_for_request        (pure string transform)
  - _is_blocked_ip                   (pure IP classification)
  - is_always_blocked_url            (literal-IP + blocked-hostname paths;
                                      hostname-DNS paths are env-dependent and
                                      intentionally NOT oracled here)
  - is_safe_url is excluded (DNS resolution).

Output contract matches tests/t_port_url_safety.c: one JSON object per line,
sorted keys, ensure_ascii=False (raw UTF-8), compact separators.
"""

import json
import os
import sys

# Make the hermes-agent-dev tree importable.
_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.url_safety import (  # noqa: E402
    normalize_url_for_request,
    _is_blocked_ip,
    is_always_blocked_url,
)

import ipaddress  # noqa: E402


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def split_op(line):
    line = line.rstrip("\n")
    if not line.strip() or line.startswith("#"):
        return None
    op, _, rest = line.partition(" ")
    return op, rest


def main():
    for raw in sys.stdin:
        parsed = split_op(raw)
        if parsed is None:
            continue
        op, rest = parsed

        if op == "normalize":
            url = rest if rest else ""
            try:
                out = normalize_url_for_request(url)
                rc = 0 if url.lower().startswith(("http://", "https://")) else -1
            except Exception:
                out = url
                rc = -1
            emit({"op": "normalize", "in": url, "out": out, "rc": rc})

        elif op == "blocked_ip":
            ip = rest if rest else ""
            try:
                blocked = bool(_is_blocked_ip(ipaddress.ip_address(ip)))
            except ValueError:
                # Python ipaddress rejects unparseable; the C side blocks those.
                blocked = True
            emit({"op": "blocked_ip", "ip": ip, "blocked": blocked})

        elif op == "always_blocked":
            url = rest if rest else ""
            emit({"op": "always_blocked", "url": url,
                  "blocked": bool(is_always_blocked_url(url))})

        elif op == "ssrf_blocked_ip":
            # SSRF transport layer: same predicate as _is_blocked_ip, applied to
            # the resolved connect-time address (C: ssrf_is_blocked_ip).
            ip = rest if rest else ""
            try:
                blocked = bool(_is_blocked_ip(ipaddress.ip_address(ip)))
            except ValueError:
                blocked = True
            emit({"op": "ssrf_blocked_ip", "ip": ip, "blocked": blocked})

        elif op == "connect_scheme":
            # _safe_connect_scheme with an empty origin map: https iff port 443.
            from tools.url_safety import _safe_connect_scheme
            port = int(rest)
            emit({"op": "connect_scheme", "port": port,
                  "scheme": _safe_connect_scheme("example.com", port, {})})

        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})

    return 0


if __name__ == "__main__":
    sys.exit(main())
