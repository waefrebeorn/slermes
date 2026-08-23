"""twin for jobs_claim_dispatch.

Traces the LIVE cron/jobs.py::claim_dispatch with load_jobs / save_jobs /
_jobs_lock monkeypatched to serve the argv-supplied job (REAL claim logic;
only the storage I/O is stubbed). Same rung order as the C harness, which
carries a traced copy of the REAL src/cron/jobs.c::cronjobs_claim_dispatch
decision ladder.

Input format "<job_id>\\t<job_json>".
"""
import os, sys, contextlib
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location("wc_live_cj", os.path.join(DEV, "cron", "jobs.py"))
_cj = ilu.module_from_spec(_spec)
sys.modules["wc_live_cj"] = _cj
_spec.loader.exec_module(_cj)

_SAVED = []


@contextlib.contextmanager
def _fake_lock():
    yield


def trace(job_id, job_json):
    global _SAVED
    import json as _json
    job = _json.loads(job_json)
    jobs = [job] if job is not None else []

    _cj.load_jobs = lambda *a, **k: [_json.loads(_json.dumps(j)) for j in jobs]
    _SAVED = []

    def _save(js, *a, **k):
        _SAVED.append([dict(j) for j in js])

    _cj.save_jobs = _save
    _cj._jobs_lock = _fake_lock

    c = Cup("claim_dispatch")

    found = any(j.get("id") == job_id for j in jobs)
    c.branch(found)                                            # B1
    result = _cj.claim_dispatch(job_id)

    if found:
        schedule = job.get("schedule") or {}
        once = schedule.get("kind") == "once"
        c.branch(once)                                         # B2
        if once:
            repeat = job.get("repeat")
            rep_ok = isinstance(repeat, dict) and bool(repeat)
            c.branch(rep_ok)                                   # B3
            if rep_ok:
                times = repeat.get("times")
                times_pos = isinstance(times, (int, float)) and \
                    not isinstance(times, bool) and times > 0
                c.branch(times_pos)                            # B4
                if times_pos:
                    completed = repeat.get("completed", 0)
                    over = completed >= times
                    c.branch(over)                             # B5

    # new completed count after a successful claim (from the saved store)
    new_completed = "-"
    if _SAVED:
        for j in _SAVED[0]:
            if j.get("id") == job_id:
                rep = j.get("repeat") or {}
                if isinstance(rep.get("completed"), (int, float)):
                    new_completed = str(int(rep["completed"]))
                break

    c.valstr(str(result))
    c.valstr(new_completed)
    return c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        job_id, _, job_json = raw.replace("\\t", "\t").partition("\t")
        cups.append(trace(job_id, job_json))
    return cups
