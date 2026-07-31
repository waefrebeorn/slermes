#!/usr/bin/env python3
import os
"""
sta_oracle_gateway_short_label.py — oracle for
t_port_gateway_short_label.c.

Recomputes each case from the LIVE hermes_cli/setup.py
(_gateway_platform_short_label) and emits one line per input (same shape as
the C harness). The runner diffs them byte-for-byte.
"""
import sys
import importlib.util


def _load():
    # Deterministic LIVE-Python resolution: prefer the canonical dev repo
    # (parent of slermes/) over any installed/stale copy on sys.path
    # (e.g. ~/.hermes/hermes-agent), which would manufacture false FAPs.
    _repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if _repo not in sys.path:
        sys.path.insert(0, _repo)
    for base in sys.path:
        cand = f"{base}/hermes_cli/setup.py"
        try:
            spec = importlib.util.spec_from_file_location("live_setup", cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    import hermes_cli.setup as mod  # type: ignore
    return mod


setup = _load()


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_gateway_short_label.py <labels.txt>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        for line in f:
            label = line.rstrip("\n").rstrip("\r")
            sys.stdout.write(setup._gateway_platform_short_label(label) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
