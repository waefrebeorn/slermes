#!/usr/bin/env python3
"""Differential oracle for gateway/run_inbound_unauthorized.py (gate w21_runinbound_92c923).

The live module is imported and every ported function runs for real:
`pairing_profile_arg`, `pairing_code_reply`, `pairing_rate_limited_reply`,
`unauthorized_owner_hint` (which itself calls the live
`neutralize_untrusted_inline_text`) and `UnauthorizedOwnerNotifier.first_time`
(the live OrderedDict). `_allowlist_env_for_platform` is the live function too;
it reads the platform registry's declarative `allowed_users_env` field, which is
the table the port carries.

Seams, named:
* `pairing_store.profile` -> the fixture's `profile` value (an attribute read of
  a live store handle).
* the notifier's `_seen` order -> the fixture's `state` (the in-process memory a
  restart would have kept); `first_time` then runs live.
`UnauthorizedOwnerNotifier.notify` (home-channel transports) is network and is
not modelled.
"""
import importlib.util
import json
import sys
from collections import OrderedDict
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "run_inbound_unauthorized.py"
GATE = "w21_runinbound_92c923"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("run_inbound_unauthorized_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["run_inbound_unauthorized_live"] = live
_spec.loader.exec_module(live)

from gateway.pairing import _allowlist_env_for_platform  # noqa: E402


class Store:
    """A pairing store handle: `profile` is the only attribute this port reads."""


def do_profile_arg(r):
    store = Store()
    if "profile" in r:
        store.profile = r["profile"]
    return live.pairing_profile_arg(store)


def do_owner_hint(r):
    return live.unauthorized_owner_hint(
        r.get("platform", ""), r.get("user_id", ""), r.get("user_name", ""),
        hermes_home=r.get("hermes_home", ""))


def do_first_time(r):
    notifier = live.UnauthorizedOwnerNotifier(max_seen=r.get("max_seen", 2048))
    notifier._seen = OrderedDict((tuple(pair), None) for pair in (r.get("state") or []))
    first = notifier.first_time(r.get("platform", ""), r.get("user_id"))
    return {"first": first, "order": [[p, u] for p, u in notifier._seen]}


def expected(r):
    f = r.get("func")
    if f == "profile_arg":
        return do_profile_arg(r)
    if f == "code_reply":
        return live.pairing_code_reply(r.get("platform", ""), r.get("code", ""),
                                       r.get("profile_arg", ""))
    if f == "rate_limited":
        return live.pairing_rate_limited_reply()
    if f == "owner_hint":
        return do_owner_hint(r)
    if f == "allowlist_env":
        return _allowlist_env_for_platform(r.get("platform", ""))
    if f == "first_time":
        return do_first_time(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"profile_arg", "code_reply", "rate_limited", "owner_hint",
              "allowlist_env", "first_time"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
