#!/usr/bin/env python3
"""Oracle for hermes_cli/anon_auth.py — the free-tier policy cluster.

The harness (t_port_anon_auth.c) emits one JSON line per case with a "fn"
discriminator. String results come back as {"str": ...} so an empty string stays
distinguishable from a missing value.
"""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

from hermes_cli import anon_auth as _aa               # noqa: E402
from hermes_cli.anon_auth import (                    # noqa: E402
    anon_failure_copy, free_inference_active, friendly_wait,
    guest_without_free_tier, is_guest_state, pin_model_for_route,
    route_can_serve_model, route_is_welcome_host, welcome_hosts,
)

GUEST_MODEL = "nous/welcome"


def main() -> int:
    total = mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("fn")
        i = c.get("in") or {}
        ga = bool(i.get("guest_allowed", True))
        state = i.get("state")
        # The live functions read these two module globals; pin them to the
        # case's values so the comparison is about the routing logic.
        _aa.guest_allowed = lambda: ga
        _aa.current_nous_state = lambda: state
        try:
            if fn == "friendly_wait":
                want = friendly_wait(i.get("seconds"))
            elif fn == "is_guest_state":
                want = is_guest_state(state)
            elif fn == "welcome_hosts":
                want = sorted(welcome_hosts())
            elif fn == "route_is_welcome_host":
                want = route_is_welcome_host(i.get("base_url"))
            elif fn == "route_can_serve_model":
                want = route_can_serve_model(i["provider"], i["base_url"],
                                             i["model"])
            elif fn == "pin_model_for_route":
                want = pin_model_for_route(i["provider"], i["base_url"],
                                           i["model"])
            elif fn == "free_inference_active":
                want = free_inference_active(state)
            elif fn == "guest_without_free_tier":
                want = guest_without_free_tier(state)
            elif fn == "has_free_tier_account":
                want = _aa.has_free_tier_account()
            elif fn == "trial_model":
                want = _aa.trial_model()
            elif fn == "failure_copy":
                want = anon_failure_copy(i["code"],
                                         retry_after=i.get("retry_after"))
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"MISMATCH {c.get('case')} ({fn}): python raised {exc!r}")
            mism += 1
            continue
        raw = c["out"]
        got = raw["str"] if isinstance(raw, dict) and set(raw) == {"str"} else raw
        if got != want:
            mism += 1
            print(f"MISMATCH {c.get('case')} ({fn}): C={str(c['out'])[:200]} "
                  f"PY={want}")
    if total == 0:
        print("ORACLE anon_auth: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE anon_auth: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
