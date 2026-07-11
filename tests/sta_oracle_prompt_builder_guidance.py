#!/usr/bin/env python3
"""
sta_oracle_prompt_builder_guidance.py — Oracle for
agent/prompt_builder.py:computer_use_guidance.

Reads C harness JSON lines (case/len/digest) from stdin, replays LIVE Python
computer_use_guidance for each case, computes the same FNV-1a digest, and
compares.
"""
import sys
import json

sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import agent.prompt_builder as PB  # noqa: E402


def fnv1a(s: str) -> int:
    h = 1469598103934665603
    for b in s.encode("utf-8"):
        h ^= b
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def main():
    data = sys.stdin.read()
    c_cases = {}
    for ln in data.splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            obj = json.loads(ln)
        except Exception:
            continue
        if "case" in obj:
            c_cases[obj["case"]] = obj

    # map JSON case token -> platform_name passed to Python
    plat = {
        "darwin": "darwin",
        "win32": "win32",
        "linux": "linux",
        "cygwin": "cygwin",
        "null": None,
    }

    mism = 0
    for case in c_cases:
        py_text = PB.computer_use_guidance(plat.get(case))
        py_digest = fnv1a(py_text)
        # C emits raw UTF-8 bytes; compare byte length, not code-point count.
        py_len = len(py_text.encode("utf-8"))
        c_digest = c_cases[case].get("digest", 0)
        c_len = c_cases[case].get("len", 0)
        if c_digest == py_digest and c_len == py_len:
            print(f"ok [{case}]")
        else:
            mism += 1
            print(f"MISMATCH [{case}]  C(len={c_len},d={c_digest}) "
                  f"PY(len={py_len},d={py_digest})")
    print(f"\nRESULT: {len(c_cases)-mism}/{len(c_cases)} match, {mism} mismatch")
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
