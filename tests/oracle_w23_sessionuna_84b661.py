#!/usr/bin/env python3
"""Differential oracle for gateway/session_unattended_unknown.py (gate
w23_sessionuna_84b661).

``is_unattended`` reads one admission row: the principal it came from and the
payload/native markers a webhook, a Kanban dispatch or an automation envelope
stamped.  It is pure, so it runs on the fixture row directly.

The two settlements are decided by the live functions over seams the process
owns:

  * ``gateway.session_authority.admission_row`` maps a stored record to the row
    dict (here the record already is that dict);
  * the store's ``admissions`` handle answers ``list_unknown`` / ``list`` and
    records every ``resolve_unknown`` it is handed, refusing the rows the
    fixture marks ``refuse``;
  * ``gateway.session_ingress_media.release_admission_media`` records the media
    release;
  * the drain's authority records ``resolve_unknown_as_owner``.

The notice text is the live ``agent.turn_failure_copy`` constant, so the C
port's copy of it is checked too.  What is compared is what the live module
would really hand the store: the settle count, the exact calls in settle order,
and which rows were refused.
"""
import asyncio
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "session_unattended_unknown.py"
GATE = "w23_sessionuna_84b661"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent.turn_failure_copy import AUTOMATION_INTERRUPTED_NOTICE  # noqa: E402
from persistence.core.errors import AuthorityRefusal  # noqa: E402

_spec = importlib.util.spec_from_file_location("unattended_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["unattended_live"] = live
_spec.loader.exec_module(live)

WANTED = {"is_unattended", "unattended_unknown", "settle_recovered", "settle_drain",
          "cron_principal"}

_NOTICE = AUTOMATION_INTERRUPTED_NOTICE


def rows_of(req, key):
    rows = req.get(key)
    return rows if isinstance(rows, list) else []


class FakeAdmissions:
    def __init__(self, rows):
        self.rows = list(rows)
        self.refused_ids = {r.get("admission_id") for r in self.rows
                            if isinstance(r, dict) and r.get("refuse")}
        self.resolved = []
        self.refused = []
        self.session_ids = []

    def list_unknown(self):
        return list(self.rows)

    def list(self, session_id=None):
        self.session_ids.append(session_id)
        return list(self.rows)

    def resolve_unknown(self, *, epoch, admission_id, generation, notice):
        if admission_id in self.refused_ids:
            self.refused.append(admission_id)
            raise AuthorityRefusal("stale_generation")
        self.resolved.append({"admission_id": admission_id, "generation": generation,
                              "epoch": epoch, "notice": notice})


class FakeStore:
    def __init__(self, rows):
        self.admissions = FakeAdmissions(rows)


class FakeAuthority:
    def __init__(self, rows):
        self.admissions = FakeAdmissions(rows)
        self.resolved = []

    async def resolve_unknown_as_owner(self, ref, admission_id, generation, *, notice):
        if admission_id in self.admissions.refused_ids:
            self.admissions.refused.append(admission_id)
            raise AuthorityRefusal("stale_generation")
        self.resolved.append({"admission_id": admission_id, "generation": generation,
                              "notice": notice})


class _Stub:
    def __init__(self, name, **attrs):
        self.name = name
        self.attrs = attrs
        self.saved = None

    def __enter__(self):
        self.saved = sys.modules.get(self.name)
        module = types.ModuleType(self.name)
        for key, value in self.attrs.items():
            setattr(module, key, value)
        sys.modules[self.name] = module
        return module

    def __exit__(self, *exc):
        if self.saved is None:
            sys.modules.pop(self.name, None)
        else:
            sys.modules[self.name] = self.saved
        return False


def is_unattended(req):
    row = req.get("row") if isinstance(req.get("row"), dict) else {}
    return live.is_unattended(row)


def unattended_unknown(req):
    records = rows_of(req, "records")
    with _Stub("gateway.session_authority", admission_row=lambda record: record):
        kept = live._unattended_unknown(records)
    return [row.get("admission_id") for row in kept]


def settle_recovered(req):
    rows = rows_of(req, "rows")
    store = FakeStore(rows)
    released = []

    def release(_store, admission_id):
        released.append(admission_id)

    with _Stub("gateway.session_ingress_media", release_admission_media=release), \
            _Stub("gateway.session_authority", admission_row=lambda record: record):
        settled = live.settle_recovered_unattended(store, req.get("epoch"))
    return {"settled": settled, "resolved": store.admissions.resolved,
            "released": released, "refused": store.admissions.refused}


def settle_drain(req):
    rows = rows_of(req, "rows")
    authority = FakeAuthority(rows)
    ref = types.SimpleNamespace(session_id=req.get("session_id"))

    def persistence(_authority):
        return types.SimpleNamespace(admissions=_authority.admissions)

    with _Stub("gateway.session_persistence", authority_persistence=persistence), \
            _Stub("gateway.session_authority", admission_row=lambda record: record):
        settled = asyncio.run(live.settle_unattended_unknown(authority, ref))
    return {"settled": settled, "resolved": authority.resolved,
            "refused": authority.admissions.refused,
            "session_ids": authority.admissions.session_ids}


def expected(r):
    f = r.get("func")
    if f == "is_unattended":
        return is_unattended(r)
    if f == "unattended_unknown":
        return unattended_unknown(r)
    if f == "settle_recovered":
        return settle_recovered(r)
    if f == "settle_drain":
        return settle_drain(r)
    if f == "cron_principal":
        return live.CRON_PRINCIPAL_ID
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
