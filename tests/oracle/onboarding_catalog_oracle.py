#!/usr/bin/env python3
"""Differential oracle: session_rules.onboarding_first_tasks (Python) vs the generated C catalog.

Checks two things:
  * every task's prose (label, goal, steps, output, quality, done, skills) is byte-identical to the
    quarry's, so a stale regeneration cannot pass;
  * eligibility agrees for a grid of facts/picks -- the rows the build card would offer.

Usage: python tests/oracle/onboarding_catalog_oracle.py [--keep]
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

from session_rules import onboarding_first_tasks as cat  # noqa: E402
from session_rules.onboarding_flow import Facts  # noqa: E402


def case_grid() -> list[dict]:
    base = {"os": "linux", "arch": "x86_64", "machine_kind": "PC", "facts_source": "local",
            "locale": "en_US", "plugins": [], "tours": True}
    out = []
    variants = [
        ("nothing", dict(base), {"installed": [], "connected": []}),
        ("macos", dict(base, os="macos"), {"installed": [], "connected": []}),
        ("windows", dict(base, os="windows"), {"installed": [], "connected": []}),
        ("plugin", dict(base), {"installed": ["blender"], "connected": []}),
        ("two-plugins", dict(base), {"installed": ["blender", "nvidia-app"], "connected": []}),
        ("connected", dict(base), {"installed": [], "connected": ["github"]}),
        ("everything", dict(base, os="macos"),
         {"installed": ["blender", "nvidia-app", "nvidia-broadcast"], "connected": ["github", "slack"]}),
        ("unknown-plugin", dict(base), {"installed": ["nope"], "connected": []}),
    ]
    for name, facts, picks in variants:
        out.append({"name": name, "facts": facts, "picks": picks})
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()

    cases = case_grid()
    tin = REPO / ".testbin" / "ocat_cases.json"
    tout = REPO / ".testbin" / "ocat_out.json"
    tin.parent.mkdir(exist_ok=True)
    tin.write_text(json.dumps(cases), encoding="utf-8")

    exe = REPO / ".testbin" / "ocat_diff.exe"
    if not exe.exists():
        build = subprocess.run(
            ["gcc", "-O1", "-w", "-I", "include", "-I", "lib", "-I", "lib/libjson",
             "src/agent/onboarding_task_catalog.c", "src/agent/onboarding_task_catalog_data.c",
             "tests/unit/ocat_diff.c", "lib/libjson/json.o", "-o", str(exe)],
            cwd=str(REPO), capture_output=True, text=True)
        if build.returncode != 0:
            print("cannot build the harness:")
            print(build.stderr[-2000:])
            return 2

    proc = subprocess.run([str(exe), str(tin), str(tout)], capture_output=True, text=True)
    if proc.returncode != 0:
        print("harness failed:", proc.returncode, proc.stderr[-800:])
        return 2
    got = json.loads(tout.read_text(encoding="utf-8"))

    failed = 0
    # 1. prose
    py_tasks = {t.id: t for t in cat.FIRST_TASKS}
    c_tasks = {t["id"]: t for t in got["tasks"]}
    if set(py_tasks) != set(c_tasks):
        print(f"FAIL ids: python-only {sorted(set(py_tasks) - set(c_tasks))}, "
              f"c-only {sorted(set(c_tasks) - set(py_tasks))}")
        failed += 1
    for tid, pt in py_tasks.items():
        ct = c_tasks.get(tid)
        if not ct:
            continue
        wants = {"label": pt.label, "goal": pt.goal, "output": pt.output, "quality": pt.quality,
                 "done": pt.done, "skills": list(pt.skills), "steps": list(pt.steps)}
        for field, want in wants.items():
            if ct[field] != want:
                print(f"FAIL {tid}.{field}\n   want: {want!r}\n   got:  {ct[field]!r}")
                failed += 1
    if got["max_picks"] != cat.MAX_PICKS or got["max_label"] != cat.MAX_LABEL:
        print(f"FAIL limits: {got['max_picks']}/{got['max_label']} vs {cat.MAX_PICKS}/{cat.MAX_LABEL}")
        failed += 1
    if got["fixed_id"] != cat.FIXED_ID:
        print(f"FAIL fixed id: {got['fixed_id']!r} vs {cat.FIXED_ID!r}")
        failed += 1

    # 2. eligibility
    by_name = {c["name"]: c for c in got["cases"]}
    rows_checked = 0
    for case in cases:
        facts = Facts(**case["facts"])
        picks = case["picks"]
        want = [t.id for t in cat.eligible_tasks(facts, picks)]
        have = by_name[case["name"]]["eligible"]
        rows_checked += len(want)
        if want != have:
            print(f"FAIL {case['name']}: eligible\n   want: {want}\n   got:  {have}")
            failed += 1
        want_rows = [t.id for t in cat.eligible_tasks(facts, picks)]
        have_rows = [r["id"] for r in by_name[case["name"]]["rows"]]
        if want_rows != have_rows:
            print(f"FAIL {case['name']}: rows\n   want: {want_rows}\n   got:  {have_rows}")
            failed += 1

    print(f"onboarding-catalog oracle: {len(py_tasks)} tasks, {len(cases)} eligibility cases, "
          f"{rows_checked} rows, {failed} problem(s)")
    if not args.keep:
        for p in (tin, tout):
            p.unlink(missing_ok=True)
        exe.unlink(missing_ok=True)
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
