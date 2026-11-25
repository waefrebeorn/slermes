#!/usr/bin/env python3
"""Differential oracle for hermes_cli/sessions_repair_profiles.py — gate
w34_sessionsre_3cc180_p2 (verdicts from file evidence, move order, report).

The live module is imported as a package module and driven through the same
seams the p1 gate uses -- a RepairPlan built with ``__new__``, a ``_Session``
with pre-seeded store handles -- plus the file seams:

  * ``_scan_voice_modes`` / ``_scan_sessions_json`` read the file the fixture
    supplies the exact text of, so what is compared is what the parser and the
    predicate make of that text (a missing or unparsable file finds nothing);
  * ``_rekey_voice_mode_entry`` / ``_drop_sessions_json_entry`` run against a
    real temp file with ``utils.atomic_json_write`` stubbed to capture the
    rewritten object: the atomic write is the seam, the rewrite is the port;
  * ``_parents_first`` orders fixture payloads;
  * ``_MoveBatch.run`` runs over fake store handles (export/import/count/delete
    are seams) so the copy-before-delete, parents-first, lineage-awaiting
    outcome table is what is compared;
  * ``RepairPlan.apply`` reports over fixture findings whose fixes succeed,
    raise, or are absent, with snapshots stubbed.
"""
import json
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w34_sessionsre_3cc180_p2"
MIN_CASES = 10

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
# stub helper only; appended so it can never shadow a quarry package
sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

import hermes_cli.sessions_repair_profiles as live  # noqa: E402
import utils  # noqa: E402
import logging  # noqa: E402

# apply's failure path logs the exception it records; the traceback is noise on
# stderr, the recorded failure is what the report compares.
logging.disable(logging.CRITICAL)

live.Store.db_path = property(lambda self: Path(self.home) / "state.db")

WANTED = {"voice_modes", "sessions_json", "parents_first",
          "rekey_voice", "drop_entry", "move_batch", "apply"}


class _Handle:
    """Stand-in for a state.db's crossed-profile repair handle."""

    chats = {}

    def __init__(self, chats=None):
        self.chats = chats if isinstance(chats, dict) else {}

    def key_profiles_for_chat(self, platform, chat_id):
        return set(self.chats.get("%s:%s" % (platform, chat_id), []))

    def find_crossed_profile_sessions(self, owner):
        return {"mislabelled": [], "foreign": [], "crossed_parents": []}

    def find_profile_less_telegram_topic_rows(self):
        return []

    def list_gateway_routing_rows(self):
        return []


def _session(handles=None):
    session = live._Session.__new__(live._Session)
    session.read_only = True
    session._dbs = dict(handles or {})
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


def _home():
    return Path(tempfile.mkdtemp(prefix="sr3p2-"))


def _findings(plan):
    out = []
    for f in plan.findings:
        d = f.as_dict()
        d["repairable"] = f.repairable
        out.append(d)
    return out


def _write(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def voice_modes(r):
    prof = (r.get("store") or {}).get("profile") or "default"
    home = _home()
    store = live.Store(prof, home, routing=True)
    content = r.get("content")
    if isinstance(content, str):
        _write(home / "gateway_voice_mode.json", content)
    plan = _plan([store])
    handle = _Handle(chats=r.get("chats"))
    plan._scan_voice_modes(_session({store.db_path: handle}), store)
    return _findings(plan)


def sessions_json(r):
    prof = (r.get("store") or {}).get("profile") or "default"
    home = _home()
    store = live.Store(prof, home, routing=True)
    content = r.get("content")
    if isinstance(content, str):
        _write(home / "sessions" / "sessions.json", content)
    plan = _plan([store])
    plan.claimed = set(str(x) for x in (r.get("claimed") or [prof]))
    plan._scan_sessions_json(store)
    return _findings(plan)


def parents_first(r):
    payloads = [p for p in (r.get("payloads") or [])
                if isinstance(p, dict) and isinstance(p.get("session"), dict)]
    return [p["session"].get("id") for p in live._parents_first(payloads)]


def rekey_voice(r):
    rec = {}

    def _write_stub(path, data, mode=None):
        rec["data"] = dict(data)

    utils.atomic_json_write = _write_stub
    home = _home()
    path = home / "gateway_voice_mode.json"
    content = r.get("content")
    if isinstance(content, str):
        _write(path, content)
    ret = live._rekey_voice_mode_entry(path, r.get("key"), r.get("owner"))
    return {"result": ret, "data": rec.get("data")}


def drop_entry(r):
    rec = {}

    def _write_stub(path, data, mode=None):
        rec["data"] = dict(data)

    utils.atomic_json_write = _write_stub
    home = _home()
    path = home / "sessions" / "sessions.json"
    content = r.get("content")
    if isinstance(content, str):
        _write(path, content)
    ret = live._drop_sessions_json_entry(path, r.get("key"))
    return {"result": ret, "data": rec.get("data")}


def move_batch(r):
    from persistence import SessionLedgered  # the export-side ledger fence

    src = live.Store(r.get("src") or "work", _home())
    dst = live.Store(r.get("dst") or "default", _home())
    payloads = r.get("payloads") or {}
    export_errors = set(r.get("export_errors") or [])
    import_errors = set(r.get("import_errors") or [])
    outcome = r.get("import_outcome") or {}
    counts = r.get("message_counts") or {}
    del_errors = set(r.get("delete_errors") or [])
    del_result = r.get("delete_result") or {}

    class Source:
        def export_session_for_move(self, sid):
            if sid in export_errors:
                raise SessionLedgered("gained ledger rows since the scan")
            return payloads.get(sid)

        def delete_moved_session(self, sid):
            if sid in del_errors:
                raise RuntimeError("delete failed")
            return bool(del_result.get(sid, True))

    class Dest:
        def import_moved_session(self, payload, profile_name=None):
            sid = payload["session"]["id"]
            if sid in import_errors:
                raise RuntimeError("import failed")
            return outcome.get(sid, "copied")

        def count_messages_all(self, sid):
            return counts.get(sid, 10 ** 6)

    plan = _plan([src, dst])
    plan.by_profile = {src.profile: src, dst.profile: dst}
    batch = live._MoveBatch(src, dst)
    for sid in (r.get("ids") or []):
        batch.fix_for(sid)
    return batch.run(_session({src.db_path: Source(), dst.db_path: Dest()}))


def apply(r):
    stores = []
    for st in (r.get("stores") or []):
        if not isinstance(st, dict):
            continue
        home = _home()
        home.mkdir(parents=True, exist_ok=True)
        if st.get("db_exists"):
            (home / "state.db").write_text("snapshot-me", encoding="utf-8")
        stores.append(live.Store(st.get("profile") or "default", home))
    plan = _plan(stores)
    labels = r.get("snapshot_labels") or {}

    for f in (r.get("findings") or []):
        if not isinstance(f, dict):
            continue
        base = dict(kind=f.get("kind") or "", store=f.get("store") or "default",
                    subject=f.get("subject") or "", detail=f.get("detail") or "")
        fix = f.get("fix")
        if not isinstance(fix, dict):
            plan.findings.append(live.Finding(**base))
        elif isinstance(fix.get("error"), dict):
            msg = fix["error"].get("message") or ""

            def _bad(session, m=msg):
                raise RuntimeError(m)

            plan.findings.append(live.Finding(_fix=_bad, **base))
        else:
            counts = dict(fix.get("counts") or {})

            def _ok(session, c=counts):
                return dict(c)

            plan.findings.append(live.Finding(_fix=_ok, **base))
    return plan.apply(_session(), snapshot=lambda st: labels.get(st.profile))


def expected(r):
    f = r.get("func")
    if f == "voice_modes":
        return voice_modes(r)
    if f == "sessions_json":
        return sessions_json(r)
    if f == "parents_first":
        return parents_first(r)
    if f == "rekey_voice":
        return rekey_voice(r)
    if f == "drop_entry":
        return drop_entry(r)
    if f == "move_batch":
        return move_batch(r)
    if f == "apply":
        return apply(r)
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
