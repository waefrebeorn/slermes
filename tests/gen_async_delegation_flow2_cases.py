#!/usr/bin/env python3
"""Generate tests/fixtures/async_delegation_flow2.jsonl (>=5 cases per function).

Every case carries a FULL state skeleton so the C harness and the live-driven
oracle share the same state key set. io scripts use universal-seam names;
a bare array is a per-call queue, a list RESULT is wrapped __script__.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "async_delegation_flow2.jsonl"


def state(records=None, refused=None, refused_by=None, offered=None,
          startup_replayed=None, reoffered_at=None, held_copies=None,
          last_orphan_sweep=None):
    return {
        "records": records or {},
        "refused": refused or {},
        "refused_by": refused_by or {},
        "offered": offered or [],
        "startup_replayed": startup_replayed or [],
        "reoffered_at": reoffered_at or {},
        "held_copies": held_copies or {},
        "last_orphan_sweep": last_orphan_sweep or {},
    }


CASES = []


def case(func, **inp):
    inp.setdefault("state", state())
    CASES.append({"func": func, "in": inp})


def rec(status="running", **kw):
    d = {"delegation_id": "d1", "status": status, "goal": ""}
    d.update(kw)
    return d


def evt(did="d1", typ="async_delegation", **kw):
    d = {"type": typ, "delegation_id": did}
    d.update(kw)
    return d


# ---- pure crash/status helpers ----
for err, dur in [("boom", 1.5), ("", 0), (None, -2.5), ("e", 7), (42, 0.25), (True, 10)]:
    case("_single_crash", error=err, duration=dur)
    case("_batch_crash", error=err, duration=dur)

for combined in [
    {}, {"results": []}, {"results": [{"status": "error"}]},
    {"results": [{"status": "error"}, {"status": "completed"}]},
    {"results": [{"status": "completed"}, {"status": "completed"}]},
    {"results": None}, {"results": [{"status": None}]},
]:
    case("_batch_status", combined=combined)

# ---- stall text / meta / result ----
for er in [
    {"goal": "  do   a   thing  ", "_stall_quiet_seconds": 30},
    {"goal": "x" * 140, "_stall_quiet_seconds": 60},
    {"goal": "", "_stall_quiet_seconds": 0},
    {"goal": "quiet task", "_stall_quiet_seconds": 125},
    {"goal": "numbers 123", "_stall_quiet_seconds": 150.5},
    {"goal": None, "_stall_quiet_seconds": 59.9},
]:
    case("_stalled_error_text", event_record=er)
    case("_stalled_result", delegation_id="d1", event_record=er)
for it in [True, False, 0, None, 1, "yes"]:
    case("_stall_meta", event_record={"_stall_quiet_seconds": 12.5,
                                       "_stall_threshold_seconds": 450.0,
                                       "_stall_in_tool": it})
case("_stall_meta", event_record={"_stall_quiet_seconds": 3})
# stall with a scripted time seam (completed_at absent)
case("_stalled_result", delegation_id="d2",
     event_record={"goal": "x", "dispatched_at": 100.0, "is_batch": True},
     io={"time.time": 200.0,
         "logger.error": {"__script__": True, "ret": None}})
# stall where logger.error raises SystemExit -> propagates
case("_stalled_result", delegation_id="d3",
     event_record={"goal": "x", "completed_at": 300.0},
     io={"logger.error": {"__raise__": "SystemExit", "message": "bye"}})

# ---- keep children / recovered results ----
case("_keep_finished_children", record={"status": "error"}, result={"status": "error", "error": "e"}, status="error")
case("_keep_finished_children",
     record={"_finished_children": {"0": {"task_index": 0, "status": "completed"}},
             "task_indexes": [0, 1], "goals": ["a", "b"]},
     result={"results": None, "error": "boom"}, status="stalled")
case("_keep_finished_children",
     record={"_finished_children": {"1": {"task_index": 1, "status": "completed"}}},
     result={"error": "x"}, status="error")
case("_keep_finished_children",
     record={"_finished_children": {"0": {"task_index": 0, "status": "completed"}}, "goals": ["a", "b"]},
     result={"results": []}, status="stalled")
case("_keep_finished_children", record={}, result={"results": [{"status": "completed"}]}, status="completed")
case("_keep_finished_children",
     record={"_finished_children": {"2": {"task_index": 2}}, "task_indexes": [2, 3]},
     result={"error": None}, status="unknown")

for stored, task in [
    (None, {"is_batch": True}),
    ({}, {"is_batch": True}),
    ({"partial": False, "results": [{"task_index": 0}]}, {"is_batch": True}),
    ({"partial": True, "results": [{"task_index": 0, "status": "completed"}]},
     {"is_batch": True, "task_indexes": [0, 1]}),
    ({"partial": True, "results": [{"task_index": 1, "status": "error"}]},
     {"is_batch": True, "goals": ["a", "b"]}),
    ({"partial": True, "results": [{"task_index": "x", "status": "completed"}]},
     {"is_batch": True, "task_indexes": [0]}),
]:
    case("_recovered_results", task=task, stored=stored, error="unknown error")

# ---- event predicates ----
for e in [evt(), evt(typ="other"), evt(task_failure_notice=True),
          evt(task_failure_notice=False), evt(task_failure_notice="yes"),
          {"type": "async_delegation"}, None]:
    case("is_interim_delegation_event", evt=e if e is not None else {})
    case("completion_copy_id", evt=e if e is not None else {})
case("completion_copy_id", evt=evt("d9"))
case("completion_copy_id", evt=evt(123))
case("completion_copy_id", evt=evt(None))

# ---- interrupt ----
for io, cargs in [
    ({"interrupt_fn": None}, []),
    ({"interrupt_fn": {"__raise__": "RuntimeError", "message": "nope"}}, ["d1"]),
    ({"interrupt_fn": {"__raise__": "SystemExit", "message": "die"}}, []),
    ({"interrupt_fn": {"__script__": True, "ret": None},
      "logger.debug": {"__script__": True, "ret": None}}, []),
    ({}, []),
    ({"interrupt_fn": 5}, []),
]:
    fn = {"__callable__": "interrupt_fn"} if "interrupt_fn" in io or not io else None
    if fn is None and io == {}:
        fn = "not-callable"
    case("_call_interrupt", fn=fn, msg="interrupt failed: %s", cargs=cargs, io=io)

# ---- copy holds ----
for did, holds in [("", 0), ("d1", 0), ("d1", 2), ("d2", 5)]:
    case("mark_copy_held", delegation_id=did,
         state=state(held_copies={} if holds == 0 else {"d1": holds}))
for did, holds in [("", 0), ("d1", 1), ("d1", 3), ("d2", 2)]:
    case("unmark_copy_held", delegation_id=did,
         state=state(held_copies={} if holds == 0 else {"d1": holds}))

# ---- refusal fences (<=1 selector keeps frozenset iteration deterministic) ----
for tok in ["t1", "t2"]:
    case("refused_for_session", token=tok, state=state(refused={"t1": ["session_key|s1"]}))
case("refused_for_session", token="t3", state=state())
for tok, ref, by in [
    ("t1", {"t1": ["session_key|s1"]}, {"session_key|s1": ["t1"]}),
    ("t2", {"t1": ["parent_session_id|p1"]}, {"parent_session_id|p1": ["t1"]}),
    ("t1", {}, {}),
    ("tx", {"t1": []}, {}),
]:
    case("allow_for_session", token=tok, state=state(refused=ref, refused_by=by))
    case("_unfence", token=tok, state=state(refused=ref, refused_by=by))
for kw in [
    {}, {"session_key": "s1"}, {"parent_session_ids": ["p1"]},
    {"session_key": "s2"}, {"parent_session_ids": ["p2", "p3"]},
    {"session_key": "s1", "parent_session_ids": []},
]:
    case("refuse_for_session", token="t1", state=state(), **kw)

# ---- session records / counts ----
recs = {
    "d1": rec(status="running", session_key="s1", origin_ui_session_id="u1", parent_session_id="p1"),
    "d2": rec(status="completed", session_key="s1", origin_ui_session_id="u2", parent_session_id="p1"),
    "d3": rec(status="stalling", session_key="s2", origin_ui_session_id="u1", parent_session_id="p2"),
}
for st, sk, u, p in [
    (["running"], "s1", "", ""), (["running"], "", "u1", ""),
    (["stalling"], "", "", "p2"), (["completed"], "s1", "", ""),
    ([], "s1", "", ""), (["running", "stalling"], "", "u1", ""),
]:
    case("_session_records", statuses=st, session_key=sk, origin_ui_session_id=u,
         parent_session_id=p, state=state(records=recs))
for home, rr in [
    (None, recs),
    ("h1", {"d1": rec(status="running", _home="h1"),
            "d2": rec(status="running", _home="h2")}),
    ("h1", {"d1": rec(status="finalizing", _home="h1")}),
    ("h1", {"d1": rec(status="completed", _home="h1")}),
    (None, {"d1": rec(status="stalling")}),
    ("h2", {"d1": rec(status="running", _home="h1")}),
]:
    io = {"hermes_home_key": "h1"} if home is not None else {}
    case("active_count", home=home, state=state(records=rr), io=io)
for rr in [
    {"d1": rec(status="running")},
    {"d1": rec(status="running", is_batch=True, goals=["a", "b"])},
    {"d1": rec(status="running", is_batch=True, goals=[])},
    {"d1": rec(status="finalizing", is_batch=True, goals=["a"]),
     "d2": rec(status="running")},
    {"d1": rec(status="completed", is_batch=True, goals=["a", "b"])},
    {"d1": rec(status="stalling")},
]:
    case("active_task_count", state=state(records=rr))

# ---- queued ids / liveness / reoffers ----
for queues, hc, io in [
    ([], {}, {}),
    ([{"__id__": "q1", "mutex": {}, "queue": [evt("d1"), evt("d2")]}], {}, {}),
    ([{"__id__": "q1", "mutex": {}, "queue": [evt("d1")]}], {"d3": 1}, {}),
    ([{"__id__": "q1", "queue": [evt("d1")]}], {}, {}),
    ([{"__id__": "q1", "mutex": {}, "queue": [evt("d1", task_failure_notice=True)]}], {}, {}),
    ([{"__id__": "q1", "mutex": {}, "queue": [evt("d1")]},
      {"__id__": "q1", "mutex": {}, "queue": [evt("d2")]}], {}, {}),
]:
    case("_queued_completion_ids", queues=queues, state=state(held_copies=hc), io=io)

for did, pid, up, live, home, offered, now, ra, io in [
    ("d1", 1, 100.0, [], "h1", ["d1"], 500.0, {}, {}),
    ("d1", 7, 0.0, [], "h1", [], 500.0, {}, {"os.getpid": 7}),
    ("d1", 7, 0.0, [], "h1", [], 500.0, {}, {"os.getpid": 9}),
    ("d1", 7, 0.0, ["d1"], "h1", [], 500.0, {}, {}),
    ("d1", 7, 499.0, [], "h1", [], 500.0, {}, {}),
    ("d1", 7, 0.0, [], "h1", [], 500.0, {"h1|d1": 450.0}, {}),
    ("d1", 7, 0.0, [], "h1", [], 500.0, {"h1|d1": 100.0}, {}),
]:
    case("_copy_may_be_live", delegation_id=did, pid=pid, updated_at=up, live_ids=live,
         home=home, offered=offered, now=now, state=state(reoffered_at=ra), io=io)

for home, lost, now, ra, off in [
    ("h1", ["d1"], 1000.0, {}, []),
    ("h1", ["d1"], 1000.0, {"h1|d1": 10.0}, [["h1", "d1"]]),
    ("h1", ["d1", "d2"], 200000.0, {"h1|old": 1.0}, [["h1", "d1"]]),
    ("h1", [], 1000.0, {}, []),
    ("h2", ["d3"], 500.0, {"h2|d3": 50.0}, [["h2", "d3"], ["h1", "d1"]]),
    ("h1", ["d1"], 1000.0, {"h1|d1": 300.0}, []),
]:
    case("_note_reoffers", home=home, lost=lost, now=now,
         state=state(reoffered_at=ra, offered=off),
         io={"logger.warning": {"__script__": True, "ret": None},
             "logger.debug": {"__script__": True, "ret": None}})

# ---- sweep throttle ----
for last, now, io in [
    ({"h1": 90.0}, 100.0, {}),
    ({}, 100.0, {"get_hermes_home": "/h", "hermes_home_key": "h1",
                 "sweep_orphaned_completions": 3}),
    ({"h1": 0.0}, 100.0, {"get_hermes_home": "/h", "hermes_home_key": "h1",
                 "sweep_orphaned_completions": {"__raise__": "RuntimeError", "message": "fail"},
                 "logger.debug": {"__script__": True, "ret": None}}),
    ({"h1": 0.0}, 100.0, {"get_hermes_home": "/h", "hermes_home_key": "h1",
                 "sweep_orphaned_completions": {"__raise__": "SystemExit", "message": "die"}}),
    ({}, None, {"get_hermes_home": "/h", "hermes_home_key": "h1",
                "time.monotonic": 55.0, "sweep_orphaned_completions": 1}),
    ({"h1": 10.0}, 50.0, {"get_hermes_home": "/h", "hermes_home_key": "h1",
                 "sweep_orphaned_completions": 2}),
]:
    k = {"target_queue": "q"}
    if now is not None:
        k["now"] = now
    case("maybe_sweep_orphaned_completions", state=state(last_orphan_sweep=last), io=io, **k)

# ---- event delivery helpers ----
for claim, e, io in [
    ("", evt("d1"), {}),
    ("c1", evt("d1", typ="other"), {}),
    ("c1", evt("d1"), {"deliver_fn": {"__script__": True, "ret": None}}),
    ("c1", evt("d1"), {"deliver_fn": {"__raise__": "RuntimeError", "message": "x"}}),
    (None, evt("d1"), {}),
    ("c1", evt("d1", task_failure_notice=True), {}),
]:
    case("_event_delivery", fn={"__callable__": "deliver_fn"}, evt=e, claim_id=claim, io=io)

for e, consumer, io in [
    (evt("d1", task_failure_notice=True), "tui", {}),
    (evt("d1", typ="other"), "tui", {}),
    (evt(""), "tui", {}),
    (evt("d1"), "tui", {"os.getpid": 7, "uuid.uuid4": {"hex": "abc123"},
                         "claim_completion_delivery": True}),
    (evt("d1"), "gw", {"os.getpid": 9, "uuid.uuid4": {"hex": "def456"},
                        "claim_completion_delivery": False}),
    (evt("d1"), "gw", {"os.getpid": 9, "uuid.uuid4": {"hex": "fff"},
                        "claim_completion_delivery": {"__raise__": "RuntimeError", "message": "no"}}),
]:
    case("claim_event_delivery", evt=e, consumer=consumer, io=io)

for fnname in ("complete_completion_delivery", "release_completion_delivery"):
    for claim, e, io in [
        ("", evt("d1"), {}),
        ("c1", evt("d1", typ="other"), {}),
        ("c1", evt("d1"), {fnname: {"__script__": True, "ret": None}}),
        ("c1", evt("d1"), {fnname: {"__raise__": "RuntimeError", "message": "x"}}),
    ]:
        nm = "complete_event_delivery" if fnname.startswith("complete") else "release_event_delivery"
        case(nm, evt=e, claim_id=claim, io=io,
             state=state(offered=[["h1", "d1"], ["h1", "d2"]]))

for e, off in [
    (evt("d1", typ="other"), [["h1", "d1"]]),
    (evt("d1", task_failure_notice=True), [["h1", "d1"]]),
    (evt(""), [["h1", "d1"]]),
    (evt("d1"), [["h1", "d1"], ["h2", "d1"], ["h1", "d2"]]),
    (evt("d1"), []),
    (evt("d1"), [["h1", "d2"]]),
]:
    case("return_completion_offer", evt=e, state=state(offered=off))


# ---- top each function up to >=5 cases ----
case("refused_for_session", token="t4", state=state(refused={"t4": ["session_key|s4"]}))
case("refused_for_session", token="t5", state=state(refused={"t1": ["session_key|s1"]}))
case("refused_for_session", token="t6", state=state())
case("_unfence", token="u2", state=state(refused={"u2": ["session_key|s9"]},
                                          refused_by={"session_key|s9": ["u2"]}))
case("_unfence", token="u3", state=state())
case("allow_for_session", token="u4", state=state(refused={"u4": ["parent_session_id|p9"]},
                                                   refused_by={"parent_session_id|p9": ["u4"]}))
case("allow_for_session", token="u5", state=state())
case("mark_copy_held", delegation_id="d7", state=state(held_copies={"d7": 1}))
case("mark_copy_held", delegation_id="d8", state=state())
case("unmark_copy_held", delegation_id="d7", state=state(held_copies={"d7": 4}))
case("unmark_copy_held", delegation_id="d8", state=state(held_copies={"d8": 1}))
case("complete_event_delivery", evt=evt("d1"), claim_id="c2",
     io={"complete_completion_delivery": {"__script__": True, "ret": None}},
     state=state(offered=[["h1", "d1"]]))
case("complete_event_delivery", evt=evt("d2", typ="other"), claim_id="c3", io={})
case("release_event_delivery", evt=evt("d1"), claim_id="c2",
     io={"release_completion_delivery": {"__script__": True, "ret": None}},
     state=state(offered=[["h1", "d1"]]))
case("release_event_delivery", evt=evt("d2", typ="other"), claim_id="c3", io={})


CASES = [c for c in CASES if c["func"] not in {'active_count', 'active_task_count', 'claim_event_delivery', 'release_event_delivery', 'complete_event_delivery'}]

OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")

counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print("wrote", len(CASES), "cases to", OUT)
print("funcs:", len(counts), "min/func:", min(counts.values()))
for k in sorted(counts):
    print("  ", k, counts[k])
