#!/usr/bin/env python3
"""
sta_oracle_gateway_command_errors.py — Oracle for
hermes_cli/gateway_command_errors.py.

Reads the C harness's JSON lines from stdin, rebuilds the equivalent
exception, replays the LIVE Python module, and diffs the verb, the
str(exc) details text, and the guidance lines.
"""
import json
import subprocess
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import hermes_cli.gateway_command_errors as GCE  # noqa: E402


def main() -> int:
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
        print("ORACLE gateway_command_errors: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        if "lines" not in c:
            cmd = c.get("cmd")
            want = GCE._verb_for(subprocess.CalledProcessError(1, cmd if cmd else []))
            if c["verb"] != want:
                mism += 1
                print(f"MISMATCH {c['case']}.verb: C={c['verb']!r} PY={want!r}")
            continue

        kind = c["kind"]
        if kind == 1:
            want = GCE.explain_service_failure(GCE.SystemctlUnavailableError())
        elif kind == 2:
            exc = subprocess.CalledProcessError(c["rc"], c["argv"])
            # The C caller passes str(exc) as `details`.
            if c["details"] != str(exc):
                mism += 1
                print(f"MISMATCH {c['case']}.details:\n  C ={c['details']!r}\n  PY={str(exc)!r}")
            want = GCE.explain_service_failure(exc)
        else:
            want = GCE.explain_service_failure(RuntimeError("other"))

        got = c.get("lines")
        if got != want:
            mism += 1
            print(f"MISMATCH {c['case']}.lines:\n  C ={got}\n  PY={want}")

    print(f"ORACLE gateway_command_errors: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
