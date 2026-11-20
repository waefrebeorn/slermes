#!/usr/bin/env python3
"""Generate tests/fixtures/goals_flow.jsonl for the UNCLAIMED goal FLOW
oracle. Each line is {"func": <name>, "in": {...}}; "in.io" carries the
scripted seam results ({"ret","raise","import_raise"} or a queue LIST)
consumed identically by the C harness dispatcher and the oracle's
recording stubs. EVERY seam is fail-closed (missing script -> RuntimeError
"unscripted seam: <name>"). List-valued seam results are wrapped {"ret":
[...]}. Pure data — no quarry import. >=5 cases per function.

Fixture domain notes (mirrored in the port header):
- str()-computed positions carry str/int/bool/None only; non-integral
  floats appear only where Python str(float) is exercised (seconds tgt).
- int()-parsed strings are simple ASCII (digits/sign/underscore/space).
- waiting_reason / reason / verdict / message parameters are str|null.
- barrier keys are real GoalState fields; attr is subgoals|gates|bogus.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "goals_flow.jsonl")
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(v):
    return {"ret": v}


def X(type, message="", **extra):
    d = {"type": type, "message": message}
    d.update(extra)
    return {"raise": d}


def IMP(type, message="", **extra):
    d = {"type": type, "message": message}
    d.update(extra)
    return {"import_raise": d}


def GS(**kw):
    """Full GoalState dict (dataclass defaults), overrides as-is."""
    d = {
        "goal": "ship the feature", "status": "active", "turns_used": 3,
        "max_turns": 20, "created_at": 1700000000, "last_turn_at": 1700000100,
        "last_verdict": "continue", "last_reason": "keep going",
        "paused_reason": None, "consecutive_parse_failures": 0,
        "consecutive_transport_failures": 0, "subgoals": [],
        "waiting_on_pid": None, "waiting_on_session": None,
        "waiting_until": 0, "waiting_on_delegations": 0,
        "waiting_reason": None, "waiting_since": 0,
        "contract": {"outcome": "", "verification": "", "constraints": "",
                     "boundaries": "", "stop_when": ""},
        "gates": [],
    }
    d.update(kw)
    return d


def MGR(state=None, sid="s1", dmt=20):
    return {"manager": {"session_id": sid, "default_max_turns": dmt, "_state": state}}


def GATE(cmd="make test", **kw):
    g = {"command": cmd, "timeout_seconds": 300, "max_retries": 3,
         "attempts": 0, "last_exit_code": None, "last_output_tail": ""}
    g.update(kw)
    return g


KICK = "[Goal set] Continue with the goal you were just given; there is no need to re-read it."

# ── _decision ────────────────────────────────────────────────────────────
case("decision", status="active", should_continue=True, prompt="go on",
     verdict="continue", reason="r", message="m")
case("decision", status=None, should_continue=False, prompt=None,
     verdict="inactive", reason="no active goal", message="")
case("decision", status="paused", should_continue=False, prompt=None,
     verdict="blocked", reason="unachievable", message="paused")
case("decision", status="done", should_continue=False, prompt=None,
     verdict="done", reason="finished", message="achieved")
case("decision", status="active", should_continue=False, prompt=None,
     verdict="waiting", reason="pid 5", message="parked")
case("decision", status="", should_continue=True, prompt="", verdict="",
     reason="", message="")

# ── _render_extra_criteria ───────────────────────────────────────────────
case("render_extra_criteria", subgoals=[])
case("render_extra_criteria", subgoals=["alpha"])
case("render_extra_criteria", subgoals=["alpha", "beta"])
case("render_extra_criteria", subgoals=["", "x"])
case("render_extra_criteria", subgoals=[5, True, None])
case("render_extra_criteria", subgoals=None)
case("render_extra_criteria", subgoals="abc")

# ── goal_kick_prompt ─────────────────────────────────────────────────────
case("goal_kick_prompt", goal="ship the API", last_user_message="please ship the API today")
case("goal_kick_prompt", goal="A" * 400, last_user_message="A" * 400)
case("goal_kick_prompt", goal="A" * 400, last_user_message="A" * 400 + "B" * 100)   # share 0.8
case("goal_kick_prompt", goal="A" * 400, last_user_message="A" * 400 + "B" * 101)   # share < 0.8
case("goal_kick_prompt", goal="A" * 399, last_user_message="A" * 399)
case("goal_kick_prompt", goal="A" * 400, last_user_message="B" * 500)
case("goal_kick_prompt", goal=None, last_user_message="anything")
case("goal_kick_prompt", goal="  a\n b\t c ", last_user_message="x a b c y")
case("goal_kick_prompt", goal="A" * 400,
     last_user_message=[{"text": "A" * 200}, "skip", {"text": "A" * 200}, {}])
case("goal_kick_prompt", goal="A" * 400,
     last_user_message=[{"text": None}, {"text": "A" * 400}])
case("goal_kick_prompt", goal=5, last_user_message=None)

# ── last_user_message_content ────────────────────────────────────────────
case("last_user_message_content", history=[])
case("last_user_message_content", history=None)
case("last_user_message_content", history=[{"role": "user", "content": "hello"}])
case("last_user_message_content", history=[
    {"role": "user", "content": "first"},
    {"role": "assistant", "content": "mid"},
    {"role": "user", "content": "last"}])
case("last_user_message_content", history=[{"role": "assistant", "content": "x"}])
case("last_user_message_content", history=[{"role": "user"}])
case("last_user_message_content", history=[{"role": "user", "content": None}])
case("last_user_message_content", history=[{"role": "user", "content": ["blocks"]}])
case("last_user_message_content", history=5)
case("last_user_message_content", history="abc")
case("last_user_message_content", history={"role": "user"})
case("last_user_message_content", history=[None, "x", {"role": "user", "content": "ok"}])

# ── count_active_delegations ─────────────────────────────────────────────
AD_BASE = {"async_delegation_import": R(None)}
case("count_active_delegations", session_id=None)
case("count_active_delegations", session_id="")
case("count_active_delegations", session_id=0)
case("count_active_delegations", session_id="s1",
     io=dict(AD_BASE, _session_records=R(["a", "b"])))
case("count_active_delegations", session_id="s1", io=dict(AD_BASE, _session_records=R([])))
case("count_active_delegations", session_id="s1", io=dict(AD_BASE, _session_records=R(None)))
case("count_active_delegations", session_id="s1",
     io=dict(AD_BASE, _session_records=X("RuntimeError", "boom")))
case("count_active_delegations", session_id="s1",
     io={"async_delegation_import": IMP("ImportError", "no module")})
case("count_active_delegations", session_id="s1",
     io=dict(AD_BASE, _session_records=X("SystemExit", "1", code=1)))
case("count_active_delegations", session_id="s1",
     io=dict(AD_BASE, _session_records=X("KeyboardInterrupt")))
case("count_active_delegations", session_id="s1", io={})
case("count_active_delegations", session_id=7, io=dict(AD_BASE, _session_records=R({"k": 1})))

# ── last_user_message_from_db ────────────────────────────────────────────
TR_BASE = {"transcript_import": R(None), "_get_session_db": R({"handle": 1})}
WIRE_U = {"role": "user", "content": "from db"}
WIRE_A = {"role": "assistant", "content": "reply"}
case("last_user_message_from_db", session_id=None)
case("last_user_message_from_db", session_id="s1",
     io={"transcript_import": R(None), "_get_session_db": R(None)})
case("last_user_message_from_db", session_id="s1",
     io=dict(TR_BASE, read_transcript=R([1, 2]), as_wire_row=[R(WIRE_A), R(WIRE_U)]))
case("last_user_message_from_db", session_id="s1",
     io=dict(TR_BASE, read_transcript=R([])))
case("last_user_message_from_db", session_id="s1",
     io=dict(TR_BASE, read_transcript=X("RuntimeError", "read failed")))
case("last_user_message_from_db", session_id="s1",
     io=dict(TR_BASE, read_transcript=R([1]), as_wire_row=X("ValueError", "bad row")))
case("last_user_message_from_db", session_id="s1",
     io={"transcript_import": IMP("ImportError", "no module")})
case("last_user_message_from_db", session_id="s1",
     io=dict(TR_BASE, read_transcript=R([1]), as_wire_row=X("SystemExit", "1", code=1)))
case("last_user_message_from_db", session_id="s1", io={})
case("last_user_message_from_db", session_id="s1",
     io=dict(TR_BASE, read_transcript=R(None)))
case("last_user_message_from_db", session_id="s1",
     io=dict(TR_BASE, read_transcript=R([1, 2]), as_wire_row=[R(WIRE_U), R(WIRE_A)]))

# ── _goal_judge_setting ──────────────────────────────────────────────────
def CFG(mt="8192"):
    return {"auxiliary": {"goal_judge": {"max_tokens": mt}}}


GJ_BASE = {"config_import": R(None), "load_config": R(CFG())}
case("goal_judge_setting", key="max_tokens", default=4096, cast="int",
     io=dict(GJ_BASE, cast=R(8192)))
case("goal_judge_setting", key="max_tokens", default=4096, cast="int",
     io=dict(GJ_BASE, cast=R(0)))
case("goal_judge_setting", key="timeout", default=30.0, cast="float",
     io={"config_import": R(None), "load_config": R({"auxiliary": {"goal_judge": {"timeout": "12.5"}}}),
         "cast": R(12.5)})
case("goal_judge_setting", key="max_tokens", default=4096, cast="int",
     io={"config_import": R(None), "load_config": R(None), "cast": R(1)})
case("goal_judge_setting", key="max_tokens", default=4096, cast="int",
     io={"config_import": R(None), "load_config": R({"auxiliary": 5}), "cast": R(1)})
case("goal_judge_setting", key="max_tokens", default=4096, cast="int",
     io={"config_import": R(None), "load_config": R({"auxiliary": {"goal_judge": None}}), "cast": R(1)})
case("goal_judge_setting", key="max_tokens", default=4096, cast="int",
     io={"config_import": R(None), "load_config": R({}), "cast": R(4096)})
case("goal_judge_setting", key="max_tokens", default=4096, cast="int",
     io=dict(GJ_BASE, cast=X("ValueError", "bad int")))
case("goal_judge_setting", key="max_tokens", default=4096, cast="int",
     io=dict(GJ_BASE, cast=R("nope")))
case("goal_judge_setting", key="max_tokens", default=4096, cast="int",
     io={"config_import": R(None), "load_config": X("SystemExit", "1", code=1)})
case("goal_judge_setting", key="max_tokens", default=4096, cast="int",
     io={"config_import": IMP("ImportError", "no config")})
case("goal_judge_setting", key="max_tokens", default=4096, cast="int", io={})
case("goal_judge_setting", key="max_tokens", default=4096, cast="int",
     io=dict(GJ_BASE, cast=R(True)))

# ── _call_goal_judge_llm ─────────────────────────────────────────────────
def RESP(content="done"):
    return {"choices": [{"message": {"content": content}}]}


CJ_BASE = {"goal_judge_max_tokens": R(4096)}
case("call_goal_judge_llm", system_prompt="sys", user_prompt="usr", timeout=30.0,
     io=dict(CJ_BASE, call_llm=R(RESP("the verdict"))))
case("call_goal_judge_llm", system_prompt="sys", user_prompt="usr", timeout=None,
     io=dict(CJ_BASE, call_llm=R(RESP(""))))
case("call_goal_judge_llm", system_prompt="sys", user_prompt="usr", timeout=30.0,
     io=dict(CJ_BASE, call_llm=R(RESP(None))))
case("call_goal_judge_llm", system_prompt="sys", user_prompt="usr", timeout=30.0,
     io=dict(CJ_BASE, call_llm=R(None)))
case("call_goal_judge_llm", system_prompt="sys", user_prompt="usr", timeout=30.0,
     io=dict(CJ_BASE, call_llm=R({})))
case("call_goal_judge_llm", system_prompt="sys", user_prompt="usr", timeout=30.0,
     io=dict(CJ_BASE, call_llm=R({"choices": []})))
case("call_goal_judge_llm", system_prompt="sys", user_prompt="usr", timeout=30.0,
     io=dict(CJ_BASE, call_llm=R({"choices": [{}]})))
case("call_goal_judge_llm", system_prompt="sys", user_prompt="usr", timeout=30.0,
     io=dict(CJ_BASE, call_llm=R({"choices": [{"message": None}]})))
case("call_goal_judge_llm", system_prompt="sys", user_prompt="usr", timeout=30.0,
     io=dict(CJ_BASE, call_llm=R({"choices": "ab"})))
case("call_goal_judge_llm", system_prompt="sys", user_prompt="usr", timeout=30.0,
     io=dict(CJ_BASE, call_llm=R(5)))
case("call_goal_judge_llm", system_prompt="sys", user_prompt="usr", timeout=30.0,
     io=dict(CJ_BASE, call_llm=X("RuntimeError", "api down")))
case("call_goal_judge_llm", system_prompt="sys", user_prompt="usr", timeout=30.0,
     io={"goal_judge_max_tokens": X("SystemExit", "1", code=1)})
case("call_goal_judge_llm", system_prompt="sys", user_prompt="usr", timeout=30.0, io={})
case("call_goal_judge_llm", system_prompt="sys", user_prompt="usr", timeout=30.0,
     io=dict(CJ_BASE, call_llm=R(RESP(0))))

# ── _gate_workspace ──────────────────────────────────────────────────────
GW_BASE = {"runtime_cwd_import": R(None)}
REFUSAL = ("the session workspace /gone is not a directory on this host, "
           "and running gates anywhere else would check a different project")
case("gate_workspace", io=dict(GW_BASE, scoped_session_cwd=R("/w"), path_is_dir=R(True)))
case("gate_workspace", io=dict(GW_BASE, scoped_session_cwd=R("/gone"), path_is_dir=R(False)))
case("gate_workspace", io=dict(GW_BASE, scoped_session_cwd=R(None), resolve_agent_cwd=R("/launch")))
case("gate_workspace", io=dict(GW_BASE, scoped_session_cwd=R(""), resolve_agent_cwd=R("/launch")))
case("gate_workspace", io=dict(GW_BASE, scoped_session_cwd=R(None),
                               resolve_agent_cwd=X("OSError", "deleted")))
case("gate_workspace", io=dict(GW_BASE, scoped_session_cwd=R(None),
                               resolve_agent_cwd=X("FileNotFoundError", "gone")))
case("gate_workspace", io=dict(GW_BASE, scoped_session_cwd=R(None),
                               resolve_agent_cwd=X("RuntimeError", "boom")))
case("gate_workspace", io=dict(GW_BASE, scoped_session_cwd=R("/w"),
                               path_is_dir=X("OSError", "probe failed")))
case("gate_workspace", io=dict(GW_BASE, scoped_session_cwd=R(5)))
case("gate_workspace", io={"runtime_cwd_import": IMP("ImportError", "no module")})
case("gate_workspace", io={})
case("gate_workspace", io=dict(GW_BASE, scoped_session_cwd=R(None), resolve_agent_cwd=R(None)))
case("gate_workspace", io=dict(GW_BASE, scoped_session_cwd=R("/w"), path_is_dir=R(1)))

# ── _acquire_session_db ──────────────────────────────────────────────────
PA_BASE = {"persistence_assembly_import": R(None), "profile_scope": R({"home": "/h"})}
case("acquire_session_db", home="/h", io=dict(PA_BASE, open_persistence=R({"db": 1})))
case("acquire_session_db", home="/h", io=dict(PA_BASE, open_persistence=R(None)))
case("acquire_session_db", home="/h",
     io={"persistence_assembly_import": R(None), "profile_scope": X("RuntimeError", "scope failed")})
case("acquire_session_db", home=None, io={"persistence_assembly_import": R(None)})
case("acquire_session_db", home=5, io={"persistence_assembly_import": R(None)})
case("acquire_session_db", home="/h",
     io={"persistence_assembly_import": IMP("ImportError", "no module")})
case("acquire_session_db", home="/h", io={})
case("acquire_session_db", home="", io=dict(PA_BASE, open_persistence=R("handle")))

# ── _release_session_db ──────────────────────────────────────────────────
case("release_session_db", db=None)
case("release_session_db", db={"handle": 1}, io={"db_close": R(None)})
case("release_session_db", db={"handle": 1}, io={"db_close": X("RuntimeError", "close failed")})
case("release_session_db", db={"handle": 1}, io={"db_close": X("SystemExit", "1", code=1)})
case("release_session_db", db={"handle": 1}, io={"db_close": X("KeyboardInterrupt")})
case("release_session_db", db={"handle": 1}, io={})
case("release_session_db", db=5, io={"db_close": R(None)})
case("release_session_db", db={"handle": 1}, io={"db_close": X("GeneratorExit")})

# ── GoalState.clear_wait ─────────────────────────────────────────────────
case("clear_wait", state={"goal_state": GS(waiting_on_pid=123, waiting_reason="watch",
                                           waiting_since=1700000000)})
case("clear_wait", state={"goal_state": GS()})
case("clear_wait", state={"goal_state": {"goal": "g"}})
case("clear_wait", state={"goal_state": GS(waiting_on_session="proc_1", waiting_until=500,
                                           waiting_on_delegations=2, waiting_reason="r",
                                           waiting_since=100, subgoals=["a"],
                                           gates=[GATE()])})
case("clear_wait", state={"goal_state": GS(waiting_until=999, waiting_reason=None)})
case("clear_wait", state={"goal_state": GS(waiting_on_pid=0, waiting_on_session="")})

# ── GoalManager.refresh ──────────────────────────────────────────────────
case("refresh", state=MGR(GS()), io={"load_goal": R(GS(goal="updated", turns_used=9))})
case("refresh", state=MGR(GS()), io={"load_goal": R(None)})
case("refresh", state=MGR(None), io={"load_goal": R({"goal": "partial"})})
case("refresh", state=MGR(GS()), io={"load_goal": X("RuntimeError", "db down")})
case("refresh", state=MGR(GS()), io={"load_goal": X("SystemExit", "1", code=1)})
case("refresh", state=MGR(GS()), io={})
case("refresh", state=MGR(GS()), io={"load_goal": R(5)})

# ── GoalManager._save ────────────────────────────────────────────────────
case("save", state=MGR(GS()), io={"save_goal": R(None)})
case("save", state=MGR(None), io={"save_goal": R(None)})
case("save", state=MGR(GS()), io={"save_goal": X("RuntimeError", "write failed")})
case("save", state=MGR(GS()), io={"save_goal": X("SystemExit", "1", code=1)})
case("save", state=MGR(GS()), io={})
case("save", state=MGR(GS(turns_used=7)), io={"save_goal": R(None)})

# ── GoalManager._require_goal ────────────────────────────────────────────
case("require_goal", state=MGR(None))
case("require_goal", state=MGR(GS()), io={"has_goal": R(True)})
case("require_goal", state=MGR(GS(status="paused")), io={"has_goal": R(True)})
case("require_goal", state=MGR(GS(status="cleared")), io={"has_goal": R(False)})
case("require_goal", state=MGR(GS()), io={"has_goal": X("RuntimeError", "boom")})
case("require_goal", state=MGR(GS()), io={"has_goal": X("SystemExit", "1", code=1)})
case("require_goal", state=MGR(GS()), io={})
case("require_goal", state=MGR(GS()), io={"has_goal": R(1)})

# ── GoalManager._require_active ──────────────────────────────────────────
case("require_active", state=MGR(GS()))
case("require_active", state=MGR(GS(status="paused")))
case("require_active", state=MGR(None))
case("require_active", state=MGR(GS(status="done")))
case("require_active", state=MGR(GS(status=5)))
case("require_active", state=MGR(GS(status="cleared")))

# ── GoalManager._pause_state ─────────────────────────────────────────────
case("pause_state", reason="turn budget exhausted", state=MGR(GS()), io={"save_goal": R(None)})
case("pause_state", reason=None, state=MGR(GS()), io={"save_goal": R(None)})
case("pause_state", reason="user-paused", state=MGR(None))
case("pause_state", reason="x", state=MGR(GS()), io={"save_goal": X("RuntimeError", "no db")})
case("pause_state", reason="x", state=MGR(GS()), io={"save_goal": X("SystemExit", "1", code=1)})
case("pause_state", reason="x", state=MGR(GS()), io={})

# ── GoalManager._pause_decision ──────────────────────────────────────────
case("pause_decision", paused_reason="budget", verdict="continue", reason="r",
     message="paused msg", state=MGR(GS()), io={"save_goal": R(None)})
case("pause_decision", paused_reason="judged unachievable: r", verdict="blocked",
     reason="impossible", message="no", state=MGR(GS(status="active")), io={"save_goal": R(None)})
case("pause_decision", paused_reason="p", verdict="gate_failed", reason="g",
     message="m", state=MGR(None))
case("pause_decision", paused_reason="p", verdict="continue", reason="r",
     message="m", state=MGR(GS()), io={"save_goal": X("KeyboardInterrupt")})
case("pause_decision", paused_reason="p", verdict="continue", reason="r",
     message="m", state=MGR(GS()), io={})

# ── GoalManager._pop_item ────────────────────────────────────────────────
POP_STATE = MGR(GS(subgoals=["a", "b", "c"]))
POP_IO = {"has_goal": R(True), "save_goal": R(None)}
case("pop_item", attr="subgoals", index=2, state=POP_STATE, io=POP_IO)
case("pop_item", attr="subgoals", index=1, state=POP_STATE, io=POP_IO)
case("pop_item", attr="subgoals", index=3, state=POP_STATE, io=POP_IO)
case("pop_item", attr="subgoals", index=0, state=POP_STATE, io=POP_IO)
case("pop_item", attr="subgoals", index=4, state=POP_STATE, io=POP_IO)
case("pop_item", attr="subgoals", index=-1, state=POP_STATE, io=POP_IO)
case("pop_item", attr="subgoals", index="2", state=POP_STATE, io=POP_IO)
case("pop_item", attr="subgoals", index=None, state=POP_STATE, io=POP_IO)
case("pop_item", attr="subgoals", index=True, state=POP_STATE, io=POP_IO)
case("pop_item", attr="gates", index=1,
     state=MGR(GS(gates=[GATE("make test"), GATE("make lint")])), io=POP_IO)
case("pop_item", attr="bogus", index=1, state=POP_STATE, io=POP_IO)
case("pop_item", attr="subgoals", index=1, state=MGR(GS()), io={"has_goal": R(False)})
case("pop_item", attr="subgoals", index=1, state=MGR(None))
case("pop_item", attr="subgoals", index=1, state=POP_STATE,
     io={"has_goal": R(True), "save_goal": X("RuntimeError", "no db")})
case("pop_item", attr="subgoals", index="1_0", state=MGR(GS(subgoals=[str(i) for i in range(12)])),
     io=POP_IO)
case("pop_item", attr="subgoals", index="abc", state=POP_STATE, io=POP_IO)

# ── GoalManager._clear_items ─────────────────────────────────────────────
case("clear_items", attr="subgoals", state=MGR(GS(subgoals=["a", "b", "c"])), io=POP_IO)
case("clear_items", attr="gates", state=MGR(GS(gates=[GATE(), GATE("lint")])), io=POP_IO)
case("clear_items", attr="subgoals", state=MGR(GS()), io=POP_IO)
case("clear_items", attr="bogus", state=POP_STATE, io=POP_IO)
case("clear_items", attr="subgoals", state=MGR(GS()), io={"has_goal": R(False)})
case("clear_items", attr="subgoals", state=MGR(None))
case("clear_items", attr="subgoals", state=POP_STATE,
     io={"has_goal": R(True), "save_goal": X("SystemExit", "1", code=1)})

# ── GoalManager._park ────────────────────────────────────────────────────
PARK_IO = {"time_time": R(1700000200), "save_goal": R(None)}
case("park", reason="watch the build", barrier={"waiting_on_pid": 123},
     state=MGR(GS()), io=PARK_IO)
case("park", reason="", barrier={"waiting_until": 1700000500},
     state=MGR(GS()), io=PARK_IO)
case("park", reason="  spaced  ", barrier={"waiting_on_session": "proc_1"},
     state=MGR(GS()), io=PARK_IO)
case("park", reason=None, barrier={"waiting_until": 500, "waiting_on_delegations": 2},
     state=MGR(GS()), io=PARK_IO)
case("park", reason=5, barrier={"waiting_on_pid": 1}, state=MGR(GS()), io=PARK_IO)
case("park", reason="r", barrier={"waiting_on_pid": 1}, state=MGR(GS(status="paused")))
case("park", reason="r", barrier={"waiting_on_pid": 1}, state=MGR(None))
case("park", reason="r", barrier={"waiting_on_pid": 1}, state=MGR(GS()),
     io={"time_time": X("RuntimeError", "clock broke")})
case("park", reason="r", barrier={"waiting_on_pid": 1}, state=MGR(GS()), io={})
case("park", reason="r", barrier={"waiting_on_pid": 9, "waiting_reason": "override"},
     state=MGR(GS(waiting_on_session="old", waiting_until=99, waiting_since=5)), io=PARK_IO)

# ── GoalManager._waiting_decision ────────────────────────────────────────
case("waiting_decision", state=MGR(GS(waiting_on_session="proc_1", waiting_reason="watch")))
case("waiting_decision", state=MGR(GS(waiting_on_session="proc_1")))
case("waiting_decision", state=MGR(GS(waiting_on_pid=123)))
case("waiting_decision", state=MGR(GS(waiting_on_session="", waiting_on_pid=7)))
case("waiting_decision", state=MGR(GS(waiting_until=150)), io={"time_time": R(100)})
case("waiting_decision", state=MGR(GS(waiting_until=50)), io={"time_time": R(100)})
case("waiting_decision", state=MGR(GS(waiting_until=150.9, waiting_reason="cooldown")),
     io={"time_time": R(100.2)})
case("waiting_decision", state=MGR(GS(waiting_until=150)),
     io={"time_time": X("RuntimeError", "clock broke")})
case("waiting_decision", state=MGR(None))
case("waiting_decision", state=MGR(GS(waiting_until="soon")), io={"time_time": R(100)})
case("waiting_decision", state=MGR(GS(waiting_until=150)), io={"time_time": R(None)})
case("waiting_decision", state=MGR(GS(waiting_until=150)), io={})

# ── GoalManager._apply_wait_directive ────────────────────────────────────
AWD_IO = {"wait_on_session": R(GS(waiting_on_session="proc_9")),
          "wait_on": R(GS(waiting_on_pid=42)),
          "wait_for_seconds": R(GS(waiting_until=999))}
case("apply_wait_directive", directive={"session_id": "proc_9"}, reason="judge wait",
     active_delegations=0, state=MGR(GS()), io=AWD_IO)
case("apply_wait_directive", directive={"pid": 42}, reason="judge wait",
     active_delegations=0, state=MGR(GS()), io=AWD_IO)
case("apply_wait_directive", directive={"pid": "42"}, reason="r",
     state=MGR(GS()), io=AWD_IO)
case("apply_wait_directive", directive={"pid": 42}, reason="r", state=MGR(GS()),
     io={"wait_on": X("ValueError", "pid is not alive on this host")})
case("apply_wait_directive", directive={"pid": 42}, reason="r", state=MGR(GS()),
     io={"wait_on": X("RuntimeError", "boom")})
case("apply_wait_directive", directive={"pid": 42}, reason="r", state=MGR(GS()),
     io={"wait_on": X("SystemExit", "1", code=1)})
case("apply_wait_directive", directive={"pid": "abc"}, reason="r", state=MGR(GS()), io=AWD_IO)
case("apply_wait_directive", directive={"seconds": 30}, reason="backoff",
     active_delegations=2, state=MGR(GS()), io=AWD_IO)
case("apply_wait_directive", directive={"seconds": 30.5}, reason="backoff",
     state=MGR(GS()), io=AWD_IO)
case("apply_wait_directive", directive={"seconds": "45"}, reason="r",
     state=MGR(GS()), io=AWD_IO)
case("apply_wait_directive", directive={}, reason="r", state=MGR(GS()), io=AWD_IO)
case("apply_wait_directive", directive=None, reason="r", state=MGR(GS()), io=AWD_IO)
case("apply_wait_directive", directive={"session_id": "proc_9"}, reason="r",
     state=MGR(GS()), io={"wait_on_session": R(None)})
case("apply_wait_directive", directive={"session_id": "proc_9"}, reason="r",
     state=MGR(GS()), io={"wait_on_session": R(5)})
case("apply_wait_directive", directive={"session_id": "proc_9"}, reason="r",
     state=MGR(GS()), io={})
case("apply_wait_directive", directive={"session_id": 0, "pid": 0, "seconds": 10},
     reason="r", state=MGR(GS()), io=AWD_IO)
case("apply_wait_directive", directive={"pid": 42}, reason="r", state=MGR(GS()),
     io={"wait_on": X("UnicodeDecodeError", "bad")})

# ── GoalManager._budget_pause ────────────────────────────────────────────
case("budget_pause", verdict="continue", reason="still working", note="",
     state=MGR(GS(turns_used=20, max_turns=20)), io={"save_goal": R(None)})
case("budget_pause", verdict="gate_failed", reason="gate failed (exit 1): $ make",
     note=" (a quality gate is still failing)",
     state=MGR(GS(turns_used=5, max_turns=5)), io={"save_goal": R(None)})
case("budget_pause", verdict="continue", reason="r", note=None,
     state=MGR(GS(turns_used=20)), io={"save_goal": R(None)})
case("budget_pause", verdict="continue", reason="r", note="", state=MGR(None))
case("budget_pause", verdict="continue", reason="r", note="",
     state=MGR(GS()), io={"save_goal": X("RuntimeError", "no db")})
case("budget_pause", verdict="continue", reason="r", note="",
     state=MGR(GS()), io={})
case("budget_pause", verdict="continue", reason="r", note="",
     state=MGR(GS(turns_used=20)), io={"save_goal": X("GeneratorExit")})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"gen_goals_flow: {len(CASES)} cases, {len(counts)} funcs")
    low = {k: v for k, v in counts.items() if v < 5}
    if low:
        print("WARNING funcs with <5 cases:", low)
    print("wrote", OUT)


if __name__ == "__main__":
    main()