#!/usr/bin/env python3
"""
sta_oracle_cron_notepad.py — Oracle for cron/notepad.py.

Replays the same operation sequence against the LIVE module in its own temp
database and diffs every observable: stored values, list order, delete/clear
counts, the rendered prompt section, and the exact ValueError text for each
cap.
"""
import json
import re
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
# The oracle driver sets HOME to a scratch dir; pathlib's Path.home() on Windows
# needs USERPROFILE, and hermes_constants resolves a platform default home at
# import time. Point it at the real profile — the DB path is passed explicitly.
import os  # noqa: E402
os.environ.setdefault("USERPROFILE", r"C:\Users\eman5")

# `import cron.notepad` runs cron/__init__.py, which pulls cron.jobs -> utils ->
# ruamel.yaml (not installed here). Load the module file directly and stub the
# one helper it imports lazily from cron.jobs.
import importlib.util  # noqa: E402
import os  # noqa: E402
import types  # noqa: E402

_pkg = types.ModuleType("cron")
_pkg.__path__ = ["C:/Users/eman5/hermes-agent/cron"]
sys.modules.setdefault("cron", _pkg)
_stub = types.ModuleType("cron.jobs")
_stub._ensure_cron_dir = lambda p: os.makedirs(p, exist_ok=True)
sys.modules.setdefault("cron.jobs", _stub)

_spec = importlib.util.spec_from_file_location(
    "cron.notepad", "C:/Users/eman5/hermes-agent/cron/notepad.py")
NP = importlib.util.module_from_spec(_spec)
sys.modules["cron.notepad"] = NP
_spec.loader.exec_module(NP)


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE cron_notepad: 0 cases (link failure in disguise)")
        return 1

    # One shared temp DB, in the same order the harness uses.
    tmp = Path(tempfile.mkdtemp(dir=".testbin")) / "notepad.db"
    NP.NOTEPAD_FILE = tmp

    def do_set(job, key, value):
        try:
            rec = NP.set_note(job, key, value)
            return True, None, rec
        except ValueError as exc:
            return False, str(exc), None

    # Replay in harness order so state matches, then compare.
    replay = []
    replay.append(do_set("job1", "cursor", "5"))
    present = NP.get_note("job1", "cursor")
    absent = NP.get_note("job1", "nope")
    replay.append(do_set("job1", "cursor", "9"))
    replay.append(do_set("job1", "seen", "alpha"))
    replay.append(do_set("job2", "cursor", "1"))
    notes = NP.list_notes("job1")
    del_present = NP.delete_note("job1", "seen")
    del_absent = NP.delete_note("job1", "seen")
    render_job1 = NP.render_notepad_section("job1")
    render_empty = NP.render_notepad_section("empty-job")
    clear1 = NP.clear_notepad("job1")
    clear2 = NP.clear_notepad("job1")
    key_long = do_set("job1", "k" * 199, "v")
    value_big = do_set("job1", "k", "v" * (NP.MAX_VALUE_BYTES + 99))
    for i in range(5):
        try:
            NP.set_note("capjob", f"fill_{i}", "x" * (13 * 1024 - 1))
        except ValueError:
            pass  # the harness ignores these too; the state must match
    total_over = do_set("capjob", "sixth", "x" * (13 * 1024 - 1))
    NP.clear_notepad("capjob")
    empty_job = do_set("", "k", "v")
    empty_key = do_set("job1", "", "v")

    expectation = {
        "set:first": replay[0], "set:update": replay[1],
        "set:second_key": replay[2], "set:other_job": replay[3],
        "set:key_too_long": key_long, "set:value_too_large": value_big,
        "set:job_total_overflow": total_over, "set:empty_job_id": empty_job,
        "set:empty_key": empty_key,
        "get:present": present, "get:absent": absent,
        "delete:present": del_present, "delete:absent": del_absent,
        "clear:job1": clear1, "clear:job1_again": clear2,
        "render:job1": render_job1, "render:empty": render_empty,
        "list:job1": [{k: v for k, v in n.items()} for n in notes],
    }

    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "set":
            ok, err, rec = expectation["set:" + c["case"]]
            if bool(c["ok"]) != ok:
                mism += 1
                print(f"MISMATCH set {c['case']}: C ok={c['ok']} PY ok={ok} "
                      f"(C err={c['err']!r} PY err={err!r})")
            elif not ok and c["err"] != err:
                mism += 1
                print(f"MISMATCH set {c['case']}.err: C={c['err']!r} PY={err!r}")
            elif ok and (c["key"] != rec["key"] or c["value"] != rec["value"]):
                mism += 1
                print(f"MISMATCH set {c['case']}: C={c['key']}/{c['value']} "
                      f"PY={rec['key']}/{rec['value']}")
        elif fam == "get":
            want = expectation["get:" + c["case"]]
            if c["value"] != want:
                mism += 1
                print(f"MISMATCH get {c['case']}: C={c['value']!r} PY={want!r}")
        elif fam == "delete":
            want = expectation["delete:" + c["case"]]
            if bool(c["ret"]) != bool(want):
                mism += 1
                print(f"MISMATCH delete {c['case']}: C={c['ret']} PY={want}")
        elif fam == "clear":
            want = expectation["clear:" + c["case"]]
            if c["ret"] != want:
                mism += 1
                print(f"MISMATCH clear {c['case']}: C={c['ret']} PY={want}")
        elif fam == "render":
            want = expectation["render:" + c["case"]]
            if c["text"] != want:
                mism += 1
                print(f"MISMATCH render {c['case']}:\n  C ={c['text']!r}\n  PY={want!r}")
        elif fam == "list":
            want = expectation["list:" + c["case"]]
            got = [{k: v for k, v in n.items()} for n in (c["notes"] or [])]
            # updated_at is a clock reading, so compare its shape and offset,
            # not the microsecond digits, on both sides.
            got_core = [{k: v for k, v in row.items() if k != "updated_at"} for row in got]
            want_core = [{k: v for k, v in row.items() if k != "updated_at"} for row in want]
            if got_core != want_core:
                mism += 1
                print(f"MISMATCH list {c['case']}:\n  C ={got_core}\n  PY={want_core}")
            for row in got:
                ts = row.get("updated_at") or ""
                if not re.fullmatch(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{6}[+-]\d{2}:\d{2}", ts):
                    mism += 1
                    print(f"MISMATCH list {c['case']}.updated_at shape: C={ts!r}")
                elif ts[-6:] != (want[0]["updated_at"][-6:] if want else ""):
                    mism += 1
                    print(f"MISMATCH list {c['case']}.updated_at offset: "
                          f"C={ts[-6:]!r} PY={want[0]['updated_at'][-6:]!r}")

    print(f"ORACLE cron_notepad: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
