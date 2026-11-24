#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_multiplex_served.py (gate w26_gatewaymul_37308a).

The live module is imported as-is; its two I/O seams are what get fixed per case:
``gateway.status.live_gateway_pid_for_home`` (the verified-live proof) and
``gateway.status.read_runtime_status`` (the ``gateway_state.json`` reader).  Both are
imported lazily *inside* the upstream functions, so patching the module attributes
routes every call through the case's values.  The record itself is passed through as
``runtime``, and what is compared is exactly what the CLI/dashboard surfaces show.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_gatewaymul_37308a"

# The import chain resolves the default hermes root through the user's home; the
# gate must not depend on the caller's shell, so pin it to the repo parent.
os.environ["USERPROFILE"] = str(REPO.parent)
os.environ["HOME"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import gateway.status as gstatus  # noqa: E402
from hermes_cli import gateway_multiplex_served as live  # noqa: E402

WANTED = {"served_profiles", "secondaries", "unserved_platforms", "ingress_urls",
          "format_lines", "constants"}

CASE = {"live": None, "runtime": None}


def patched_pid_for_home(home):
    return CASE["live"]


def patched_read_runtime_status(path):
    return CASE["runtime"]


gstatus.live_gateway_pid_for_home = patched_pid_for_home
gstatus.read_runtime_status = patched_read_runtime_status


def live_pid(req):
    raw = req.get("live_pid")
    if isinstance(raw, bool):
        return 1234 if raw else None
    if isinstance(raw, (int, float)) and raw != 0:
        return int(raw)
    return None


def prime(req):
    CASE["live"] = live_pid(req)
    runtime = req.get("runtime")
    CASE["runtime"] = runtime if isinstance(runtime, dict) else runtime


def expected(r):
    prime(r)
    f = r.get("func")
    if f == "served_profiles":
        return live.recorded_served_profiles()
    if f == "secondaries":
        return live.multiplexer_served_secondaries()
    if f == "unserved_platforms":
        return live.served_profile_unserved_platforms(r.get("profile") or "")
    if f == "ingress_urls":
        return live.served_profile_ingress_urls(r.get("profile") or None)
    if f == "format_lines":
        indent = r.get("indent")
        urls = r.get("urls")
        return live.format_ingress_url_lines(urls, "  " if indent is None else indent)
    if f == "constants":
        from gateway import config as gcfg
        return {
            "served_profile_mirror_platforms": sorted(gcfg.SERVED_PROFILE_MIRROR_PLATFORMS),
            "shared_listener_mirror_platforms": sorted(gcfg.SHARED_LISTENER_MIRROR_PLATFORMS),
            "listener_mirror_paths": dict(gcfg.SHARED_LISTENER_MIRROR_PATHS),
        }
    raise AssertionError("unknown func %r" % (f,))


def same(py, c):
    """Python-data equality with the C side's value semantics."""
    if isinstance(py, bool) or isinstance(c, bool):
        return py is not None and c is not None and bool(py) == bool(c) and \
            (isinstance(py, (bool, int, float)) and isinstance(c, (bool, int, float)))
    if isinstance(py, (int, float)) and isinstance(c, (int, float)):
        return float(py) == float(c)
    if isinstance(py, dict) and isinstance(c, dict):
        return set(py) == set(c) and all(same(py[k], c[k]) for k in py)
    if isinstance(py, list) and isinstance(c, list):
        return len(py) == len(c) and all(same(a, b) for a, b in zip(py, c))
    return py == c


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if not same(py, got):
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
