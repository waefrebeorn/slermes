#!/usr/bin/env python3
"""Differential oracle for plugins/config_backends/remote/backend.py (gate p2).

Only the install/reconcile derivations run: a fetched body becomes a document
and its schema version (R6, oldest writer), locks are decoded, the diff base and
version tuple are read, lock lookup runs, and the write advisories are built.
Seams: the clock (fixture `now`), the plane URL/instance (fixture), the managed
file (fixture), `DEFAULT_CONFIG` and `SUPPORT_FLOOR_VERSION` (fixture).
"""
import json
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_backend_010a82_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.config_backends.remote import backend as live  # noqa: E402
import hermes_cli.config_defaults as config_defaults  # noqa: E402
import hermes_cli.config_migrations as config_migrations  # noqa: E402

WANTED = {
    "install", "diff_base", "version_of", "cache_version", "exists", "locked",
    "first_change", "inherited_notes", "migration_check_input", "describe",
}


def bare():
    return live.RemoteBackend.__new__(live.RemoteBackend)


def writer_json(w):
    if w is live._UNKNOWN:
        return {"known": False, "value": None}
    return {"known": True, "value": w}


def writer_from(req):
    spec = req.get("profile_writer") or {"kind": "unknown"}
    kind = spec.get("kind")
    if kind == "null":
        return None
    if kind == "int":
        return spec.get("value")
    return live._UNKNOWN


def with_latest(latest, fn):
    old = config_defaults.DEFAULT_CONFIG
    config_defaults.DEFAULT_CONFIG = {"_config_version": latest}
    try:
        return fn()
    finally:
        config_defaults.DEFAULT_CONFIG = old


def install(req):
    st = SimpleNamespace(home=None, profile="p", gen=0, installed=0, base=None,
                         postprocessed=False, server_config={}, doc={}, etag="",
                         profile_version=0, profile_writer=None, other_writers=[],
                         locks=[], provenance={}, changed_ns=0, fetched_at=0.0,
                         last_error=None, next_poll=0.0)
    latest = req.get("latest", 1)
    body = req.get("body")
    notes = with_latest(latest, lambda: bare()._install(st, body, req.get("etag")))
    return {
        "doc": st.doc,
        "doc_version": st.doc.get("_config_version"),
        "profile_version": st.profile_version,
        "profile_writer": writer_json(st.profile_writer),
        "other_writers": list(st.other_writers),
        "locks": [[list(p), lv] for p, lv in st.locks],
        "etag": st.etag,
        "provenance": st.provenance,
        "notes": notes,
    }


def diff_base(req):
    state = req.get("state") or {}
    st = SimpleNamespace(base=state.get("base"), server_config=state.get("server_config"))
    return bare()._diff_base(st)


def version_of(req):
    state = req.get("state") or {}
    st = SimpleNamespace(changed_ns=state.get("changed_ns", 0), etag=state.get("etag", ""),
                         gen=state.get("gen", 0))
    private = state.get("private")
    token = None
    if private:
        token = live._PRIVATE.set(SimpleNamespace(st=st, doc={}, serial=private["serial"],
                                                  gen=private["gen"]))
    try:
        return list(live.RemoteBackend._version_of(st))
    finally:
        if token is not None:
            live._PRIVATE.reset(token)


def locked(req):
    locks = [(tuple(p), lv) for p, lv in req.get("locks") or []]

    class Fake(live.RemoteBackend):
        def _state(self, home):
            return SimpleNamespace(locks=locks)

    level = Fake().locked(None, req.get("dotted", ""))
    return {"found": level is not None, "level": level}


def first_change(req):
    return list(live._first_change(tuple(req.get("path") or []), req.get("sent"),
                                   bool(req.get("found")), req.get("got"))
                or []) or None


def inherited_notes(req):
    st = SimpleNamespace(server_config=req.get("server_config") or {})
    unsets = [tuple(p) for p in req.get("unsets") or []]
    return live.RemoteBackend._inherited_notes(st, unsets)


def migration_check_input(req):
    st = SimpleNamespace(server_config=req.get("server_config") or {},
                         other_writers=list(req.get("other_writers") or []),
                         profile_writer=writer_from(req))
    sets = {tuple(p): v for p, v in req.get("sets") or []}
    unsets = [tuple(p) for p in req.get("unsets") or []]
    body = {"writerConfigVersion": req.get("latest", 1)} if req.get("has_writer") else {}
    latest = req.get("latest", 1)
    floor = req.get("floor", 1)

    old_floor = config_migrations.SUPPORT_FLOOR_VERSION
    config_migrations.SUPPORT_FLOOR_VERSION = floor
    try:
        out = with_latest(latest, lambda: bare()._migration_check_input(st, body, sets, unsets))
    finally:
        config_migrations.SUPPORT_FLOOR_VERSION = old_floor
    if out is None:
        return None
    stored, after = out
    return {"stored": stored, "after": after}


def describe(req):
    state = req.get("state")

    class Fake(live.RemoteBackend):
        @staticmethod
        def _key(home):
            return "k"

    fake = Fake()
    if state is not None:
        state = dict(state)
        state["locks"] = [None] * int(state.get("locks_count") or 0)
        fake._states["k"] = SimpleNamespace(**state)
    old_url, old_inst = live.client.shown_url, live.client.instance_id
    old_time = live.time.time
    live.client.shown_url = lambda: req.get("url", "")
    live.client.instance_id = lambda: req.get("instance_id", "")
    live.time.time = lambda: req.get("now", 0)
    try:
        return fake.describe(None)
    finally:
        live.client.shown_url = old_url
        live.client.instance_id = old_inst
        live.time.time = old_time


def expected(r):
    f = r.get("func")
    if f == "install":
        return install(r)
    if f == "diff_base":
        return diff_base(r)
    if f == "version_of":
        return version_of(r)
    if f == "cache_version":
        return bare().cache_version(None, r.get("version"))
    if f == "exists":
        return bare().exists(None)
    if f == "locked":
        return locked(r)
    if f == "first_change":
        return first_change(r)
    if f == "inherited_notes":
        return inherited_notes(r)
    if f == "migration_check_input":
        return migration_check_input(r)
    if f == "describe":
        return describe(r)
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
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    if cases == 0:
        print("ORACLE %s: no cases" % GATE, file=sys.stderr)
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
