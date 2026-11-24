#!/usr/bin/env python3
"""Differential oracle for gateway/hosted_rooms_gateway_import.py (gate w26_hostedroom_f8aecd).

The import itself is two transactional steps against two SQLite stores, driven
once per start; the seams here are those stores, the root ``shared-state.db``
file and the ready-import thread.  Each becomes a fixed input:

  * ``source_for`` -- a real temp directory holding (or not) the sibling file,
    so the ``is_file()`` decision is exercised, not faked;
  * ``handoff_gateway_id`` -- the local authority id is supplied, the
    ``:gateway-store`` join is the live one;
  * ``import_after_ready`` -- authorities are stubs; the thread factory and the
    two lazy imports it makes (persistence/scope lookup) are recorded, so the
    plan is read off the threads the live function actually started.
"""
import json
import sys
import tempfile
import threading
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_hostedroom_f8aecd"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway import hosted_rooms_gateway_import as live  # noqa: E402
import gateway.hosted_rooms as hosted_rooms  # noqa: E402
import gateway.session_persistence as session_persistence  # noqa: E402
import persistence_assembly  # noqa: E402

WANTED = {"source_for", "handoff_id", "import_plan", "constants"}

_DIRS = {}


def _store_dir(has_sibling: bool) -> Path:
    key = bool(has_sibling)
    if key not in _DIRS:
        d = Path(tempfile.mkdtemp(prefix="w26_hostedroom_"))
        if key:
            (d / "shared-state.db").write_text("standalone", encoding="utf-8")
        _DIRS[key] = d
    return _DIRS[key]


def source_for(req):
    path = _store_dir(bool(req.get("sibling_exists"))) / str(req.get("target_name"))
    found = live.source_for(path)
    return None if found is None else found.name


def handoff_id(req):
    hosted_rooms.local_authority_gateway_id = lambda: req["local_id"]
    return live.handoff_gateway_id()


class _RecordedThread:
    created = []

    def __init__(self, target=None, name=None, daemon=None):
        self.target = target
        self.name = name
        self.daemon = daemon
        _RecordedThread.created.append(self)

    def start(self):
        pass


class _Service:
    def __init__(self, ready_started=False):
        self._ready_import_started = bool(ready_started)


class _Authority:
    def __init__(self, service, epoch=3):
        self.hosted_room_service = service
        self.epoch = epoch


def import_plan(req):
    authorities = []
    for entry in req.get("authorities") or []:
        service = _Service(entry.get("ready_started")) if entry.get("has_service") else None
        authorities.append(_Authority(service))
    _RecordedThread.created = []
    threading.Thread = _RecordedThread
    session_persistence.authority_persistence = lambda a: types.SimpleNamespace(scope="scope")
    persistence_assembly.store_location = lambda scope: ("C:/tmp/state.db", 7)
    live.import_after_ready(authorities)
    started = set()
    for thread in _RecordedThread.created:
        assert thread.name == "hosted-rooms-ready-import", thread.name
        authority = thread.target.__defaults__[3]
        started.add(id(authority))
    return [{"index": i, "start": id(a) in started} for i, a in enumerate(authorities)]


def constants():
    return {
        "source_name": live.SOURCE_NAME,
        "handoff_suffix": ":gateway-store",
        "thread_name": "hosted-rooms-ready-import",
        "warn_template": ("hosted rooms: could not import the standalone Group Chats from %s (%s); "
                          "will retry on the next start"),
        "info_template": "hosted rooms: imported %d standalone Group Chat(s) from %s",
    }


def expected(r):
    f = r.get("func")
    if f == "source_for":
        return source_for(r)
    if f == "handoff_id":
        return handoff_id(r)
    if f == "import_plan":
        return import_plan(r)
    if f == "constants":
        return constants()
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
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
