#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_runtime_multiplex.py
(gate w25_gatewayrun_b1c386).

The live module is imported from the Python quarry and its decisions are driven
with their filesystem/config seams fixed by the request:

  * ``hermes_constants.named_profile_home`` -> ``named`` (is this home a named
    profile under a root?);
  * ``hermes_cli.gateway_multiplex_mode.host_serves_named_profile`` -> ``verdict``
    (true / false / null, the boot policy answer);
  * ``gateway.status.read_runtime_status`` -> ``{"served_profiles": served}``.

``multiplexer_serves_home`` and ``implied_host_root`` then answer whether the
root's multiplexer serves the home; the port compares ``is not None``.  The two
supporting pure helpers are called straight off their live modules:
``profiles.normalize_profile_name`` and ``gateway_multiplex_mode.settled_explicit_flag``.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_gatewayrun_b1c386"

if not os.environ.get("USERPROFILE") and not os.environ.get("HOMEPATH"):
    os.environ["USERPROFILE"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import gateway.status as gstatus  # noqa: E402
import hermes_cli.gateway_multiplex_mode as gmm  # noqa: E402
import hermes_cli.profiles as profiles  # noqa: E402
import hermes_constants as hc  # noqa: E402
from hermes_cli import gateway_runtime_multiplex as live  # noqa: E402

WANTED = {"normalize_name", "settled_flag", "serves_home", "implied_serves"}

PROFILES_DIR = Path("C:/w25gate/profiles")
REAL_NAMED = hc.named_profile_home
REAL_VERDICT = gmm.host_serves_named_profile
REAL_STATUS = gstatus.read_runtime_status


def _install(named, verdict, served):
    hc.named_profile_home = lambda home: (Path(home) if named else None)
    gmm.host_serves_named_profile = lambda root, home: verdict
    gstatus.read_runtime_status = lambda path: (
        {"served_profiles": served} if served is not None else {})


def _restore():
    hc.named_profile_home = REAL_NAMED
    gmm.host_serves_named_profile = REAL_VERDICT
    gstatus.read_runtime_status = REAL_STATUS


def normalize_name(req):
    return profiles.normalize_profile_name(req.get("name"))


def settled_flag(req):
    return gmm.settled_explicit_flag(req.get("value"))


def serves_home(req):
    home = PROFILES_DIR / req.get("home_name")
    _install(req.get("named"), req.get("verdict"), req.get("served"))
    try:
        return live.multiplexer_serves_home(home) is not None
    finally:
        _restore()


def implied_serves(req):
    home = PROFILES_DIR / "bob"
    _install(req.get("named"), req.get("verdict"), None)
    try:
        return live.implied_host_root(home) is not None
    finally:
        _restore()


def expected(r):
    f = r.get("func")
    if f == "normalize_name":
        return normalize_name(r)
    if f == "settled_flag":
        return settled_flag(r)
    if f == "serves_home":
        return serves_home(r)
    if f == "implied_serves":
        return implied_serves(r)
    raise AssertionError("unknown func %r" % (f,))


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
            py = {"__error__": type(exc).__name__}
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
