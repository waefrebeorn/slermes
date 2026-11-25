#!/usr/bin/env python3
"""Differential oracle: agent.onboarding_text.reminder (Python) vs the C port.

Every reminder key is generated from real flow records, rendered by the Python layer, and compared
with the C output for the same record. Nothing is skipped: the ``build_*`` guidance is included, since
the C port renders it from the same generated catalog.

Usage:  python tests/oracle/onboarding_reminders_oracle.py [--keep]
Env:    HERMES_QUARRY (default C:/Users/eman5/hermes-agent)
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

QUARRY = Path(os.environ.get("HERMES_QUARRY", r"C:/Users/eman5/hermes-agent"))
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(QUARRY))

from agent import onboarding_text as text                        # noqa: E402
from session_rules import onboarding_flow as flow                 # noqa: E402
from session_rules.onboarding_first_tasks import (FIXED_ID, TASK_BY_ID,  # noqa: E402
                                                  eligible_tasks)

ALL_KEYS = [
    "after_name", "after_theme", "after_layout", "offer_first", "after_connectors", "after_plugins",
    "after_tour", "after_build", "tour_quick", "tour_full", "tour_skip", "moved_on", "skip_all",
    "complete", "already_theme", "already_layout", "wrote", "timed_out",
    # The catalog-driven guidance: rendered from the generated task table.
    "build_hermes", "build_own",
]


def facts_variants() -> list[tuple[str, dict]]:
    base = {"os": "linux", "arch": "x86_64", "machine_kind": "PC", "facts_source": "local",
            "locale": "en_US", "plugins": [], "tours": True}
    return [
        ("plain", dict(base)),
        ("name", dict(base)),
        ("picks", dict(base, plugins=["spotify"])),
    ]


def record_for(kind: str, facts: flow.Facts) -> dict:
    rec = flow.new_record(facts, now=0.0)
    if kind == "plain":
        return rec
    if kind == "name":
        rec["picks"].update({"name": "Ada", "accent": "mint", "layout": "terminal-deck",
                             "connected": ["github"], "newly_connected": [],
                             "installed": ["spotify"]})
        for beat in ("name", "theme", "layout", "connectors", "plugins", "tour"):
            rec["beats"][beat] = flow.DONE
        return rec
    # picks: everything settled so next_beat is None
    rec["picks"].update({"name": "", "accent": None, "layout": "sidebar-left", "connected": [],
                         "newly_connected": [], "installed": []})
    for beat in flow.BEAT_IDS:
        rec["beats"][beat] = flow.DONE
    return rec


def seam_for(facts: flow.Facts, picks) -> dict:
    rows = [{"id": t.id, "label": t.label, "goal": t.goal} for t in eligible_tasks(facts, picks)]
    return {"eligible_rows": rows, "fixed_label": TASK_BY_ID[FIXED_ID].label}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()

    cases = []
    expected = {}
    for label, fv in facts_variants():
        facts = flow.Facts(**fv)
        for kind in ("plain", "name", "picks"):
            rec = record_for(kind, facts)
            seam = seam_for(facts, rec["picks"])
            for key in ALL_KEYS:
                beat = "theme" if kind == "plain" else None
                name = f"{label}/{kind}/{key}"
                cases.append({"name": name, "key": key, "record": rec, "facts": fv,
                              "beat": beat, "seam": seam})
                expected[name] = {
                    "text": text.reminder(key, rec, facts, beat=beat),
                    "turn_note": text.turn_note(rec),
                    "finish": text.finish_reminder(rec),
                }

    tin = REPO / ".testbin" / "oremind_cases.json"
    tout = REPO / ".testbin" / "oremind_out.json"
    tin.parent.mkdir(exist_ok=True)
    tin.write_text(json.dumps(cases), encoding="utf-8")

    exe = REPO / ".testbin" / "oremind_diff.exe"
    if not exe.exists():
        build = subprocess.run(
            ["gcc", "-O1", "-w", "-I", "include", "-I", "lib", "-I", "lib/libjson",
             "src/agent/onboarding_reminders.c", "src/agent/onboarding_task_catalog.c",
             "src/agent/onboarding_task_catalog_data.c", "tests/unit/oremind_diff.c",
             "lib/libjson/json.o", "-o", str(exe)],
            cwd=str(REPO), capture_output=True, text=True)
        if build.returncode != 0:
            print("cannot build the harness:")
            print(build.stderr[-2000:])
            return 2

    proc = subprocess.run([str(exe), str(tin), str(tout)], capture_output=True, text=True)
    if proc.returncode != 0:
        print("harness failed:", proc.returncode, proc.stderr[-1000:])
        return 2
    got = {c["name"]: c for c in json.loads(tout.read_text(encoding="utf-8"))}

    agree = skipped = failed = 0
    for case in cases:
        name = case["name"]
        have = got.get(name)
        want = expected[name]
        if not have:
            print(f"FAIL {name}: no output")
            failed += 1
            continue
        if not have["ported"]:
            skipped += 1
            continue
        problems = []
        if have["text"] != want["text"]:
            problems.append(("text", want["text"], have["text"]))
        if have["turn_note"] != want["turn_note"]:
            problems.append(("turn_note", want["turn_note"], have["turn_note"]))
        if have["finish"] != want["finish"]:
            problems.append(("finish", want["finish"], have["finish"]))
        if problems:
            failed += 1
            print(f"FAIL {name}")
            for field, w, g in problems:
                print(f"     {field}\n       want: {w[:240]!r}\n       got:  {str(g)[:240]!r}")
        else:
            agree += 1

    print(f"onboarding-reminders oracle: {agree} agree, {skipped} not ported, {failed} mismatched")
    if not args.keep:
        for p in (tin, tout):
            p.unlink(missing_ok=True)
        exe.unlink(missing_ok=True)
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
