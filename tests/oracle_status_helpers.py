#!/usr/bin/env python3
"""Oracle for hermes_cli/status_auth.py:_format_iso_timestamp (port_status_helpers.c).

The nearby status.py PoP annotations for _configured_model_label /
_effective_provider_label are not covered by the harness (it only drives the
timestamp fn, include-self). This oracle verifies the one function the harness
emits. _format_iso_timestamp converts ISO timestamps to the LOCAL timezone and
renders through hermes_time.safe_strftime(%Y-%m-%d %H:%M:%S %Z) — so live and C
both run on this box and must agree on the local zone conversion; any mismatch is
a real divergence in parsing/formatting, not a tz artifact.
"""
import ast
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

AUTH = "C:/Users/eman5/hermes-agent/hermes_cli/status_auth.py"


def main() -> int:
    # seed hermes_time import + datetime/UTC, then ast-extract the live fn
    import datetime as dt
    import re as _re
    import time
    sys.path.insert(0, "C:/Users/eman5/hermes-agent")

    # repar surrogate helper is internal to hermes_time; we call the real one
    sys.path.insert(0, "C:/Users/eman5/hermes-agent")
    import hermes_time as ht

    src = open(AUTH, encoding="utf-8").read()
    # live does `from datetime import datetime, timezone, UTC` — `datetime` here is
    # the CLASS, not the module (seeding the module made .fromisoformat raise and
    # the live fn echo its input — a false parity gap).
    ns = {"datetime": dt.datetime, "UTC": dt.timezone.utc, "safe_strftime": ht.safe_strftime,
          "Any": object, "Optional": object}
    body = []
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "_format_iso_timestamp":
            body.append(n)
    exec(compile(ast.Module(body=body, type_ignores=[]), AUTH, "exec"), ns)
    fmt = ns["_format_iso_timestamp"]

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            import json
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        want = fmt(c.get("in"))
        got = c.get("out")
        if want != got:
            mism += 1
            print(f"MISMATCH in={c.get('in')!r} C={got!r} PY={want!r}")
    if total == 0:
        print("ORACLE status_helpers: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE status_helpers: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
