#!/usr/bin/env python3
"""
sta_oracle_vault_login_js.py — Oracle for the two JS builders in
agent/vault_login_classifier.py.

The C templates are generated from the LIVE Python module by
scripts/gen_vault_login_js.py, so this compares the finished expressions
byte-for-byte: any drift in either the template or the json.dumps-compatible
quoting shows up as a length+prefix diff.
"""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import agent.vault_login_classifier as VLC  # noqa: E402


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
        print("ORACLE vault_login_js: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        if c["kind"] == "inspect":
            want = VLC.build_inspection_js(c["nonce"])
        else:
            fills = c["fills"]
            if isinstance(fills, str):
                fills = json.loads(fills)
            want = VLC.build_fill_js(fills, c["origin"], c["nonce"])
        got = c["out"]
        if got != want:
            mism += 1
            print(f"MISMATCH {c['case']}: len C={len(got or '')} PY={len(want)}\n"
                  f"  C ={(got or '')[:160]!r}\n  PY={want[:160]!r}")
            for i, (a, b) in enumerate(zip(got or "", want)):
                if a != b:
                    print(f"  first diff at {i}: C={a!r} PY={b!r} "
                          f"ctx C={(got or '')[max(0, i - 40):i + 40]!r}")
                    break

    print(f"ORACLE vault_login_js: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
