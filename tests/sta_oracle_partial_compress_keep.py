#!/usr/bin/env python3
"""
sta_oracle_partial_compress_keep.py — Oracle for
hermes_cli/partial_compress.py:_coerce_keep.

Reads C harness JSON lines (case/token/ret) from stdin, recomputes each case
against LIVE Python and compares the returned ints.
"""
import sys
import os
import json

sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import hermes_cli.partial_compress as PC  # noqa: E402


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

    # Recompute LIVE Python for each token; need to map case->token.
    # We reconstruct tokens from the harness-emitted token field.
    py = {}
    for case, obj in c_cases.items():
        tok = obj.get("token")
        token_val = None if tok is None else tok  # JSON string or null
        py[case] = PC._coerce_keep(token_val)

    order = list(c_cases.keys())
    mism = 0
    for case in order:
        c_ret = c_cases[case].get("ret")
        p_ret = py[case]
        if c_ret == p_ret:
            print(f"ok [{case}]")
        else:
            mism += 1
            print(f"MISMATCH [{case}]  C={c_ret} PY={p_ret}")
    print(f"\nRESULT: {len(order)-mism}/{len(order)} match, {mism} mismatch")
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
