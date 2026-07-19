#!/usr/bin/env python3
"""
sta_oracle_account_usage.py — oracle for t_port_account_usage.c.

Recomputes each case from the LIVE agent/account_usage.py and emits one JSON
line per case (same shape as the C harness). The runner diffs them byte-for-byte.
"""
import json
import math
import sys
import importlib.util


def _load():
    for base in sys.path:
        cand = f"{base}/agent/account_usage.py"
        try:
            spec = importlib.util.spec_from_file_location("live_account_usage", cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    import agent.account_usage as mod  # type: ignore
    return mod


au = _load()


def emit_title_case(c):
    value = c.get("value")
    out = au._title_case_slug(value)
    return {"fn": "title_case_slug", "out": out if out is not None else ""}


def emit_fmt_usd(c):
    d = float(c.get("value", 0.0))
    return {"fn": "fmt_usd", "out": au._fmt_usd(d)}


def emit_is_finite(c):
    v = c.get("value", 0.0)
    fin = isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v)
    return {"fn": "is_finite_num", "out": bool(fin)}


DISPATCH = {
    "title_case_slug": emit_title_case,
    "fmt_usd": emit_fmt_usd,
    "is_finite_num": emit_is_finite,
}


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_account_usage.py <cases.json>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        fn = DISPATCH.get(op)
        out = fn(c) if fn else {"fn": op}
        sys.stdout.write(json.dumps(out, ensure_ascii=False, separators=(",", ":")) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
