#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/webhook_delivery.py
(gate w22_webhookdel_ead437).

`validate_destination` runs live and raises for a refusal, so the compared value
is the row it returns or the refusal's reason. `snapshot_destination` needs the
adapter, so a stub adapter answers the profile scope and the home-channel
lookup (`gateway.config.Platform` is real). `route_digest` runs live too: the
stub adapter's `_routes` holds the fixture's routes and the chat id goes through
the real legacy/v2 decoder, so the digest exercises `json.dumps(sort_keys=True,
separators=(",", ":"))` + sha256 exactly as the module does.

Seams: the adapter (dynamic-route reload, route table, home channel), the
authority-store readback that `retained_destination` performs (that stays
upstream, unported), and `platform_registry` plugin platform names (empty here).
"""
import base64
import contextlib
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "platforms" / "webhook_delivery.py"
GATE = "w22_webhookdel_ead437"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.core.errors import AuthorityRefusal  # noqa: E402

_spec = importlib.util.spec_from_file_location("webhook_delivery_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["webhook_delivery_live"] = live
_spec.loader.exec_module(live)

WANTED = {"validate", "snapshot", "digest"}


class _Home:
    def __init__(self, chat_id):
        self.chat_id = chat_id


class _DeliveryConfig:
    def __init__(self, home):
        self.home = home

    def get_home_channel(self, platform):
        return None if self.home is None else _Home(self.home)


class _Adapter:
    def __init__(self, routes=None, home=None):
        self._routes = routes or {}
        self.home = home

    def _reload_dynamic_routes(self):
        return None

    @contextlib.contextmanager
    def _profile_scope(self, profile):
        yield self

    def _delivery_config(self, profile):
        return _DeliveryConfig(self.home)


def validate(r):
    try:
        return live.validate_destination(r.get("delivery"))
    except AuthorityRefusal as exc:
        return str(exc)


def snapshot(r):
    adapter = _Adapter(home=r.get("home"))
    try:
        return live.snapshot_destination(adapter, r.get("delivery"))
    except AuthorityRefusal as exc:
        return str(exc)


def digest(r):
    adapter = _Adapter(routes=r.get("routes") or {})
    try:
        return live.route_digest(adapter, r.get("chat_id"))
    except AuthorityRefusal as exc:
        return None if str(exc) == "not_found" else str(exc)


def expected(r):
    f = r.get("func")
    if f == "validate":
        return validate(r)
    if f == "snapshot":
        return snapshot(r)
    if f == "digest":
        return digest(r)
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
