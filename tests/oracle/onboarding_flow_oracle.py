#!/usr/bin/env python3
"""Differential oracle: session_rules.onboarding_flow.apply (Python) vs the C port.

The C port is pure, so it can be compared case by case. This script:

  1. builds cases from the *real* Python flow (records made by ``new_record``, eligible ids from
     ``eligible_tasks``), applies each event sequence, and records the expected steps;
  2. runs the C harness (``tests/unit/oflow_diff.c``) over the same cases;
  3. compares the two structurally and reports every mismatch with the path that differs.

Usage:  python tests/oracle/onboarding_flow_oracle.py [--keep]
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

from session_rules import onboarding_flow as flow            # noqa: E402
from session_rules.onboarding_first_tasks import eligible_tasks  # noqa: E402


def facts_variants() -> list[dict]:
    base = {"os": "linux", "arch": "x86_64", "machine_kind": "PC", "facts_source": "local",
            "locale": "en_US", "plugins": [], "tours": True}
    return [
        dict(base, name="no-plugins"),
        dict(base, name="with-plugins", plugins=["spotify", "kanban"]),
        dict(base, name="no-tours", tours=False),
        dict(base, name="windows", os="windows", machine_kind="PC", plugins=["spotify"]),
    ]


def build_cases() -> list[dict]:
    """Each case: facts, a starting record, and the events applied to it in order."""
    cases: list[dict] = []
    for fv in facts_variants():
        label = fv["name"]
        facts = flow.Facts(**{k: v for k, v in fv.items() if k != "name"})

        def case(name: str, events: list[dict], record: dict | None = None) -> None:
            rec = record if record is not None else flow.new_record(facts, now=0.0)
            eligible = [t.id for t in eligible_tasks(facts, rec["picks"])]
            steps, cur = [], rec
            for i, ev in enumerate(events):
                step = flow.apply(cur, ev, facts, now=float(i + 1))
                steps.append({"record": step.record, "result": step.result, "reminder": step.reminder})
                cur = step.record
            cases.append({"name": f"{label}/{name}", "facts": fv_without_name(fv), "record": rec,
                          "events": events, "eligible": eligible, "expected": steps})

        # shown: only the visible beats, so the record's own state decides.
        case("show-name", [flow.Shown("name")])
        case("show-theme", [flow.Shown("theme")])
        case("show-connectors-no-suggest", [flow.Shown("connectors")])
        case("show-connectors-suggest", [flow.Shown("connectors", suggest=["github"])])
        case("show-tour", [flow.Shown("tour")])
        case("show-plugins", [flow.Shown("plugins")])
        case("show-build-no-options", [flow.Shown("build")])
        case("show-build-options", [flow.Shown("build", options=build_picks(facts))])

        # answered
        case("answer-name", [flow.Answered("name", {"name": "Ada"})])
        case("answer-name-blank", [flow.Answered("name", {"name": "   "})])
        case("answer-name-wrong-key", [flow.Answered("name", {"accent": "x"})])
        case("answer-name-said", [flow.Answered("name", {"name": "Ada"}, said="Ada")])
        case("answer-theme", [flow.Answered("theme", {"accent": "mint", "mode": "dark"})])
        case("answer-theme-null", [flow.Answered("theme", {"accent": None})])
        case("answer-layout", [flow.Answered("layout", {"layout": "terminal-deck", "label": "Elite"})])
        case("answer-layout-bad", [flow.Answered("layout", {"layout": "nope"})])
        case("answer-connectors", [flow.Answered("connectors", {"connected": ["github"],
                                                               "newly_connected": []})])
        case("answer-connectors-bad", [flow.Answered("connectors", {"connected": "github"})])
        case("answer-tour", [flow.Answered("tour", {"tour": "full"})])
        case("answer-tour-bad", [flow.Answered("tour", {"tour": "nope"})])
        case("answer-plugins", [flow.Answered("plugins", {"installed": list(fv.get("plugins") or [])})])
        case("answer-build-hermes", [flow.Answered("build", {"build": "hermes"})])
        case("answer-build-own", [flow.Answered("build", {"build": "own"})])
        case("answer-build-bad", [flow.Answered("build", {"build": "nonsense"})])

        # the other events
        case("wrote", [flow.Wrote("name", present={"name": "ignored"})])
        case("timed-out", [flow.TimedOut("theme", present={"accent": "mint"})])
        case("recorded-name", [flow.Recorded("name", "Grace")])
        case("recorded-theme", [flow.Recorded("theme", "mint")])
        case("recorded-layout", [flow.Recorded("layout", "terminal-deck")])
        case("recorded-layout-bad", [flow.Recorded("layout", "nope")])
        case("recorded-tour", [flow.Recorded("tour", "quick")])
        case("recorded-build-hermes", [flow.Recorded("build", "hermes")])
        case("skipped-theme", [flow.Skipped("theme")])
        case("end", [flow.End("skipped")])
        case("done-too-early", [flow.Done()])

        # applied (general tool results)
        case("applied-theme", [flow.Applied("set_theme", {},
                                            {"applied": {"accent": "mint", "mode": "dark"}})])
        case("applied-theme-layout", [flow.Applied("set_theme", {},
                                                   {"applied": {"accent": "mint", "layout": "terminal-deck"}})])
        case("applied-connections", [flow.Applied("manage_connections", {"action": "display"},
                                                  {"connected": ["github"], "newly_connected": ["github"]})])
        case("applied-tour", [flow.Applied("gui_tour", {"action": "start", "preset": "quick"}, {})])
        case("applied-other", [flow.Applied("read_file", {}, {})])

        # sequences: the record carries state across steps
        case("seq-show-then-answer", [flow.Shown("name"), flow.Answered("name", {"name": "Ada"})])
        case("seq-skip-then-next", [flow.Shown("theme"), flow.Skipped("theme")])
        case("seq-end-then-more", [flow.End(), flow.Done(), flow.Shown("theme")])
        case("seq-full-name", [flow.Shown("name"), flow.Answered("name", {"name": "Ada"}),
                               flow.Shown("theme"), flow.Recorded("theme", "mint"),
                               flow.Shown("layout")])
        # build settles -> run completes
        rec_build = flow.new_record(facts, now=0.0)
        for beat in ("name", "theme", "layout", "connectors", "plugins", "tour"):
            rec_build["beats"][beat] = flow.DONE
        case("seq-build-completes",
             [flow.Answered("build", {"build": "hermes"}), flow.Done(), flow.Shown("theme")],
             record=rec_build)
    return cases


def fv_without_name(fv: dict) -> dict:
    out = {k: v for k, v in fv.items() if k != "name"}
    out["plugins"] = list(out.get("plugins") or [])
    return out


def build_picks(facts) -> list[dict]:
    tasks = list(eligible_tasks(facts, {}))[:2]
    return [{"id": t.id, "label": t.label} for t in tasks]


_EVENT_KINDS = {
    "Shown": "shown", "Answered": "answered", "Wrote": "wrote", "TimedOut": "timed_out",
    "Recorded": "recorded", "Skipped": "skipped", "End": "end", "Done": "done", "Applied": "applied",
}


def event_to_json(ev) -> dict:
    """Dataclass -> the JSON shape the C port reads."""
    name = ev.kind if hasattr(ev, "kind") else _EVENT_KINDS[type(ev).__name__]
    if name == "shown":
        return {"type": "shown", "beat": ev.beat, "tool_call_id": ev.tool_call_id or "",
                "options": [dict(o) for o in (ev.options or [])],
                "suggest": list(ev.suggest or [])}
    if name == "answered":
        return {"type": "answered", "beat": ev.beat, "answer": dict(ev.answer), "said": ev.said or ""}
    if name in ("wrote", "timed_out", "recorded", "skipped"):
        out = {"type": name, "beat": ev.beat}
        if hasattr(ev, "value"):
            out["value"] = ev.value
        out["present"] = dict(ev.present or {}) if getattr(ev, "present", None) else {}
        return out
    if name == "end":
        return {"type": "end", "outcome": ev.outcome}
    if name == "done":
        return {"type": "done"}
    if name == "applied":
        return {"type": "applied", "tool": ev.tool, "args": dict(ev.args), "result": dict(ev.result)}
    raise SystemExit(f"unmapped event {name}")


def diff(path: str, want, got, out: list[str]) -> None:
    if type(want) is not type(got) and not (isinstance(want, (int, float)) and isinstance(got, (int, float))):
        out.append(f"{path}: type {type(want).__name__} vs {type(got).__name__} ({want!r} vs {got!r})")
        return
    if isinstance(want, dict):
        for k in sorted(set(want) | set(got)):
            if k not in want:
                out.append(f"{path}.{k}: unexpected {got[k]!r}")
            elif k not in got:
                out.append(f"{path}.{k}: missing (expected {want[k]!r})")
            else:
                diff(f"{path}.{k}", want[k], got[k], out)
    elif isinstance(want, list):
        if len(want) != len(got):
            out.append(f"{path}: length {len(want)} vs {len(got)}")
        for i, (a, b) in enumerate(zip(want, got)):
            diff(f"{path}[{i}]", a, b, out)
    elif isinstance(want, float) and isinstance(got, (int, float)):
        if abs(float(want) - float(got)) > 1e-9:
            out.append(f"{path}: {want!r} vs {got!r}")
    elif want != got:
        out.append(f"{path}: {want!r} vs {got!r}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()

    cases = build_cases()
    payload = [
        {"name": c["name"], "facts": c["facts"], "record": c["record"],
         "eligible": c["eligible"], "events": [event_to_json(e) for e in c["events"]]}
        for c in cases
    ]
    tin = REPO / ".testbin" / "oflow_cases.json"
    tout = REPO / ".testbin" / "oflow_out.json"
    tin.parent.mkdir(exist_ok=True)
    tin.write_text(json.dumps(payload), encoding="utf-8")

    exe = REPO / ".testbin" / "oflow_diff.exe"
    if not exe.exists():
        # Build the harness the same way the rest of the tree builds objects: gcc + libjson.
        exe.parent.mkdir(exist_ok=True)
        build = subprocess.run(
            ["gcc", "-O1", "-w", "-I", "include", "-I", "lib", "-I", "lib/libjson",
             "src/agent/onboarding_flow_apply.c", "tests/unit/oflow_diff.c",
             "lib/libjson/json.o", "-o", str(exe)],
            cwd=str(REPO), capture_output=True, text=True,
        )
        if build.returncode != 0:
            print("cannot build the harness:")
            print(build.stderr[-2000:])
            return 2
    proc = subprocess.run([str(exe), str(tin), str(tout)], capture_output=True, text=True)
    if proc.returncode != 0:
        print("harness failed:", proc.returncode)
        print(proc.stdout[-3000:])
        print(proc.stderr[-2000:])
        return 2
    got = {c["name"]: c for c in json.loads(tout.read_text(encoding="utf-8"))}

    failures = 0
    total_steps = 0
    for case in cases:
        name = case["name"]
        if name not in got:
            print(f"FAIL {name}: no output from harness")
            failures += 1
            continue
        problems: list[str] = []
        for i, (want, have) in enumerate(zip(case["expected"], got[name]["steps"])):
            total_steps += 1
            diff(f"step{i}.record", want["record"], have.get("record"), problems)
            diff(f"step{i}.result", want["result"], have.get("result"), problems)
            diff(f"step{i}.reminder", want["reminder"], have.get("reminder"), problems)
        if problems:
            failures += 1
            print(f"FAIL {name}")
            for p in problems[:8]:
                print(f"     {p}")

    print(f"onboarding-flow oracle: {len(cases)} cases, {total_steps} steps, "
          f"{failures} case(s) mismatched")
    if not args.keep:
        for p in (tin, tout):
            p.unlink(missing_ok=True)
        exe.unlink(missing_ok=True)
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
