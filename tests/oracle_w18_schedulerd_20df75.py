#!/usr/bin/env python3
"""Differential oracle for cron/scheduler_detached_worker.py (gate w18_schedulerd_20df75).

AST-extracts the three LIVE helpers and drives them with recording fakes, then
compares the resulting trace/return to the C harness output.
"""
import ast
import json
import sys
import types
import __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "cron" / "scheduler_detached_worker.py"
_ANN = __future__.annotations.compiler_flag


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "sdw_live"}
    import concurrent.futures, subprocess, threading, contextlib
    g.update(dict(concurrent=concurrent, subprocess=subprocess,
                  threading=threading, contextlib=contextlib, Optional=object))
    wanted = {"_close_late_session_db_result", "defer_teardown_to_running_worker",
              "reap_terminal_worker_in_background"}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in wanted:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()


class Handle:
    def __init__(self, rec): self.rec = rec
    def close(self): self.rec.append("close")


class Future:
    def __init__(self, raises=False, has_handle=False, done=False, rec=None):
        self.raises, self.has_handle, self._done = raises, has_handle, done
        self.rec = rec if rec is not None else []
        self.cb = None
    def result(self):
        if self.raises: raise RuntimeError("late")
        return Handle(self.rec) if self.has_handle else None
    def done(self): return self._done
    def add_done_callback(self, cb): self.cb = cb


def run_case(r):
    f = r.get("func")
    if f == "close_late_result":
        fut = Future(raises=bool(r.get("result_raises")), has_handle=bool(r.get("has_handle")))
        G["_close_late_session_db_result"](fut)
        return bool(fut.rec)
    if f == "defer_teardown":
        rec = []
        fut = None if r.get("future_none") else Future(done=bool(r.get("future_done")), rec=rec)
        cron = types.ModuleType("cron"); cron.__path__ = []
        sched = types.ModuleType("cron.scheduler")
        sched._finalize_cron_session = lambda *a, **k: rec.append("finalize")
        sched._teardown_cron_agent = lambda *a, **k: rec.append("teardown")
        cron.scheduler = sched
        saved = (sys.modules.get("cron"), sys.modules.get("cron.scheduler"))
        sys.modules["cron"], sys.modules["cron.scheduler"] = cron, sched
        try:
            ret = G["defer_teardown_to_running_worker"](
                fut, bool(r.get("session_db_truthy")), object(), "job-id", "job-name", "cron-sid", workdir=None)
            added = fut is not None and fut.cb is not None
            trace = []
            if added and r.get("invoke_callback"):
                fut.cb(fut)
                trace = rec[:]
            return {"ret": bool(ret), "callback_added": bool(added), "trace": trace}
        finally:
            for name, val in (("cron", saved[0]), ("cron.scheduler", saved[1])):
                if val is None: sys.modules.pop(name, None)
                else: sys.modules[name] = val
    if f == "reap_background":
        rec = {}
        class Thread:
            def __init__(self, target=None, name=None, daemon=None):
                rec["name"] = name
                rec["daemon"] = daemon
                rec["wait_is_target"] = getattr(target, "__name__", "") == "wait"
            def start(self): rec["started"] = True
        class P:
            def __init__(self):
                if r.get("has_pid"): self.pid = r.get("pid", "?")
            def wait(self): rec["waited"] = True
        saved = G.get("threading")
        G["threading"] = types.SimpleNamespace(Thread=Thread)
        try:
            G["reap_terminal_worker_in_background"](P())
        finally:
            G["threading"] = saved
        return {"thread_name": rec.get("name"), "daemon": bool(rec.get("daemon")),
                "wait_is_target": bool(rec.get("wait_is_target")),
                "started": bool(rec.get("started"))}
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        try: r = json.loads(line)
        except ValueError: continue
        f = r.get("func")
        if f not in {"close_late_result", "defer_teardown", "reap_background"}: continue
        cases += 1
        try: py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


GATE = "w18_schedulerd_20df75"
if __name__ == "__main__":
    main()
