#!/usr/bin/env python3
"""Generate tests/fixtures/cron_jobs_flow.jsonl for the cron_jobs_flow oracle.

Each case is {"func", "in"}; the harness echoes "in" and the oracle recomputes
the live reference from it. Scripted seam results live in in.io: a single
{"ret"/"raise"/"state"} object is reused for every call; a bare LIST is a
per-call queue. List-valued RETURNS are wrapped {"ret": [...]} so they are not
consumed as queues. Run: python tests/gen_cron_jobs_flow_cases.py [--check]
(--check drives the oracle's PYTHON half over every case standalone).
"""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
FIXTURE = HERE / "fixtures" / "cron_jobs_flow.jsonl"

STORE = {"ret": {"cron_dir": "/h/cron", "jobs_file": "/h/cron/jobs.json",
                 "output_dir": "/h/cron/output"}}
OK = {"ret": None}
TRUE = {"ret": True}
FALSE = {"ret": False}


def raise_(t, msg="boom", **kw):
    d = {"type": t, "message": msg}
    d.update(kw)
    return {"raise": d}


def case(func, **inp):
    return {"func": func, "in": inp}


def store_io(**extra):
    io = {"_current_cron_store": STORE}
    io.update(extra)
    return io


CASES = []

# ── self_removal_delivery_allowed ─────────────────────────────────────────
M = {"job_id": "j1", "removed": True}
CASES += [
    case("self_removal_delivery_allowed", job_id="j1"),
    case("self_removal_delivery_allowed", job_id="j1",
         state={"self_removal_marker": {"job_id": "j2", "removed": True}}),
    case("self_removal_delivery_allowed", job_id="j1",
         state={"self_removal_marker": {"job_id": "j1", "removed": False}}),
    case("self_removal_delivery_allowed", job_id="j1",
         state={"self_removal_marker": M},
         io={"load_jobs": {"ret": [{"id": "j1"}, {"id": "j2"}]}}),
    case("self_removal_delivery_allowed", job_id="j1",
         state={"self_removal_marker": M}, io={"load_jobs": {"ret": []}}),
    case("self_removal_delivery_allowed", job_id="j1",
         state={"self_removal_marker": M},
         io={"load_jobs": {"ret": [{"id": "j2"}, {"id": "j3"}]}}),
    case("self_removal_delivery_allowed", job_id="j1",
         state={"self_removal_marker": M},
         io={"load_jobs": raise_("RuntimeError", "store down")}),
    case("self_removal_delivery_allowed", job_id="j1",
         state={"self_removal_marker": M}, io={"load_jobs": {"ret": ["x"]}}),
    case("self_removal_delivery_allowed", job_id="1",
         state={"self_removal_marker": {"job_id": "1", "removed": True}},
         io={"load_jobs": {"ret": [{"id": 1}]}}),
]

# ── acquire_flock ─────────────────────────────────────────────────────────
CASES += [
    case("acquire_flock", lock_fd=7, timeout=30,
         io={"time.monotonic": {"ret": 0}, "fcntl.flock": TRUE}),
    case("acquire_flock", lock_fd=7, timeout=30,
         io={"time.monotonic": [{"ret": 0}, {"ret": 0.05}],
             "fcntl.flock": [raise_("OSError", "busy"), TRUE],
             "time.sleep": OK}),
    case("acquire_flock", lock_fd=7, timeout=30,
         io={"time.monotonic": [{"ret": 0}, {"ret": 31}],
             "fcntl.flock": raise_("OSError", "busy")}),
    case("acquire_flock", lock_fd=7, timeout=30,
         io={"time.monotonic": {"ret": 0},
             "fcntl.flock": raise_("ValueError", "bad fd")}),
    case("acquire_flock", lock_fd=7, timeout=30, state={"platform": "msvcrt"},
         io={"msvcrt.locking": TRUE}),
    case("acquire_flock", lock_fd=7, timeout=30, state={"platform": "msvcrt"},
         io={"msvcrt.locking": raise_("OSError", "locked")}),
    case("acquire_flock", lock_fd=7, timeout=30, state={"platform": "none"}),
    case("acquire_flock", lock_fd=7, timeout=30,
         io={"time.monotonic": [{"ret": 0}, {"ret": 0.05}],
             "fcntl.flock": raise_("OSError", "busy"),
             "time.sleep": raise_("RuntimeError", "no sleep")}),
]

# ── release_flock ─────────────────────────────────────────────────────────
CASES += [
    case("release_flock", lock_fd=7, io={"fcntl.flock": OK, "close": OK}),
    case("release_flock", lock_fd=7,
         io={"fcntl.flock": raise_("OSError", "unlck"), "close": OK}),
    case("release_flock", lock_fd=7,
         io={"fcntl.flock": raise_("ValueError", "bad"), "close": OK}),
    case("release_flock", lock_fd=7, state={"platform": "msvcrt"},
         io={"msvcrt.locking": OK, "close": OK}),
    case("release_flock", lock_fd=7, state={"platform": "msvcrt"},
         io={"msvcrt.locking": raise_("OSError", "x"), "close": OK}),
    case("release_flock", lock_fd=7, state={"platform": "none"}, io={"close": OK}),
    case("release_flock", lock_fd=7,
         io={"fcntl.flock": OK, "close": raise_("OSError", "close fail")}),
    case("release_flock", lock_fd=7,
         io={"fcntl.flock": raise_("ValueError", "inflight"),
             "close": raise_("RuntimeError", "replaces")}),
]

# ── ensure_cron_dir ───────────────────────────────────────────────────────
CASES += [
    case("ensure_cron_dir", cron_dir="/h/profiles/p1/cron",
         io={"_is_named_profile_path": TRUE, "mkdir": OK}),
    case("ensure_cron_dir", cron_dir="/h/cron",
         io={"_is_named_profile_path": FALSE, "mkdir": OK}),
    case("ensure_cron_dir", cron_dir="/h/cron",
         io={"_is_named_profile_path": FALSE, "mkdir": raise_("OSError", "ro")}),
    case("ensure_cron_dir", cron_dir="/h/cron",
         io={"_is_named_profile_path": raise_("RuntimeError", "resolve")}),
    case("ensure_cron_dir", cron_dir="/h/cron",
         io={"_is_named_profile_path": OK, "mkdir": OK}),
]

# ── cron_schedule ─────────────────────────────────────────────────────────
CS = dict(expr="0 9 * * *", display="0 9 * * *",
          missing_croniter="Cron expressions require 'croniter' package.",
          invalid_label="cron expression")
CASES += [
    case("cron_schedule", **CS, io={"_ensure_croniter": TRUE, "croniter": TRUE}),
    case("cron_schedule", **CS, io={"_ensure_croniter": FALSE}),
    case("cron_schedule", **CS, io={"_ensure_croniter": OK}),
    case("cron_schedule", **CS,
         io={"_ensure_croniter": TRUE, "croniter": raise_("ValueError", "bad expr")}),
    case("cron_schedule", **CS,
         io={"_ensure_croniter": TRUE, "croniter": raise_("KeyError", "dow")}),
    case("cron_schedule", **CS, io={"_ensure_croniter": raise_("RuntimeError", "probe")}),
]

# ── parse_aware ───────────────────────────────────────────────────────────
ISO = "2026-01-01T00:00:00+00:00"
CASES += [
    case("parse_aware", value=ISO,
         io={"datetime.fromisoformat": {"ret": ISO},
             "_ensure_aware": {"ret": "2026-01-01T01:00:00+01:00"}}),
    case("parse_aware", value="not-a-date",
         io={"datetime.fromisoformat": raise_("ValueError", "Invalid isoformat string")}),
    case("parse_aware", value=5,
         io={"datetime.fromisoformat": raise_("TypeError", "argument must be str")}),
    case("parse_aware", value=ISO,
         io={"datetime.fromisoformat": {"ret": ISO},
             "_ensure_aware": raise_("ValueError", "zone")}),
    case("parse_aware", value=ISO,
         io={"datetime.fromisoformat": raise_("SystemExit", "bye")}),
    case("parse_aware", value=ISO, zone="Europe/Brussels",
         io={"datetime.fromisoformat": {"ret": ISO}, "_ensure_aware": {"ret": ISO}}),
]

# ── append_telemetry_record ───────────────────────────────────────────────
ENTRY = {"job_id": "j1", "n": 1}
APP_IO = store_io(_is_named_profile_path=FALSE, mkdir=OK, open=OK, write=OK, close=OK)
CASES += [
    case("append_telemetry_record", filename="m.jsonl", entry=ENTRY,
         state={"recent": []}, io=APP_IO),
    case("append_telemetry_record", filename="m.jsonl", entry=ENTRY,
         state={"recent": [{"i": i} for i in range(25)]}, io=APP_IO),
    case("append_telemetry_record", filename="m.jsonl", entry=ENTRY,
         state={"recent": []},
         io=store_io(_is_named_profile_path=FALSE, mkdir=OK,
                     open=raise_("OSError", "no file"))),
    case("append_telemetry_record", filename="m.jsonl", entry=ENTRY,
         state={"recent": []},
         io=store_io(_is_named_profile_path=FALSE, mkdir=OK, open=OK,
                     write=raise_("ValueError", "disk"), close=OK)),
    case("append_telemetry_record", filename="m.jsonl", entry=ENTRY,
         state={"recent": []},
         io=store_io(_is_named_profile_path=FALSE, mkdir=raise_("OSError", "ro"))),
    case("append_telemetry_record", filename="m.jsonl", entry=ENTRY,
         state={"recent": []},
         io={"_current_cron_store": raise_("RuntimeError", "no store")}),
    case("append_telemetry_record", filename="m.jsonl", entry=ENTRY,
         state={"recent": []},
         io=store_io(_is_named_profile_path=FALSE, mkdir=OK, open=OK,
                     write=raise_("SystemExit", "halt"), close=OK)),
    case("append_telemetry_record", filename="m.jsonl", entry=ENTRY,
         state={"recent": []},
         io=store_io(_is_named_profile_path=FALSE, mkdir=OK, open=OK, write=OK,
                     close=raise_("OSError", "close"))),
]

# ── record_timezone_migration_catchup ─────────────────────────────────────
TZJOB = {"id": "j1", "name": "Job", "schedule": {"expr": "0 9 * * *"}}
RAW = "2026-01-01T09:00:00+00:00"
NORM = "2026-01-01T10:00:00+01:00"
TZ_IO = store_io(_hermes_now={"ret": NORM}, _is_named_profile_path=FALSE,
                 mkdir=OK, open=OK, write=OK, close=OK)
CASES += [
    case("record_timezone_migration_catchup", job=TZJOB,
         raw_next_run_at=RAW, normalized_next_run_at=NORM, io=TZ_IO),
    case("record_timezone_migration_catchup",
         job={"id": "j1", "schedule": {"expr": "e"}},
         raw_next_run_at=RAW, normalized_next_run_at=NORM, io=TZ_IO),
    case("record_timezone_migration_catchup",
         job={"id": "j1", "name": "N", "schedule": None},
         raw_next_run_at=RAW, normalized_next_run_at=NORM, io=TZ_IO),
    case("record_timezone_migration_catchup", job=TZJOB,
         raw_next_run_at=RAW, normalized_next_run_at=NORM,
         state={"_timezone_migration_catchups": 5}, io=TZ_IO),
    case("record_timezone_migration_catchup", job=TZJOB,
         raw_next_run_at=RAW, normalized_next_run_at=NORM,
         io=store_io(_hermes_now={"ret": NORM}, _is_named_profile_path=FALSE,
                     mkdir=OK, open=raise_("OSError", "no"))),
    case("record_timezone_migration_catchup", job=TZJOB,
         raw_next_run_at=RAW, normalized_next_run_at=NORM,
         io=store_io(_hermes_now=raise_("RuntimeError", "clock"))),
]

# ── write_marker ──────────────────────────────────────────────────────────
CASES += [
    case("write_marker", name="ticker_heartbeat", text="123 456", tmp_prefix=".hb_",
         io=store_io(ensure_dirs=OK, atomic_write_text=OK)),
    case("write_marker", name="ticker_heartbeat", text="x", tmp_prefix=".hb_",
         io={"ensure_dirs": raise_("OSError", "dirs")}),
    case("write_marker", name="ticker_heartbeat", text="x", tmp_prefix=".hb_",
         io=store_io(ensure_dirs=OK, atomic_write_text=raise_("OSError", "write"))),
    case("write_marker", name="ticker_heartbeat", text="x", tmp_prefix=".hb_",
         io={"ensure_dirs": OK, "_current_cron_store": raise_("RuntimeError", "s")}),
    case("write_marker", name="ticker_heartbeat", text="x", tmp_prefix=".hb_",
         io=store_io(ensure_dirs=OK, atomic_write_text=raise_("SystemExit", "halt"))),
    case("write_marker", name="ticker_last_success", text="", tmp_prefix=".hb_",
         io=store_io(ensure_dirs=OK, atomic_write_text=OK)),
]

# ── read_marker_fields ────────────────────────────────────────────────────
def rd(text):
    return store_io(read_text={"ret": text})


CASES += [
    case("read_marker_fields", name="ticker_heartbeat", io=rd("123 456")),
    case("read_marker_fields", name="ticker_heartbeat", io=rd("\ufeff123 456")),
    case("read_marker_fields", name="ticker_heartbeat", io=rd("")),
    case("read_marker_fields", name="ticker_heartbeat",
         io=store_io(read_text=raise_("OSError", "missing"))),
    case("read_marker_fields", name="ticker_heartbeat", io=rd("  \t\n ")),
    case("read_marker_fields", name="ticker_heartbeat", io=rd("a\nb\tc  d")),
    case("read_marker_fields", name="ticker_heartbeat",
         io={"_current_cron_store": raise_("RuntimeError", "s")}),
]

# ── ticker_heartbeat_writer_alive ─────────────────────────────────────────
def hb(text, pid):
    return store_io(read_text={"ret": text}, pid_exists_stdlib=pid)


CASES += [
    case("ticker_heartbeat_writer_alive", io=hb("100 456", TRUE)),
    case("ticker_heartbeat_writer_alive", io=hb("100 456", FALSE)),
    case("ticker_heartbeat_writer_alive", io=hb("100", TRUE)),
    case("ticker_heartbeat_writer_alive", io=hb("100 abc", TRUE)),
    case("ticker_heartbeat_writer_alive",
         io=hb("100 456", raise_("RuntimeError", "pid probe"))),
    case("ticker_heartbeat_writer_alive", io=hb("", TRUE)),
    case("ticker_heartbeat_writer_alive", io=hb("100 -42", TRUE)),
    case("ticker_heartbeat_writer_alive",
         io=hb("100 456", raise_("SystemExit", "halt"))),
]

# ── unmerged_disk_jobs ────────────────────────────────────────────────────
JOBS_AB = [{"id": "a"}, {"id": "b"}]
CASES += [
    case("unmerged_disk_jobs", jobs=JOBS_AB, state={"load_stamp": [1, 2, 3]},
         io=store_io(_jobs_file_stamp={"ret": [1, 2, 3]})),
    case("unmerged_disk_jobs", jobs=JOBS_AB,
         io=store_io(_peek_jobs_unlocked={"ret": [{"id": "c"}, {"id": "a"}]})),
    case("unmerged_disk_jobs", jobs=JOBS_AB, state={"load_stamp": [1, 2, 3]},
         io=store_io(_jobs_file_stamp={"ret": [9, 9, 9]},
                     _peek_jobs_unlocked={"ret": [{"id": "c"}]})),
    case("unmerged_disk_jobs", jobs=JOBS_AB,
         io=store_io(_peek_jobs_unlocked=OK)),
    case("unmerged_disk_jobs", jobs=JOBS_AB, removed_ids=["c"],
         io=store_io(_peek_jobs_unlocked={"ret": [{"id": "c"}, {"id": "d"}]})),
    case("unmerged_disk_jobs", jobs=JOBS_AB,
         io=store_io(_peek_jobs_unlocked={"ret": [{"id": "c"}, {"id": "c"}]})),
    case("unmerged_disk_jobs", jobs=JOBS_AB,
         io=store_io(_peek_jobs_unlocked={"ret": ["x", {"id": "c"}, 5]})),
    case("unmerged_disk_jobs", jobs=JOBS_AB,
         io=store_io(_peek_jobs_unlocked={"ret": [{"id": ""}, {"id": None}, {"id": "c"}]})),
    case("unmerged_disk_jobs", jobs=["x", {"id": "a"}],
         io=store_io(_peek_jobs_unlocked={"ret": [{"id": "a"}, {"id": "b"}]})),
    case("unmerged_disk_jobs", jobs=[{"id": 1}],
         io=store_io(_peek_jobs_unlocked={"ret": [{"id": 1}, {"id": 2}]})),
    case("unmerged_disk_jobs", jobs=JOBS_AB, state={"load_stamp": [1, 2, 3]},
         io=store_io(_jobs_file_stamp=OK, _peek_jobs_unlocked={"ret": [{"id": "c"}]})),
]

# ── unlink_quiet ──────────────────────────────────────────────────────────
CASES += [
    case("unlink_quiet", path=None),
    case("unlink_quiet", path="/tmp/x", io={"os.unlink": OK}),
    case("unlink_quiet", path="/tmp/x", io={"os.unlink": raise_("OSError", "gone")}),
    case("unlink_quiet", path="/tmp/x", io={"os.unlink": raise_("FileNotFoundError", "nope")}),
    case("unlink_quiet", path="/tmp/x", io={"os.unlink": raise_("ValueError", "bad")}),
    case("unlink_quiet", path="/tmp/x", io={"os.unlink": raise_("SystemExit", "halt")}),
    case("unlink_quiet", path="", io={"os.unlink": OK}),
]

# ── stage_jobs_payload ────────────────────────────────────────────────────
TMP = "/h/cron/.jobs_x.tmp"
STG_IO = {"mkstemp_beside": {"ret": [9, TMP]}, "fdopen": TRUE,
          "_hermes_now": {"ret": ISO}, "json_dump": OK, "flush": OK,
          "fileno": {"ret": 9}, "fsync": OK, "close": OK, "os.unlink": OK}


def stg(**over):
    io = dict(STG_IO)
    io.update(over)
    return io


CASES += [
    case("stage_jobs_payload", jobs_file="/h/cron/jobs.json", jobs=[{"id": "a"}], io=stg()),
    case("stage_jobs_payload", jobs_file="/h/cron/jobs.json", jobs=[{"id": "a"}],
         io=stg(json_dump=raise_("ValueError", "serialize"))),
    case("stage_jobs_payload", jobs_file="/h/cron/jobs.json", jobs=[{"id": "a"}],
         io=stg(fdopen=raise_("OSError", "fd"))),
    case("stage_jobs_payload", jobs_file="/h/cron/jobs.json", jobs=[{"id": "a"}],
         io=stg(fsync=raise_("OSError", "sync"))),
    case("stage_jobs_payload", jobs_file="/h/cron/jobs.json", jobs=[{"id": "a"}],
         io=stg(json_dump=raise_("SystemExit", "halt"))),
    case("stage_jobs_payload", jobs_file="/h/cron/jobs.json", jobs=[{"id": "a"}],
         io=stg(mkstemp_beside=raise_("OSError", "tmp"))),
    case("stage_jobs_payload", jobs_file="/h/cron/jobs.json", jobs=[{"id": "a"}],
         io=stg(flush=raise_("ValueError", "flush"))),
    case("stage_jobs_payload", jobs_file="/h/cron/jobs.json", jobs=[{"id": "a"}],
         io=stg(close=raise_("OSError", "close"))),
]

# ── with_job ──────────────────────────────────────────────────────────────
WJ_IO = {"jobs_lock.enter": OK, "load_jobs": {"ret": [{"id": "a"}, {"id": "b"}]},
         "jobs_lock.exit": OK, "fn": {"ret": "R"}}


def wj(**over):
    io = dict(WJ_IO)
    io.update(over)
    return io


CASES += [
    case("with_job", job_id="a", io=wj()),
    case("with_job", job_id="b", io=wj()),
    case("with_job", job_id="z", io=wj()),
    case("with_job", job_id="z", missing="M", io=wj()),
    case("with_job", job_id="a", io=wj(fn=raise_("ValueError", "apply"))),
    case("with_job", job_id="a", io=wj(load_jobs=raise_("RuntimeError", "load"))),
    case("with_job", job_id="a", io=wj(load_jobs={"ret": []})),
    case("with_job", job_id="a", io=wj(load_jobs={"ret": [5]})),
    case("with_job", job_id="a", io=wj(**{"jobs_lock.enter": raise_("RuntimeError", "lock")})),
    case("with_job", job_id="a", io=wj(**{"jobs_lock.exit": raise_("OSError", "exit")})),
    case("with_job", job_id=1, io=wj(load_jobs={"ret": [{"id": 1}]})),
]

# ── next_run_or_reject_past_oneshot ───────────────────────────────────────
ONCE = {"kind": "once", "run_at": ISO}
NR = dict(label="L", fallback_run_at=None, what="create ", timezone=None)
CASES += [
    case("next_run_or_reject_past_oneshot", parsed_schedule=ONCE, **NR,
         io={"compute_next_run": {"ret": ISO}}),
    case("next_run_or_reject_past_oneshot", parsed_schedule={"kind": "interval"}, **NR,
         io={"compute_next_run": {"ret": "X"}}),
    case("next_run_or_reject_past_oneshot", parsed_schedule=ONCE, **NR,
         io={"compute_next_run": OK}),
    case("next_run_or_reject_past_oneshot",
         parsed_schedule={"kind": "once", "run_at": ""},
         label="L", fallback_run_at="FB", what="update ", timezone=None,
         io={"compute_next_run": OK}),
    case("next_run_or_reject_past_oneshot", parsed_schedule={"kind": "once"}, **NR,
         io={"compute_next_run": OK}),
    case("next_run_or_reject_past_oneshot", parsed_schedule=ONCE, **NR,
         io={"compute_next_run": raise_("RuntimeError", "compute")}),
    case("next_run_or_reject_past_oneshot", parsed_schedule={"kind": "once"},
         label="L", fallback_run_at=5, what="create ", timezone=None,
         io={"compute_next_run": OK}),
]

# ── apply_pin_update ──────────────────────────────────────────────────────
CASES += [
    case("apply_pin_update", job={"model": "m1"}, updates={"name": "x"}),
    case("apply_pin_update", job={"model": "m1"}, updates={"pinned": True, "model": "m2"}),
    case("apply_pin_update", job={"model": "m1"}, updates={"pinned": True},
         io={"_normalize_job_optional_text": {"ret": "m1"}}),
    case("apply_pin_update", job={"model": "  "}, updates={"pinned": True},
         io={"_normalize_job_optional_text": OK, "_main_model_pin": {"ret": ["p1", "m1"]}}),
    case("apply_pin_update", job={}, updates={"pinned": False}),
    case("apply_pin_update", job={}, updates={"pinned": "yes"},
         io={"_normalize_job_optional_text": OK, "_main_model_pin": {"ret": ["p2", "m2"]}}),
    case("apply_pin_update", job={}, updates={"pinned": True},
         io={"_normalize_job_optional_text": OK,
             "_main_model_pin": raise_("RuntimeError", "pin")}),
    case("apply_pin_update", job={"model": "m"}, updates={"pinned": 0}),
]

# ── refresh_claim ─────────────────────────────────────────────────────────
CLAIM = {"by": "o1", "at": "old"}
RC_IO = {"_hermes_now": {"ret": ISO}, "save_jobs": OK}
CASES += [
    case("refresh_claim", jobs=[{"id": "a"}], claim=CLAIM, expected_owner="o1", io=RC_IO),
    case("refresh_claim", jobs=[{"id": "a"}], claim=None, expected_owner="o1", io=RC_IO),
    case("refresh_claim", jobs=[{"id": "a"}], claim="str", expected_owner="o1", io=RC_IO),
    case("refresh_claim", jobs=[{"id": "a"}], claim=CLAIM, expected_owner="o2", io=RC_IO),
    case("refresh_claim", jobs=[{"id": "a"}], claim=CLAIM, expected_owner="o1",
         io={"_hermes_now": {"ret": ISO}, "save_jobs": raise_("OSError", "save")}),
    case("refresh_claim", jobs=[{"id": "a"}], claim={"at": "x"}, expected_owner="o1", io=RC_IO),
    case("refresh_claim", jobs=[{"id": "a"}], claim=CLAIM, expected_owner="o1",
         io={"_hermes_now": raise_("RuntimeError", "clock"), "save_jobs": OK}),
]


def main():
    FIXTURE.parent.mkdir(parents=True, exist_ok=True)
    with open(FIXTURE, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"wrote {len(CASES)} cases -> {FIXTURE}")
    if "--check" in sys.argv:
        sys.path.insert(0, str(HERE))
        import oracle_cron_jobs_flow as O
        bad = 0
        per = {}
        for c in CASES:
            func = c["func"]
            per[func] = per.get(func, 0) + 1
            try:
                out = O.run_case(func, c["in"])
            except Exception as e:
                bad += 1
                print(f"PYERR {func}: {type(e).__name__}: {e}")
                continue
            r = out["ret"]
            if isinstance(r, dict) and "__raise__" in r and r.get("message", "").startswith(
                    ("FATAL", "NameError")):
                bad += 1
                print(f"SUSPECT {func}: {r}")
        print(f"check: {len(CASES)} cases, {bad} python-side errors")
        for k in sorted(per):
            print(f"  {k}: {per[k]}")
        return 1 if bad else 0
    return 0


if __name__ == "__main__":
    sys.exit(main())