#!/usr/bin/env python3
"""Differential oracle for hermes_cli/sessions_repair_profiles.py — gate
w34_sessionsre_3cc180_p1 (the store scan: sessions, topics, routing).

The live module is imported as a package module and its scan methods are called
on a ``RepairPlan`` built with ``__new__``, whose ``_Session`` has its store
handles pre-seeded with fake repair objects: every store handle is a seam, so
the whole scan runs over fixture values and no state.db is ever opened.
``Store.db_path`` (the ``persistence_assembly.store_location`` seam) is answered
from the store's own home.

  * ``_ledgered_lineages`` — the lineages a move must leave where they are;
  * ``_scan_sessions``      — foreign / mislabelled / crossed-parent verdicts,
                               including the ``--legacy-main`` three-way and the
                               rows a pending move already owns;
  * ``_scan_topics``        — profile-less telegram topic bindings;
  * ``_scan_routing``       — routing rows in the wrong store, or for a
                               namespace no live profile claims.
"""
import json
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w34_sessionsre_3cc180_p1"
MIN_CASES = 10

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
# stub helper only; appended so it can never shadow a quarry package
sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

import hermes_cli.sessions_repair_profiles as live  # noqa: E402

# The store_location seam: a store's db path follows its home.
live.Store.db_path = property(lambda self: Path(self.home) / "state.db")

WANTED = {"ledgered_lineages", "scan_sessions", "scan_topics", "scan_routing"}


class _Handle:
    """Stand-in for a state.db's crossed-profile repair handle."""

    chats = {}

    def __init__(self, crossed=None, topics=None, routing=None):
        self.crossed = crossed if isinstance(crossed, dict) else {
            "mislabelled": [], "foreign": [], "crossed_parents": []}
        self.topics = topics if isinstance(topics, list) else []
        self.routing = routing if isinstance(routing, list) else []

    def find_crossed_profile_sessions(self, owner):
        return self.crossed

    def find_profile_less_telegram_topic_rows(self):
        return self.topics

    def list_gateway_routing_rows(self):
        return self.routing

    def key_profiles_for_chat(self, platform, chat_id):
        return set(self.chats.get("%s:%s" % (platform, chat_id), []))


def _session(handles):
    """A _Session whose stores are already open: no import, no file."""
    session = live._Session.__new__(live._Session)
    session.read_only = True
    session._dbs = dict(handles)
    return session


def _plan(stores, legacy_main="report", multiplexes=False):
    plan = live.RepairPlan.__new__(live.RepairPlan)
    plan.stores = stores
    plan.by_profile = {s.profile: s for s in stores}
    plan.claimed = set(plan.by_profile)
    plan.routing_store = next((s for s in stores if s.routing), None)
    plan.legacy_main = legacy_main
    plan.multiplexes = multiplexes
    plan.findings = []
    plan._moves = {}
    return plan


def _stores(profiles, routing_profile=None):
    return [live.Store(p, Path(tempfile.mkdtemp(prefix="sr3-")), routing=(p == routing_profile))
            for p in profiles]


def _findings(plan):
    out = []
    for f in plan.findings:
        d = f.as_dict()
        d["repairable"] = f.repairable
        out.append(d)
    return out


def ledgered_lineages(r):
    rows = r.get("foreign")
    return live._ledgered_lineages([x for x in (rows or []) if isinstance(x, dict)])


def scan_sessions(r):
    st = r.get("store") or {}
    prof = st.get("profile") or "default"
    claimed = [str(x) for x in (r.get("claimed") or [])]
    routing = r.get("routing_store") or None
    assert prof in claimed, "fixture: the scanned store is one of the plan's stores"
    assert routing is None or routing in claimed, "fixture: the routing store is enumerated"
    stores = _stores(claimed, routing)
    plan = _plan(stores, str(r.get("legacy_main") or "report"), bool(r.get("multiplexes")))
    store = next(s for s in stores if s.profile == prof)
    handle = _Handle(crossed=r.get("crossed"))
    plan._scan_sessions(_session({s.db_path: handle for s in stores}), store)
    return _findings(plan)


def scan_topics(r):
    prof = (r.get("store") or {}).get("profile") or "default"
    stores = _stores([prof])
    plan = _plan(stores)
    handle = _Handle(topics=r.get("rows"))
    plan._scan_topics(_session({stores[0].db_path: handle}), stores[0])
    return _findings(plan)


def scan_routing(r):
    st = r.get("store") or {}
    prof = st.get("profile") or "default"
    claimed = [str(x) for x in (r.get("claimed") or [prof])]
    routing = r.get("routing_store") or None
    want_routing_store = bool(st.get("routing"))
    assert prof in claimed, "fixture: the scanned store is one of the plan's stores"
    if want_routing_store:
        assert routing == prof, "fixture: the routing index's store is the scanned store"
    stores = _stores(claimed, routing)
    plan = _plan(stores, "report", bool(r.get("multiplexes")))
    store = next(s for s in stores if s.profile == prof)
    plan._scan_routing(_session({s.db_path: _Handle(routing=r.get("rows")) for s in stores}), store)
    return _findings(plan)


def expected(r):
    f = r.get("func")
    if f == "ledgered_lineages":
        return ledgered_lineages(r)
    if f == "scan_sessions":
        return scan_sessions(r)
    if f == "scan_topics":
        return scan_topics(r)
    if f == "scan_routing":
        return scan_routing(r)
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n  c =%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
            continue
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    if cases < MIN_CASES:
        print("ORACLE %s: only %d cases (< %d) — gate not exercised"
              % (GATE, cases, MIN_CASES), file=sys.stderr)
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
