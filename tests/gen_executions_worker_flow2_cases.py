#!/usr/bin/env python3
"""Generate JSONL fixtures for tests/oracle_executions_worker_flow2.py.

Each case is {"func": <name>, "in": {"args": {...}, "io": {...}}} where `io`
scripts the THREE universal seams `_persist`, `_value` and `_ledger` (the only
I/O in the port). `args` holds the live method's parameters (minus self).

Shape rules (shared with the C port header):
  * io[name] = a plain value (every call) | a list (per-call sequence) |
    {"__raise__": cls, "message": m} (raised exception) |
    {"__script__": true, "ret": v} / {"__script__": true, "raise": {...}}.
  * an absent io[name] fail-closes with RuntimeError ("no script for <name>").
  * enum/NewType constructors are identity; a `lease` is a {holder, ttl_seconds}
    object; delegation payloads are plain JSON values mapped by
    delegation_payload(x) -> {"delegation_id": x}.
  * int/str/bool seam results stay in the documented domain.

Run: python tests/gen_executions_worker_flow2_cases.py
Writes tests/fixtures/executions_worker_flow2.jsonl (>=6 cases per func).
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve()
OUT = HERE.parent / "fixtures" / "executions_worker_flow2.jsonl"

CASES = []


def case(func, args, io=None):
    CASES.append({"func": func, "in": {"args": args, "io": io or {}}})


def ident(seed=0):
    return {
        "epoch": f"ep{seed}",
        "execution_id": f"ex{seed}",
        "session_id": f"ses{seed}",
        "generation": seed,
        "sequence": seed + 1,
    }


def with_ident(seed, **kw):
    d = ident(seed)
    d.update(kw)
    return d


def raised(cls, message):
    return {"__raise__": cls, "message": message}


def lease(holder, ttl):
    return {"holder": holder, "ttl_seconds": ttl}


BOOL_SCRIPTS = [
    True, False, 1, 0, 2, "yes", "", None, [], [1], {"a": 1}, {},
    raised("RuntimeError", "boom"),
    raised("SystemExit", "bye"),           # BaseException passthrough
    [True, False, 1],                      # per-call sequence
    {"__script__": True, "ret": True},
]

# ── _value: bool-returning lease verbs ───────────────────────────────────
for fn, op in [("renew_turn_lease", "turn.renew"),
               ("acquire_compression_lock", "compression.lock.acquire"),
               ("renew_compression_lock", "compression.lock.renew"),
               ("reopen_orphaned_compression", "compression.reopen")]:
    for i, scr in enumerate(BOOL_SCRIPTS):
        if "lease" in (fn,):
            args = with_ident(i, lease=lease(f"holder{i}", i * 1.5))
        elif "compression_lock" in fn:
            args = with_ident(i, lease=lease(f"lock{i}", i + 1))
        else:
            args = ident(i)
        case(fn, args, {"_value": scr})
    case(fn, ident(99), {})                 # fail-closed: no _value script

# ── _value: None-returning / identity-returning verbs ────────────────────
# touch_session_activity
for i, scr in enumerate([
    None, 0, True, "ok", {"value": 1}, raised("AuthorityRefusal", "stale"),
    raised("SystemExit", "bye"), [None, "x"], {"__script__": True, "ret": "z"},
]):
    case("touch_session_activity", with_ident(
        i, activity={"ts": i * 1.0, "description": f"d{i}", "provenance": "p"}),
        {"_value": scr})
case("touch_session_activity", with_ident(20, activity={}), {})

# clear_session_activity
for i, scr in enumerate([
    None, 1, "x", False, raised("RuntimeError", "x"),
    raised("KeyboardInterrupt", "stop"), [0, 1], {"__script__": True, "ret": None},
]):
    case("clear_session_activity", ident(i), {"_value": scr})
case("clear_session_activity", ident(30), {})

# end_session
for i, scr in enumerate([
    None, 0, "done", raised("AuthorityRefusal", "bad"), raised("SystemExit", "bye"),
    [None, 1], {"__script__": True, "raise": raised("RuntimeError", "wrap")},
]):
    case("end_session", with_ident(i, end_reason=f"reason{i}"), {"_value": scr})
case("end_session", with_ident(40, end_reason=None), {})

# set_session_system_prompt
for i, scr in enumerate([
    None, 1, "ok", raised("RuntimeError", "x"), raised("SystemExit", "bye"),
    [0, 1], {"__script__": True, "ret": 5},
]):
    case("set_session_system_prompt", with_ident(i, system_prompt=f"sys{i}"), {"_value": scr})
case("set_session_system_prompt", with_ident(50, system_prompt=None), {})

# latest_conversation_role (identity return)
for i, scr in enumerate([
    "user", None, "assistant", 0, 1, "", ["tool"], {"role": "x"},
    raised("RuntimeError", "x"), raised("SystemExit", "bye"), ["a", "b"],
    {"__script__": True, "ret": "z"},
]):
    case("latest_conversation_role", ident(i), {"_value": scr})
case("latest_conversation_role", ident(60), {})

# message_role (identity return, row_id payload)
for i, scr in enumerate([
    "user", None, "assistant", 0, "x", raised("AuthorityRefusal", "r"),
    raised("SystemExit", "bye"), ["user", None], {"__script__": True, "ret": "a"},
]):
    case("message_role", with_ident(i, row_id=i), {"_value": scr})
case("message_role", with_ident(70, row_id=7), {})

# compression_lock_holder (identity return)
for i, scr in enumerate([
    None, "holder", "", 0, "h", raised("RuntimeError", "x"),
    raised("SystemExit", "bye"), ["h1", "h2"], {"__script__": True, "ret": "z"},
]):
    case("compression_lock_holder", ident(i), {"_value": scr})
case("compression_lock_holder", ident(80), {})

# cleanup_compression_lock
for i, scr in enumerate([
    None, 1, "ok", raised("RuntimeError", "x"), raised("SystemExit", "bye"),
    [0, 1], {"__script__": True, "ret": "z"},
]):
    case("cleanup_compression_lock", with_ident(i, target_session_id=f"t{i}", holder=f"h{i}"),
         {"_value": scr})
case("cleanup_compression_lock", with_ident(90, target_session_id="t", holder="h"), {})

# record_compression_cooldown
for i, scr in enumerate([
    None, 1, "ok", raised("RuntimeError", "x"), raised("SystemExit", "bye"),
    [0, 1], {"__script__": True, "ret": "z"},
]):
    case("record_compression_cooldown", with_ident(
        i, cooldown={"cooldown_until": i * 1.0, "error": f"e{i}"}), {"_value": scr})
case("record_compression_cooldown", with_ident(91, cooldown={}), {})

# restore_compression_cooldown
for i, scr in enumerate([
    None, 1, "ok", raised("RuntimeError", "x"), raised("SystemExit", "bye"),
    [0, 1], {"__script__": True, "ret": "z"},
]):
    case("restore_compression_cooldown", with_ident(
        i, snapshot={"session_exists": bool(i % 2), "cooldown_until": i * 1.0,
                     "error": f"e{i}"}), {"_value": scr})
case("restore_compression_cooldown", with_ident(92, snapshot={}), {})

# clear_compression_cooldown
for i, scr in enumerate([
    None, 1, "ok", raised("RuntimeError", "x"), raised("SystemExit", "bye"),
    [0, 1], {"__script__": True, "ret": "z"},
]):
    case("clear_compression_cooldown", ident(i), {"_value": scr})
case("clear_compression_cooldown", ident(93), {})

# set_compression_fallback_streak
for i, scr in enumerate([
    None, 1, "ok", raised("RuntimeError", "x"), raised("SystemExit", "bye"),
    [0, 1], {"__script__": True, "ret": "z"},
]):
    case("set_compression_fallback_streak", with_ident(i, streak=i), {"_value": scr})
case("set_compression_fallback_streak", with_ident(94, streak=0), {})

# set_compression_ineffective_count
for i, scr in enumerate([
    None, 1, "ok", raised("RuntimeError", "x"), raised("SystemExit", "bye"),
    [0, 1], {"__script__": True, "ret": "z"},
]):
    case("set_compression_ineffective_count", with_ident(i, count=i), {"_value": scr})
case("set_compression_ineffective_count", with_ident(95, count=0), {})

# set_compression_recovery_deadline
for i, scr in enumerate([
    None, 1, "ok", raised("RuntimeError", "x"), raised("SystemExit", "bye"),
    [0, 1], {"__script__": True, "ret": "z"},
]):
    case("set_compression_recovery_deadline", with_ident(i, deadline=i * 1.5), {"_value": scr})
case("set_compression_recovery_deadline", with_ident(96, deadline=None), {})

# ── _persist: projection verbs ───────────────────────────────────────────
# compression_session_context -> result['session']
for i, scr in enumerate([
    {"session": {"k": i}}, {"session": None}, {"session": [1, 2, 3]},
    {"session": "s"}, {"other": 1, "session": {"x": True}},
    raised("RuntimeError", "x"), raised("SystemExit", "bye"),
    [{"session": 1}, {"session": 2}], {"__script__": True, "ret": {"session": "z"}},
]):
    case("compression_session_context", with_ident(i, target_session_id=f"t{i}"),
         {"_persist": scr})
case("compression_session_context", with_ident(97, target_session_id="t"), {})

# compression_lineage_view
def lineage(i, flag=False, ident1=None):
    return {
        "readable_ids": [f"r{i}", "r2"], "lineage": [f"l{i}"],
        "root": f"root{i}", "tip": f"tip{i}", "resume": f"res{i}",
        "identity": [flag, f"id{i}" if ident1 is None else ident1],
        "tool_root": f"tool{i}",
    }

for i, scr in enumerate([
    lineage(0), lineage(1, True), lineage(2, 0, 5), lineage(3, "", None),
    lineage(4, 2, 3.5), raised("RuntimeError", "x"), raised("SystemExit", "bye"),
    [lineage(5), lineage(6)], {"__script__": True, "ret": lineage(7)},
]):
    case("compression_lineage_view", with_ident(i, target_session_id=f"t{i}"),
         {"_persist": scr})
case("compression_lineage_view", with_ident(98, target_session_id="t"), {})

# compression_history
for i, scr in enumerate([
    {"messages": []}, {"messages": [{"role": "user"}]},
    {"messages": ["a", "b"]}, {"messages": [None, 0, ""]},
    raised("RuntimeError", "x"), raised("SystemExit", "bye"),
    [{"messages": [1]}, {"messages": [2]}],
    {"__script__": True, "ret": {"messages": [3]}},
]):
    case("compression_history", with_ident(
        i, target_session_id=f"t{i}", include_ancestors=bool(i % 2),
        include_inactive=bool((i + 1) % 2), repair_alternation=False,
        include_row_ids=True, include_compacted=False), {"_persist": scr})
case("compression_history", with_ident(99, target_session_id="t", include_ancestors=True,
     include_inactive=False, repair_alternation=True, include_row_ids=False,
     include_compacted=True), {})

# ── _ledger: direct delegation verbs ─────────────────────────────────────
VIEW = {
    "delegation_id": "d", "origin_session": "s", "state": "done",
    "dispatched_at": 1.0, "completed_at": None, "delivery_state": "acked",
    "delivery_attempts": 2, "origin_session_id": "os", "result": {"ok": True},
}
for fn, field in [("dispatch_delegation", "dispatch"),
                  ("complete_delegation", "completion"),
                  ("record_delegation_child", "child")]:
    for i, scr in enumerate([
        None, {"value": 1}, "ok", raised("RuntimeError", "x"),
        raised("AuthorityRefusal", "bad"), raised("SystemExit", "bye"),
        [None, 1], {"__script__": True, "ret": "z"},
    ]):
        args = with_ident(i, worker_pid=100 + i, worker_birth=1000.0 + i)
        args[field] = f"{field}{i}" if i % 2 else {"event": field, "n": i}
        if fn == "dispatch_delegation":
            args["owner_started_at"] = i if i % 2 == 0 else None
            args["probe_refused"] = "permission_denied" if i % 3 == 0 else None
        case(fn, args, {"_ledger": scr})
    base = with_ident(40, worker_pid=200, worker_birth=2000.0)
    base[field] = "x"
    if fn == "dispatch_delegation":
        base["owner_started_at"] = None
        base["probe_refused"] = None
    case(fn, base, {})

# read_delegation -> WorkerDelegationView(**ret) -> dict
for i, scr in enumerate([
    dict(VIEW, delegation_id=f"d{i}"), dict(VIEW, state="pending", completed_at=None),
    {}, raised("RuntimeError", "x"), raised("AuthorityRefusal", "bad"),
    raised("SystemExit", "bye"), [dict(VIEW), dict(VIEW, state="x")],
    {"__script__": True, "ret": dict(VIEW)},
]):
    case("read_delegation", with_ident(i, delegation_id=f"d{i}",
         worker_pid=300 + i, worker_birth=3000.0 + i), {"_ledger": scr})
case("read_delegation", with_ident(41, delegation_id="d", worker_pid=1, worker_birth=2.0), {})

# withdraw_delegation
for i, scr in enumerate([
    None, 1, "ok", raised("RuntimeError", "x"), raised("AuthorityRefusal", "bad"),
    raised("SystemExit", "bye"), [None, 1], {"__script__": True, "ret": "z"},
]):
    case("withdraw_delegation", with_ident(i, delegation_id=f"d{i}",
         worker_pid=400 + i, worker_birth=4000.0 + i), {"_ledger": scr})
case("withdraw_delegation", with_ident(42, delegation_id="d", worker_pid=1, worker_birth=2.0), {})

# ── _ledger: five delivery verbs (via _delivery) ─────────────────────────
for fn in ["claim_delegation_delivery", "ack_delegation_delivery",
           "drop_delegation_delivery", "defer_delegation_delivery",
           "release_delegation_delivery"]:
    for i, scr in enumerate(BOOL_SCRIPTS):
        case(fn, with_ident(i, claim=f"c{i}" if i % 2 else {"delegation_id": f"c{i}"},
             worker_pid=500 + i, worker_birth=5000.0 + i), {"_ledger": scr})
    case(fn, with_ident(43, claim="c", worker_pid=1, worker_birth=2.0), {})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(counts), OUT))
    bad = {k: v for k, v in counts.items() if v < 6}
    if bad:
        raise SystemExit("under 6 cases/func: %r" % bad)


if __name__ == "__main__":
    main()
