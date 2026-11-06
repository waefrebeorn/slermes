#!/usr/bin/env python3
"""Oracle for tools/file_operations_native.py::sha256_of.

The module imports only stdlib (contextlib, hashlib, os, secrets, stat, typing),
so we import the LIVE module and call the real sha256_of on the same paths the
C harness emitted. C out NULL must equal Python None.
"""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import tools.file_operations_native as FOP  # noqa: E402

# fresh shell path seed; the module keeps no state


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
        print("ORACLE file_operations_native: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        if c.get("tag") != "sha":
            continue
        total += 1
        want = FOP.sha256_of(c["path"])
        c_out = c["out"]
        if want != c_out:
            mism += 1
            print(f"MISMATCH {c.get('case')}: C={c_out} PY={want} path={c['path']}")
    print(f"ORACLE file_operations_native: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
