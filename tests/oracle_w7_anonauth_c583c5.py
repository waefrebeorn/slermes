#!/usr/bin/env python3
"""Differential oracle for w7_anonauth_c583c5 (hermes_cli/anon_auth.py)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/hermes_cli/anon_auth.py"
spec = importlib.util.spec_from_file_location("w7_anonauth_c583c5_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w7_anonauth_c583c5_live"] = live
spec.loader.exec_module(live)

BASE_HOSTS = frozenset({"welcome-api.nousresearch.com"})


def patch_hosts(extra_hosts):
    extras = frozenset(p.strip().lower() for p in (extra_hosts or "").split(",") if p.strip())
    live.welcome_hosts = lambda: BASE_HOSTS | extras if extras else BASE_HOSTS


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "friendly_wait":
            py = live.friendly_wait(r.get("seconds"))
        elif func == "is_guest":
            py = live.is_guest_state(r.get("state"))
        elif func == "parse_refusal":
            py = live.parse_welcome_refusal(r.get("body"))
        elif func == "failure_copy":
            retry = r.get("retry_after", "")
            py = live.anon_failure_copy(r.get("code", ""), retry_after=(retry or None))
        elif func == "welcome_host":
            patch_hosts(r.get("extra_hosts", ""))
            py = live.route_is_welcome_host(r.get("base_url", ""))
        elif func == "pin_model":
            patch_hosts(r.get("extra_hosts", ""))
            py = live.pin_model_for_route(r.get("provider", ""), r.get("base_url", ""),
                                          r.get("model", ""))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} {r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} {r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w7_anonauth_c583c5: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
