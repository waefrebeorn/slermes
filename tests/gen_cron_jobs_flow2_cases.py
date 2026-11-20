#!/usr/bin/env python3
"""Generate tests/fixtures/cron_jobs_flow2.jsonl (FLOW2 oracle fixtures).

Each case is {"func", "in"}; the harness echoes "in" and the oracle recomputes
the live reference from it. Scripted seam results live in in.io: a single
{"ret"/"raise"/"state"} object is reused for every call; a bare LIST is a
per-call queue. Run: python tests/gen_cron_jobs_flow2_cases.py [--check].
"""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
FIXTURE = HERE / "fixtures" / "cron_jobs_flow2.jsonl"


def raise_(t, msg="boom", **kw):
    d = {"type": t, "message": msg}
    d.update(kw)
    return {"raise": d}


def ret(v):
    return {"ret": v}


def case(func, **inp):
    return {"func": func, "in": inp}


CASES = []

# ── normalize_job_updates ──────────────────────────────────────────────────
CASES += [
    case("normalize_job_updates", job={}, updates={}),
    case("normalize_job_updates", job={}, updates={"workdir": "/raw"},
         io={"norm_workdir": ret("/norm")}),
    case("normalize_job_updates", job={},
         updates={"workdir": "/raw", "timezone": "UTC"},
         io={"norm_workdir": ret("/norm"),
             "norm_timezone": raise_("ValueError", "bad tz")}),
    case("normalize_job_updates", job={"repeat": {"completed": 4}},
         updates={"repeat": {"times": "3"}},
         io={"normalize_repeat_value": ret(3)}),
    case("normalize_job_updates", job={"repeat": {"completed": 7}},
         updates={"repeat": 2},
         io={"normalize_repeat_value": ret(2)}),
    case("normalize_job_updates", job={}, updates={"repeat": {"times": 5, "completed": 9}},
         io={"normalize_repeat_value": ret(5)}),
    case("normalize_job_updates", job={}, updates={"repeat": 1},
         io={"normalize_repeat_value": raise_("TypeError", "bad repeat")}),
    case("normalize_job_updates", job={}, updates={"monitor_url": " http://x ", "catch_up": 1},
         io={"norm_monitor_url": ret("http://x"), "norm_catch_up": ret(True)}),
]

# ── rederive_repeat ────────────────────────────────────────────────────────
CASES += [
    case("rederive_repeat", job={}, updates={}),
    case("rederive_repeat", job={"schedule": {"kind": "once"}},
         updates={"schedule": "*/5 * * * *", "repeat": {"times": 1}}),
    case("rederive_repeat", job={"schedule": {"kind": "once"}},
         updates={"schedule": "0 9 * * *"},
         io={"parse_schedule": ret({"kind": "once"})}),
    case("rederive_repeat", job={"schedule": {"kind": "once"}, "repeat": {"times": 1}},
         updates={"schedule": "*/5 * * * *"},
         io={"parse_schedule": ret({"kind": "interval"})}),
    case("rederive_repeat", job={"schedule": {"kind": "interval"}, "repeat": {}},
         updates={"schedule": "0 9 * * *"},
         io={"parse_schedule": ret({"kind": "once"})}),
    case("rederive_repeat", job={"schedule": {"kind": "cron"}, "repeat": {"times": 4}},
         updates={"schedule": ret({"kind": "interval"})}),
    case("rederive_repeat", job={"schedule": {"kind": "once"}},
         updates={"schedule": "bad"},
         io={"parse_schedule": raise_("ValueError", "bad expr")}),
]

# ── apply_schedule_update ──────────────────────────────────────────────────
CASES += [
    case("apply_schedule_update", updated={"schedule": {"kind": "once"}},
         updates={"schedule_display": "manual"}),
    case("apply_schedule_update", updated={"schedule": "0 9 * * *", "state": "paused"},
         updates={}, io={"parse_schedule": ret({"kind": "cron", "display": "0 9 * * *"})}),
    case("apply_schedule_update", updated={"schedule": {"kind": "cron"}, "name": "job1"},
         updates={}, io={"_next_run_or_reject_past_oneshot": ret("2026-01-02T00:00:00+00:00")}),
    case("apply_schedule_update",
         updated={"schedule": {"kind": "cron", "display": "in-sched"}, "schedule_display": "in-updated"},
         updates={}, io={"_next_run_or_reject_past_oneshot": ret("2026-01-02T00:00:00+00:00")}),
    case("apply_schedule_update", updated={"schedule": "bad", "state": "scheduled"},
         updates={}, io={"parse_schedule": raise_("ValueError", "bad expr")}),
    case("apply_schedule_update", updated={"schedule": {"kind": "once"}},
         updates={}, io={"_next_run_or_reject_past_oneshot": raise_("ValueError", "past")}),
]

# ── fill_missing_next_run ──────────────────────────────────────────────────
CASES += [
    case("fill_missing_next_run", updated={"enabled": False}),
    case("fill_missing_next_run", updated={"state": "paused"}),
    case("fill_missing_next_run", updated={"next_run_at": "2026-01-01T00:00:00+00:00"}),
    case("fill_missing_next_run", updated={"schedule": {"kind": "cron"}},
         io={"compute_next_run": ret("2026-01-02T00:00:00+00:00")}),
    case("fill_missing_next_run", updated={"schedule": {"kind": "once", "run_at": "2020-01-01"}},
         io={"compute_next_run": ret(None)}),
    case("fill_missing_next_run", updated={"schedule": {"kind": "interval"}},
         io={"compute_next_run": ret(None)}),
    case("fill_missing_next_run", updated={"schedule": {"kind": "cron"}},
         io={"compute_next_run": raise_("RuntimeError", "no croniter")}),
]

# ── cron_config_number ─────────────────────────────────────────────────────
CASES += [
    case("cron_config_number", key="grace", default=30,
         io={"load_config": ret({"cron": {"grace": 5}}), "cast": ret(5)}),
    case("cron_config_number", key="grace", default=30,
         io={"load_config": ret({"cron": {}}), "cast": ret(30)}),
    case("cron_config_number", key="grace", default=30,
         io={"load_config": raise_("ValueError", "bad config"), "cast": ret(30)}),
    case("cron_config_number", key="grace", default=30,
         io={"load_config": raise_("SystemExit", "exit"), "cast": ret(30)}),
    case("cron_config_number", key="grace", default=30,
         io={"load_config": ret({"cron": {"grace": 5}}),
             "cast": [raise_("ValueError", "bad cast"), ret(30)]}),
    case("cron_config_number", key="grace", default=30,
         io={"load_config": ret({"cron": {"grace": 5}}),
             "cast": raise_("KeyboardInterrupt", "stop")}),
    case("cron_config_number", key="grace", default=30,
         io={"load_config": ret("not-a-dict"), "cast": ret(30)}),
]

# ── record_run_outcome ─────────────────────────────────────────────────────
CASES += [
    case("record_run_outcome", job={"id": "j1"}, success=True, now="2026-01-02T00:00:00+00:00",
         io={"sanitize_persisted_error": ret(None)}),
    case("record_run_outcome",
         job={"id": "j1", "manual_run_at": "x", "manual_run_prompt": "p", "preflight_alerted": True},
         success=True, now="2026-01-02T00:00:00+00:00",
         io={"sanitize_persisted_error": ret(None)}),
    case("record_run_outcome", job={"id": "j1"}, success=False, error="boom",
         now="2026-01-02T00:00:00+00:00",
         io={"sanitize_persisted_error": [ret("clean"), ret("clean2"), ret("clean3")]}),
    case("record_run_outcome", job={"id": "j1"}, success=False, error="boom", status="cancelled",
         now="2026-01-02T00:00:00+00:00", io={"sanitize_persisted_error": ret("d")}),
    case("record_run_outcome", job={"id": "j1"}, success=False, delivery_error="  fail  ",
         now="2026-01-02T00:00:00+00:00",
         io={"sanitize_persisted_error": [ret(None), ret(None), ret("fail")]}),
    case("record_run_outcome", job={"id": "j1", "run_claim": {"owner": "x"}, "pending_slot": 1},
         success=True, status="ok", now="2026-01-02T00:00:00+00:00",
         io={"sanitize_persisted_error": ret(None)}),
    case("record_run_outcome", job={"id": "j1"}, success=False, error="boom",
         now="2026-01-02T00:00:00+00:00",
         io={"sanitize_persisted_error": raise_("RuntimeError", "sanitize down")}),
]

# ── advance_after_run ──────────────────────────────────────────────────────
CASES += [
    case("advance_after_run", job={"schedule": {"kind": "cron"}}, now="2026-01-02T00:00:00+00:00",
         io={"compute_next_run": ret("2026-01-03T00:00:00+00:00")}),
    case("advance_after_run",
         job={"schedule": {"kind": "once"}, "repeat": {"times": 2, "completed": 2}},
         now="2026-01-02T00:00:00+00:00", io={"_complete_job_record": ret(None)}),
    case("advance_after_run",
         job={"schedule": {"kind": "interval"}, "repeat": {"times": 3, "completed": 2}},
         now="2026-01-02T00:00:00+00:00", io={"_complete_job_record": ret(None)}),
    case("advance_after_run", job={"schedule": {"kind": "cron"}}, now="2026-01-02T00:00:00+00:00",
         io={"compute_next_run": ret(None), "logger.error": ret(None)}),
    case("advance_after_run", job={"schedule": {"kind": "once"}}, now="2026-01-02T00:00:00+00:00",
         io={"compute_next_run": ret(None), "_complete_job_record": ret(None)}),
    case("advance_after_run",
         job={"schedule": {"kind": "once"}, "repeat": {"times": 2, "completed": 1}},
         now="2026-01-02T00:00:00+00:00", ladder_rung=True,
         io={"compute_next_run": ret("2026-01-03T00:00:00+00:00")}),
    case("advance_after_run", job={"schedule": {"kind": "cron"}}, now="2026-01-02T00:00:00+00:00",
         io={"compute_next_run": raise_("RuntimeError", "no croniter")}),
]

# ── under_fire_fence ───────────────────────────────────────────────────────
CASES += [
    case("under_fire_fence", job_id="j1",
         io={"fire_lock.enter": ret(True), "fn": ret("ok"), "fire_lock.exit": ret(None)}),
    case("under_fire_fence", job_id="j1",
         io={"fire_lock.enter": ret(False), "fire_lock.exit": ret(None)}),
    case("under_fire_fence", job_id="j1", io={"fire_lock.enter": raise_("OSError", "lock")}),
    case("under_fire_fence", job_id="j1",
         io={"fire_lock.enter": ret(True), "fn": raise_("ValueError", "fn fail"),
             "fire_lock.exit": ret(None)}),
    case("under_fire_fence", job_id="j1",
         io={"fire_lock.enter": ret(True), "fn": ret("ok"),
             "fire_lock.exit": raise_("RuntimeError", "exit fail")}),
    case("under_fire_fence", job_id="j1",
         io={"fire_lock.enter": ret(True), "fn": ret([1, 2]), "fire_lock.exit": ret(None)}),
]


def write():
    FIXTURE.parent.mkdir(parents=True, exist_ok=True)
    with FIXTURE.open("w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    return counts


def check():
    import importlib.util
    spec = importlib.util.spec_from_file_location("ocj2", HERE / "oracle_cron_jobs_flow2.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    errs = 0
    for c in CASES:
        try:
            mod.run_case(c["func"], c["in"])
        except Exception as e:
            errs += 1
            print(f"PYERR {c['func']}: {type(e).__name__}: {e}")
    print(f"GEN-CHECK cron_jobs_flow2: {len(CASES)} cases, {errs} errors")
    return 0 if errs == 0 else 1


if __name__ == "__main__":
    counts = write()
    print("wrote", FIXTURE, len(CASES), "cases")
    print("per-func:", json.dumps(counts, sort_keys=True))
    if "--check" in sys.argv:
        sys.exit(check())
