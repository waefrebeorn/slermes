#!/usr/bin/env python3
"""sta_oracle_terminal_breadcrumbs_pure.py — oracle twin for terminal_breadcrumbs_pure.c."""
import json
import os
import sys
import importlib.util


def _load_module():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(os.path.dirname(here)))
    cand = os.path.join(repo, "hermes_cli", "terminal_breadcrumbs.py")
    spec = importlib.util.spec_from_file_location("live_tbr", cand)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    path = sys.argv[1]
    with open(path, encoding="utf-8") as f:
        spec = json.load(f)
    mod = _load_module()
    out = []
    for case in spec.get("ops", []):
        op = case.get("op")
        if op == "sanitize":
            out.append({"op": op, "out": mod._SANITIZE_RE.sub("-", case.get("raw", "").strip().strip("/"))[:120]})
        elif op == "is_stale":
            ts = case.get("ts")
            now = case.get("now", 1_000_000_000_000.0)
            stale = (now - ts) > mod._STALE_AFTER_SECONDS if isinstance(ts, (int, float)) else True
            out.append({"op": op, "out": bool(stale)})
    for line in out:
        sys.stdout.write(json.dumps(line, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
