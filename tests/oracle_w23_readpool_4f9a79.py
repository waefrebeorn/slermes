#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/_state/readpool.py (gate
w23_readpool_4f9a79).

The module budgets descriptors for read connections.  Everything it cannot know
by itself is an OS seam, and every seam is scripted here:

  * ``_fd_soft_limit`` / ``_open_fd_count`` / ``time.monotonic`` /
    ``_fd_usage_cache`` -- the descriptor probe and its cache;
  * the ``resource`` module (or its absence) for the soft RLIMIT_NOFILE;
  * ``os.listdir`` over ``/proc/self/fd`` and ``/dev/fd``;
  * the live ``StoreHolder`` members of a path and the persistence of their
    permits;
  * the ``hermes_state`` logger, captured to compare the exact wording.

Everything else -- the decision ladder, the permit arithmetic, the rate limiter
and the canonicalisation of a db path -- is the live code.
"""
import errno
import importlib.util
import json
import sys
import types
import weakref
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "persistence" / "sqlite" / "_state" / "readpool.py"
GATE = "w23_readpool_4f9a79"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("readpool_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["readpool_live"] = live
_spec.loader.exec_module(live)

WANTED = {"consts", "fd_headroom", "fd_soft_limit", "fd_count", "budget_key",
          "acquire", "register", "note_exhausted"}


class Recorder:
    """Captures what the module logs at WARNING."""

    def __init__(self):
        self.msgs = []

    def warning(self, fmt, *args):
        self.msgs.append(fmt % args)

    def debug(self, *args, **kwargs):
        pass

    def info(self, *args, **kwargs):
        pass


class FakeHolder:
    """A StoreHolder with only what the budget asks of it."""

    def __init__(self, index, evictable, budget, site="", read_only=True, has_conn=True):
        self.index = index
        self.read_only = read_only
        self._conn = object() if has_conn else None
        self._creation_site = site
        self.db_path = "/db/state.db"
        self._evictable = evictable
        self._budget = budget
        self.evictions = []

    def _evict_one_idle_read_conn(self):
        if not self._evictable:
            return False
        self._evictable = False
        self.evictions.append(self.index)
        # closing an idle pooled connection releases BOTH permits
        self._budget.release()
        return True


def case_consts(_req):
    return {
        "pool_max": live._READ_POOL_MAX,
        "process_max": live._READ_POOL_PROCESS_MAX,
        "handles_warn": live._HANDLES_PER_PATH_WARN,
        "fd_reserve": live._FD_HEADROOM_RESERVE,
        "fd_cache_seconds": live._FD_USAGE_CACHE_SECONDS,
        "pool_warn_seconds": live._POOL_EXHAUSTION_WARN_INTERVAL_S,
    }


def case_fd_headroom(req):
    saved = (live._fd_soft_limit, live._open_fd_count, live._fd_usage_cache, live.time)
    probed = []

    def probe():
        probed.append(1)
        return req.get("probe")

    live._fd_soft_limit = lambda: req.get("soft")
    live._open_fd_count = probe
    live._fd_usage_cache = (req.get("stamp", 0.0), req.get("cached"))
    live.time = types.SimpleNamespace(monotonic=lambda: req.get("now", 0.0))
    try:
        ok = live._fd_headroom_ok()
        cached = live._fd_usage_cache[1]
    finally:
        (live._fd_soft_limit, live._open_fd_count, live._fd_usage_cache, live.time) = saved
    return {"ok": bool(ok), "cached": cached, "probed": bool(probed)}


def case_fd_soft_limit(req):
    saved = sys.modules.get("resource", "MISSING")
    try:
        if not req.get("module", True):
            sys.modules["resource"] = None  # `import resource` -> ImportError
        else:
            mod = types.ModuleType("resource")
            mod.RLIMIT_NOFILE = 7
            mod.RLIM_INFINITY = 9223372036854775807
            soft = mod.RLIM_INFINITY if req.get("infinity") else req.get("soft")
            mode = req.get("raise")

            def getrlimit(which, _soft=soft, _mode=mode):
                if _mode == "os":
                    raise OSError("no rlimit here")
                if _mode == "value":
                    raise ValueError("no rlimit here")
                return (_soft, _soft)

            if mode != "attr":
                mod.getrlimit = getrlimit
            sys.modules["resource"] = mod
        return {"soft": live._fd_soft_limit()}
    finally:
        if saved == "MISSING":
            sys.modules.pop("resource", None)
        else:
            sys.modules["resource"] = saved


class ListDirProbe:
    """os.listdir over the two fd directories, scripted per call."""

    name = "nt"

    def __init__(self, outcomes):
        self.outcomes = list(outcomes)
        self.calls = 0

    def listdir(self, path):
        index = self.calls
        self.calls += 1
        if index >= len(self.outcomes):
            raise OSError(errno.ENOENT, "no such directory")
        outcome = self.outcomes[index]
        if "error" in outcome:
            code = {"emfile": errno.EMFILE, "enfile": errno.ENFILE,
                    "enoent": errno.ENOENT, "eacces": errno.EACCES}[outcome["error"]]
            raise OSError(code, outcome["error"])
        return list(outcome.get("entries") or [])


def case_fd_count(req):
    saved = live.os
    live.os = ListDirProbe(req.get("dirs") or [])
    try:
        return {"count": live._open_fd_count()}
    finally:
        live.os = saved


def case_budget_key(req):
    return {"key": live._read_budget_key(req.get("path"))}


def case_acquire(req):
    budget = live._PathReadBudget()
    saved_fd = live._fd_headroom_ok
    saved_budgets = live._read_budgets
    saved_denied = live._read_open_denied_fd_headroom
    proc_sem = live._process_read_permits
    proc_before = proc_sem._value

    for _ in range(int(req.get("path_used", 0))):
        budget.permits.acquire(blocking=False)
    for _ in range(int(req.get("process_used", 0))):
        proc_sem.acquire(blocking=False)

    holders = [FakeHolder(i, bool(m.get("evictable")), budget)
               for i, m in enumerate(req.get("members") or [])]
    for holder in holders:
        budget.register(holder)

    live._fd_headroom_ok = lambda: bool(req.get("fd_ok", True))
    budgets = weakref.WeakValueDictionary()
    budgets["<case>"] = budget
    live._read_budgets = budgets
    requester_index = req.get("requester", -1)
    requester = holders[requester_index] if 0 <= requester_index < len(holders) else None
    results = []
    try:
        for op in req.get("ops") or []:
            if op.get("op") == "release":
                budget.release()
                results.append(True)
                continue
            results.append(bool(budget.acquire(requester)))
        evicted = sum(len(holder.evictions) for holder in holders)
        denied = live._read_open_denied_fd_headroom - saved_denied
        path_used = live._READ_POOL_MAX - budget.permits._value
        process_used = live._READ_POOL_PROCESS_MAX - live._process_read_permits._value
    finally:
        live._fd_headroom_ok = saved_fd
        live._read_budgets = saved_budgets
        live._read_open_denied_fd_headroom = saved_denied
        while proc_sem._value < proc_before:
            proc_sem.release()
    return {"results": results, "process_used": process_used, "path_used": path_used,
            "denied_fd": denied, "evicted": evicted}


def case_register(req):
    budget = live._PathReadBudget()
    saved = live.logger
    recorder = Recorder()
    live.logger = recorder
    holders = []
    try:
        for i, member in enumerate(req.get("members") or []):
            holder = FakeHolder(i, False, budget, site=member.get("site", ""),
                                read_only=bool(member.get("read_only")),
                                has_conn=bool(member.get("has_conn")))
            holders.append(holder)  # a real holder is owned by its caller, not the WeakSet
            holder.db_path = req.get("db_path", "/db/state.db")
            budget.register(holder)
    finally:
        live.logger = saved
    return {"warned": bool(recorder.msgs),
            "msg": recorder.msgs[0] if recorder.msgs else None}


def case_note_exhausted(req):
    saved_time = live.time
    saved_logger = live.logger
    saved_at = dict(live._pool_exhaustion_warned_at)
    saved_suppressed = dict(live._pool_exhaustion_suppressed)
    live._pool_exhaustion_warned_at.clear()
    live._pool_exhaustion_suppressed.clear()
    recorder = Recorder()
    live.logger = recorder
    warned, msgs = [], []
    try:
        for op in req.get("ops") or []:
            moment = op.get("t", 0.0)
            live.time = types.SimpleNamespace(monotonic=lambda v=moment: v)
            before = len(recorder.msgs)
            live.note_read_pool_exhausted(op.get("db"))
            if len(recorder.msgs) > before:
                warned.append(True)
                msgs.append(recorder.msgs[-1])
            else:
                warned.append(False)
                msgs.append(None)
    finally:
        live.time = saved_time
        live.logger = saved_logger
        live._pool_exhaustion_warned_at.clear()
        live._pool_exhaustion_warned_at.update(saved_at)
        live._pool_exhaustion_suppressed.clear()
        live._pool_exhaustion_suppressed.update(saved_suppressed)
    return {"warned": warned, "msgs": msgs}


HANDLERS = {
    "consts": case_consts,
    "fd_headroom": case_fd_headroom,
    "fd_soft_limit": case_fd_soft_limit,
    "fd_count": case_fd_count,
    "budget_key": case_budget_key,
    "acquire": case_acquire,
    "register": case_register,
    "note_exhausted": case_note_exhausted,
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
            py = HANDLERS[r["func"]](r)
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
