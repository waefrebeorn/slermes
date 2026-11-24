#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_runtime_tickets.py
(gate w26_gatewayrun_1eda26).

Two seams are fixed inputs while the live code runs:

  * the profile registry -- ``validate_profile_name`` / ``profile_exists`` /
    ``get_profile_dir`` are answered from the fixture's
    ``{"existing": {name: home}, "invalid": [...]}`` table, so the live
    ``_profile_home`` still strips, validates and resolves;
  * ``mint`` -- returns the fixture's ticket or raises the real
    ``TicketUnavailable`` with the fixture's reason/detail.

``_request_lines`` reads a real BytesIO, and ``_reply`` is the live validator.
"""
import io
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_gatewayrun_1eda26"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import gateway_runtime_tickets as live  # noqa: E402
from hermes_cli import profiles as live_profiles  # noqa: E402
from hermes_cli.gateway_runtime_attach import TicketUnavailable  # noqa: E402

WANTED = {"request_lines", "reply", "serve", "constants"}


class _Profiles:
    def __init__(self, spec):
        spec = spec if isinstance(spec, dict) else {}
        self.existing = spec.get("existing") if isinstance(spec.get("existing"), dict) else {}
        self.invalid = set(spec.get("invalid") or [])

    def __enter__(self):
        self.saved = (live_profiles.validate_profile_name,
                      live_profiles.get_profile_dir,
                      live_profiles.profile_exists)
        existing, invalid = self.existing, self.invalid

        def validate(name):
            if name in invalid:
                raise ValueError("invalid profile name: %r" % (name,))

        def get_profile_dir(name):
            return Path(existing[name])

        def profile_exists(name):
            return name in existing

        live_profiles.validate_profile_name = validate
        live_profiles.get_profile_dir = get_profile_dir
        live_profiles.profile_exists = profile_exists
        return self

    def __exit__(self, *exc):
        (live_profiles.validate_profile_name,
         live_profiles.get_profile_dir,
         live_profiles.profile_exists) = self.saved
        return False


def _mint(req):
    spec = req.get("mint")
    spec = spec if isinstance(spec, dict) else {}

    def mint(purpose, profile_home):
        reason = spec.get("raise")
        if reason:
            raise TicketUnavailable(reason, spec.get("detail"))
        return spec.get("ticket", "TICKET")

    return mint


def _wire(req):
    wire = req.get("wire")
    return wire.encode("utf-8") if isinstance(wire, str) else b""


def request_lines(req):
    out = []
    for line in live._request_lines(io.BytesIO(_wire(req))):
        out.append(None if line is None else line.decode("utf-8"))
    return out


def reply(req):
    raw = req.get("line")
    line = None if raw is None else str(raw).encode("utf-8")
    with _Profiles(req.get("profiles")):
        return live._reply(line, _mint(req))


def serve(req):
    out = []
    mint = _mint(req)
    with _Profiles(req.get("profiles")):
        for line in live._request_lines(io.BytesIO(_wire(req))):
            out.append(live._reply(line, mint))
    return out


def constants():
    from gateway.runtime_bootstrap import TicketStore
    return {"protocol": live.TICKETS_PROTOCOL,
            "purposes": list(live.TICKET_PURPOSES),
            "max_request_line_bytes": live.MAX_REQUEST_LINE_BYTES,
            "profile_not_served": live.PROFILE_NOT_SERVED,
            "ticket_ttl_seconds": TicketStore.TTL_SECONDS}


DISPATCH = {
    "request_lines": request_lines,
    "reply": reply,
    "serve": serve,
    "constants": lambda req: constants(),
}


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
            py = DISPATCH[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
