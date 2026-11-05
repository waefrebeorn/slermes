#!/usr/bin/env python3
"""
sta_oracle_input_sanitize.py — Oracle for hermes_cli/input_sanitize.py.

Reads the C harness's JSON lines from stdin, replays the LIVE Python module
over each `in` value, and compares all three helpers byte-for-byte.
"""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import hermes_cli.input_sanitize as IS  # noqa: E402


def main() -> int:
    data = sys.stdin.read()
    cases = []
    for line in data.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass

    if not cases:
        print("ORACLE input_sanitize: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        text = c.get("in")
        want_strip = IS.strip_leaked_bracketed_paste_wrappers(text)
        want_coll = IS.collapse_repeated_input_artifacts(text)
        want_san = IS.sanitize_user_prompt_text(text)
        for key, want in (("strip", want_strip), ("collapse", want_coll),
                          ("sanitize", want_san)):
            got = c.get(key)
            if got != want:
                mism += 1
                print(f"MISMATCH {c['case']}.{key}: C={got!r} PY={want!r}")

    print(f"ORACLE input_sanitize: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
