#!/usr/bin/env python3
"""Generator for tests/fixtures/tui_methods_goals_flow.jsonl.

Emits >=5 cases per ported func for the TUI /goal command methods FLOW port
(hermes_cli/goal_command.py + the unclaimed hermes_cli/goals.py gate/contract
manager leaves). Cases carry the mgr state carrier, the args, and the scripted
seam results (in.io) the universal seam consumes on both sides.

Domain rules (shared with the port header):
- parse_contract / gate_workspace / run_gate results are LISTS, so they are
  scripted one level deeper (bare array == per-call queue).
- pause scripts are {null, NON-EMPTY record} (the live `if state` truthiness
  site); resume/set may use {} (the `is None` / required-attr sites).
- state/contract/gate doubles carry every field the flows read; one case per
  family deliberately drops a field for the AttributeError branch.
- fail_closed cases omit a script on purpose (recorded RuntimeError).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "tui_methods_goals_flow.jsonl")


def contract(**kw):
    c = {"outcome": "", "verification": "", "constraints": "",
         "boundaries": "", "stop_when": ""}
    c.update(kw)
    return c


def gate(command="make test", **kw):
    g = {"command": command, "timeout_seconds": 300, "max_retries": 3,
         "attempts": 0, "last_exit_code": None, "last_output_tail": ""}
    g.update(kw)
    return g


def state(**kw):
    s = {"goal": "ship the API", "status": "active", "turns_used": 2,
         "max_turns": 20, "created_at": 0, "last_turn_at": 0,
         "last_verdict": None, "last_reason": None, "paused_reason": None,
         "consecutive_parse_failures": 0, "consecutive_transport_failures": 0,
         "subgoals": [], "waiting_on_pid": None, "waiting_on_session": None,
         "waiting_until": 0, "waiting_on_delegations": 0, "waiting_reason": None,
         "waiting_since": 0, "contract": contract(), "gates": []}
    s.update(kw)
    return s


def mgr(st="default", **kw):
    if st == "default":
        st = state()
    m = {"session_id": "s1", "_state": st}
    m.update(kw)
    return m


def sc(ret=None, raise_=None):
    d = {"__script__": True}
    if raise_ is not None:
        d["raise"] = raise_
    else:
        d["ret"] = ret
    return d


def rs(type_, msg):
    return {"type": type_, "message": msg}


CASES = []

_DEFAULT = object()


def case(func, args=None, io=None, mgr_=_DEFAULT, fail_closed=False):
    if mgr_ is _DEFAULT:
        mgr_ = mgr()
    inp = {"args": args or {}, "state": {"mgr": mgr_}, "io": io or {}}
    c = {"func": func, "in": inp}
    if fail_closed:
        c["fail_closed"] = True
    CASES.append(c)


# -- _english --
case("_english", {"key": "k", "default": "No goal set.", "values": {}})
case("_english", {"key": "k", "default": "\u23f8 Goal paused: {goal}",
                  "values": {"goal": "ship"}})
case("_english", {"key": "k", "default": "\u2299 Goal set ({budget}-turn budget): {goal}",
                  "values": {"budget": 20, "goal": "x"}})
case("_english", {"key": "k", "default": "Invalid goal: {error}",
                  "values": {"error": "boom"}})
case("_english", {"key": "k", "default": "hello {missing}", "values": {}})
case("_english", {"key": "k", "default": None, "values": {}})
case("_english", {"key": "k", "default": "{{literal}} {x}", "values": {"x": 1}})
case("_english", {"key": "k", "default": "bad } brace", "values": {}})

# -- _status --
case("_status", {}, {"status_line": "\u2299 Goal (active, 2/20 turns): ship the API"})
case("_status", {}, {"status_line": [sc(raise_=rs("RuntimeError", "db down"))]})
case("_status", {}, {}, fail_closed=True)
case("_status", {}, {"status_line": ["a", "b"]})
case("_status", {}, {"status_line": sc(raise_=rs("SystemExit", "bye"))})
case("_status", {}, {}, mgr_=None)

# -- _show --
case("_show", {}, {"status_line": "S", "state_has_contract": False})
case("_show", {}, {"status_line": "S", "state_has_contract": True,
                   "contract_render_block": "- Outcome: done"},
     mgr_=mgr(state(contract=contract(outcome="done"))))
case("_show", {}, {"status_line": "S"}, mgr_=mgr(None))
_st = state(); del _st["contract"]
case("_show", {}, {"status_line": "S", "state_has_contract": True}, mgr_=mgr(_st))
case("_show", {}, {"status_line": "S"}, fail_closed=True)
case("_show", {}, {"status_line": sc(raise_=rs("ValueError", "x"))})

# -- _pause --
case("_pause", {}, {"pause": state(status="paused", paused_reason="user-paused"),
                    "render": "\u23f8 Goal paused: ship the API"})
case("_pause", {}, {"pause": None, "render": "No goal set."})
case("_pause", {}, {"pause": state(), "render": sc(raise_=rs("RuntimeError", "i18n down"))})
case("_pause", {}, {"pause": state()}, fail_closed=True)
case("_pause", {}, {"pause": sc(raise_=rs("ValueError", "nope"))})
_st = state(); del _st["goal"]
case("_pause", {}, {"pause": _st, "render": "out"}, mgr_=mgr())

# -- _resume --
case("_resume", {}, {"resume": state(), "render": "\u25b6 Goal resumed: ship the API",
                     "next_continuation_prompt": "keep going"})
case("_resume", {}, {"resume": None, "render": "No goal to resume."})
case("_resume", {}, {"resume": {}})
case("_resume", {}, {"resume": state(), "render": "out"}, fail_closed=True)
case("_resume", {}, {"resume": sc(raise_=rs("RuntimeError", "x"))})
case("_resume", {}, {"resume": state(), "render": sc(raise_=rs("SystemExit", "1"))})

# -- _clear --
case("_clear", {}, {"has_goal": True, "save": None, "render": "\u2713 Goal cleared."})
case("_clear", {}, {"has_goal": False, "save": None, "render": "No active goal."})
case("_clear", {}, {"has_goal": False, "render": "No active goal."}, mgr_=mgr(None))
case("_clear", {}, {"has_goal": True, "save": sc(raise_=rs("OSError", "disk"))})
case("_clear", {}, {"has_goal": True}, fail_closed=True)
case("_clear", {}, {"has_goal": sc(raise_=rs("RuntimeError", "x"))})

# -- _unwait --
case("_unwait", {}, {"stop_waiting": True})
case("_unwait", {}, {"stop_waiting": False})
case("_unwait", {}, {"stop_waiting": sc(raise_=rs("RuntimeError", "x"))})
case("_unwait", {}, {}, fail_closed=True)
case("_unwait", {}, {"stop_waiting": [True, False]})
case("_unwait", {}, {"stop_waiting": 1})

# -- _wait --
case("_wait", {"arg": ""}, {})
case("_wait", {"arg": "abc"}, {})
case("_wait", {"arg": "123"}, {"wait_on": state(waiting_on_pid=123)})
case("_wait", {"arg": "123 fixing tests"}, {"wait_on": state()})
case("_wait", {"arg": "0"},
     {"wait_on": sc(raise_=rs("ValueError", "pid must be a positive integer"))})
case("_wait", {"arg": "  "}, {})
case("_wait", {"arg": "5"}, {}, fail_closed=True)
case("_wait", {"arg": "1_0"}, {"wait_on": state()})
case("_wait", {"arg": "+7 reason"}, {"wait_on": state()})

# -- _gate_add --
case("_gate_add", {"arg": "make test"}, {"require_goal": state(gates=[]), "save": None})
case("_gate_add", {"arg": "   "}, {"require_goal": state()})
case("_gate_add", {"arg": "x"}, {"require_goal": sc(raise_=rs("RuntimeError", "no active goal"))})
case("_gate_add", {"arg": "x"}, {"require_goal": state()}, fail_closed=True)
case("_gate_add", {"arg": "x"}, {"require_goal": state(), "save": sc(raise_=rs("OSError", "disk"))})
_st = state(); del _st["gates"]
case("_gate_add", {"arg": "x"}, {"require_goal": _st, "save": None})

# -- _gate_remove --
case("_gate_remove", {"arg": "1"}, {"pop_item": gate(command="make test")})
case("_gate_remove", {"arg": "abc"}, {})
case("_gate_remove", {"arg": "9"},
     {"pop_item": sc(raise_=rs("IndexError", "index out of range (1..2)"))})
case("_gate_remove", {"arg": "1"}, {"pop_item": None})
case("_gate_remove", {"arg": "1"}, {}, fail_closed=True)
case("_gate_remove", {"arg": "2"}, {"pop_item": gate(command="lint")})

# -- _gate_clear --
case("_gate_clear", {}, {"clear_items": 2})
case("_gate_clear", {}, {"clear_items": 1})
case("_gate_clear", {}, {"clear_items": 0})
case("_gate_clear", {}, {"clear_items": True})
case("_gate_clear", {}, {"clear_items": sc(raise_=rs("RuntimeError", "x"))})
case("_gate_clear", {}, {}, fail_closed=True)

# -- _bare_control --
case("_bare_control", {"verb": "resume", "rest": "last goal"},
     {"resume": state(), "render": "\u25b6 Goal resumed: ship the API",
      "next_continuation_prompt": "p"})
case("_bare_control", {"verb": "pause", "rest": ""},
     {"pause": state(status="paused"), "render": "\u23f8 Goal paused: ship the API"})
case("_bare_control", {"verb": "status", "rest": "x"}, {"status_line": "S"})
case("_bare_control", {"verb": "bogus", "rest": ""}, {})
case("_bare_control", {"verb": "unwait", "rest": "it's"}, {"stop_waiting": False})
case("_bare_control", {"verb": "show", "rest": "r"},
     {"status_line": "S", "state_has_contract": False})
case("_bare_control", {"verb": "resume", "rest": "x"},
     {"resume": state(), "render": None, "next_continuation_prompt": "p"})

# -- _gate --
case("_gate", {"arg": ""}, {})
case("_gate", {"arg": "LIST"}, {})
case("_gate", {"arg": "add make test"},
     {"authorize_gate": None, "require_goal": state(gates=[]), "save": None})
case("_gate", {"arg": "add x"}, {"authorize_gate": "Admins only."})
case("_gate", {"arg": "remove 1"}, {"pop_item": gate()})
case("_gate", {"arg": "rm 1"}, {"pop_item": gate(command="lint")})
case("_gate", {"arg": "clear"}, {"clear_items": 3})
case("_gate", {"arg": "clear extra"}, {})
case("_gate", {"arg": "add"}, {})
case("_gate", {"arg": "bogus x"}, {})
case("_gate", {"arg": "remove abc"}, {})
case("_gate", {"arg": "rm 9"},
     {"pop_item": sc(raise_=rs("IndexError", "index out of range (1..2)"))})
case("_gate", {"arg": "add x"}, {"authorize_gate": sc(raise_=rs("SystemExit", "no"))})
case("_gate", {"arg": "add x"}, {}, fail_closed=True)
case("_gate", {"arg": "  "}, {})
case("_gate", {"arg": "add x"},
     {"authorize_gate": None, "require_goal": sc(raise_=rs("RuntimeError", "no active goal"))})

# -- _set --
case("_set", {"arg": "ship it\nverify: tests pass"},
     {"parse_contract": [["ship it", contract(verification="tests pass")]],
      "contract_is_empty": False, "has_goal": False,
      "set": state(goal="ship it", contract=contract(verification="tests pass")),
      "render": "\u2299 Goal set (20-turn budget): ship it",
      "state_has_contract": [True, True],
      "contract_render_block": "- Verification: tests pass",
      "goal_kick_prompt": "ship it"})
case("_set", {"arg": "build a house", "drafting": True, "progress": True},
     {"progress": None, "draft_contract": contract(outcome="house built"),
      "has_goal": True,
      "set": state(goal="build a house", contract=contract(outcome="house built")),
      "render": "\u2299 Goal set (20-turn budget): build a house",
      "state_has_contract": [True, True],
      "contract_render_block": "- Outcome: house built",
      "goal_kick_prompt": "build a house"},
     mgr_=mgr(state(goal="old goal")))
case("_set", {"arg": "", "drafting": True}, {})
case("_set", {"arg": "x", "drafting": True, "progress": True},
     {"progress": None, "draft_contract": sc(raise_=rs("RuntimeError", "aux down")),
      "logger_debug": None, "has_goal": False, "set": state(goal="x"),
      "render": "out", "state_has_contract": [False, False],
      "goal_kick_prompt": "x"})
case("_set", {"arg": "y"},
     {"parse_contract": [["y", contract()]], "contract_is_empty": True,
      "has_goal": False, "set": state(goal="y"), "render": "out",
      "state_has_contract": [False, False], "goal_kick_prompt": "y"})
case("_set", {"arg": "z"}, {"parse_contract": [["z", None]]})
case("_set", {"arg": "q"}, {"parse_contract": sc(raise_=rs("ValueError", "bad"))})
case("_set", {"arg": "x", "drafting": True},
     {"draft_contract": sc(raise_=rs("SystemExit", "3"))})
case("_set", {"arg": "same"},
     {"parse_contract": [["same", contract()]], "contract_is_empty": True,
      "has_goal": True, "set": state(goal="same"), "render": "out",
      "state_has_contract": [False, False], "goal_kick_prompt": "same"},
     mgr_=mgr(state(goal="same")))
case("_set", {"arg": "w"}, {}, fail_closed=True)
case("_set", {"arg": "x", "drafting": True},
     {"draft_contract": contract(outcome="o"), "has_goal": False,
      "set": state(goal="x", contract=contract(outcome="o")), "render": "out",
      "state_has_contract": [True, True], "contract_render_block": "- Outcome: o",
      "goal_kick_prompt": "x"})
case("_set", {"arg": "v"},
     {"parse_contract": [["v", contract()]], "contract_is_empty": True,
      "has_goal": True, "set": sc(raise_=rs("ValueError", "goal text is empty"))})

# -- is_goal_control --
for a in ["", "status", "resume last goal", "wait 123", "gate add x", "ship it",
          "  PAUSE  ", "draft x", "clear", "unwait", "continue"]:
    case("is_goal_control", {"arg": a}, {})
case("is_goal_control", {"arg": None}, {})

# -- dispatch_goal_command --
case("dispatch_goal_command", {"arg": "status"}, {"status_line": "S"})
case("dispatch_goal_command", {"arg": ""}, {"status_line": "S"})
case("dispatch_goal_command", {"arg": "resume last goal"},
     {"resume": state(), "render": "R", "next_continuation_prompt": "p"})
case("dispatch_goal_command", {"arg": "wait 123"}, {"wait_on": state()})
case("dispatch_goal_command", {"arg": "gate list"}, {})
case("dispatch_goal_command", {"arg": "gate add x"},
     {"authorize_gate": None, "require_goal": state(), "save": None})
case("dispatch_goal_command", {"arg": "-- pause the cron"},
     {"parse_contract": [["pause the cron", contract()]], "contract_is_empty": True,
      "has_goal": False, "set": state(goal="pause the cron"), "render": "out",
      "state_has_contract": [False, False], "goal_kick_prompt": "pause the cron"})
case("dispatch_goal_command", {"arg": "draft build a house", "progress": True},
     {"progress": None, "draft_contract": contract(), "has_goal": False,
      "set": state(goal="build a house"), "render": "out",
      "state_has_contract": [False, False], "goal_kick_prompt": "build a house"})
case("dispatch_goal_command", {"arg": "ship it"},
     {"parse_contract": [["ship it", contract()]], "contract_is_empty": True,
      "has_goal": False, "set": state(goal="ship it"), "render": "out",
      "state_has_contract": [False, False], "goal_kick_prompt": "ship it"})
case("dispatch_goal_command", {"arg": "wait abc"}, {})
case("dispatch_goal_command", {"arg": "gate remove abc"}, {})
case("dispatch_goal_command", {"arg": "bogus"},
     {"parse_contract": [["bogus", contract()]], "contract_is_empty": True,
      "has_goal": False, "set": sc(raise_=rs("ValueError", "goal text is empty")),
      "render": "Invalid goal: goal text is empty"})
case("dispatch_goal_command", {"arg": None}, {})
case("dispatch_goal_command", {"arg": "wait 0"},
     {"wait_on": sc(raise_=rs("ValueError", "pid must be a positive integer"))})
case("dispatch_goal_command", {"arg": "status"},
     {"status_line": sc(raise_=rs("SystemExit", "1"))})
case("dispatch_goal_command", {"arg": "gate add x"}, {}, fail_closed=True)
case("dispatch_goal_command", {"arg": "  STATUS  "}, {"status_line": "S"})
case("dispatch_goal_command", {"arg": "bogus"},
     {"parse_contract": sc(raise_=rs("RuntimeError", "x")),
      "render": "Invalid goal: x"})

# -- clear (goals.py leaf) --
case("clear", {}, {"save": None})
case("clear", {}, {}, mgr_=mgr(None))
case("clear", {}, {}, mgr_={"session_id": "s1"})
case("clear", {}, {"save": sc(raise_=rs("OSError", "disk"))})
case("clear", {}, {}, fail_closed=True)
case("clear", {}, {}, mgr_=None)

# -- add_gate (goals.py leaf) --
case("add_gate", {"command": "make test"}, {"require_goal": state(gates=[]), "save": None})
case("add_gate", {"command": ""}, {"require_goal": state()})
case("add_gate", {"command": "x", "timeout_seconds": "30"},
     {"require_goal": state(), "save": None})
case("add_gate", {"command": "x", "timeout_seconds": "abc"}, {"require_goal": state()})
case("add_gate", {"command": "x", "max_retries": 5}, {"require_goal": state(), "save": None})
case("add_gate", {"command": "x"},
     {"require_goal": sc(raise_=rs("RuntimeError", "no active goal"))})
case("add_gate", {"command": "x"}, {"require_goal": state()}, fail_closed=True)
case("add_gate", {"command": "x", "timeout_seconds": "1\x1c"}, {"require_goal": state()})
case("add_gate", {"command": "x", "timeout_seconds": 0},
     {"require_goal": state(), "save": None})
case("add_gate", {"command": "x", "max_retries": "2"},
     {"require_goal": state(), "save": None})

# -- remove_gate (goals.py leaf) --
case("remove_gate", {"index_1based": 1}, {"pop_item": gate()})
case("remove_gate", {"index_1based": 9},
     {"pop_item": sc(raise_=rs("IndexError", "index out of range (1..2)"))})
case("remove_gate", {"index_1based": 1}, {"pop_item": None})
case("remove_gate", {"index_1based": 1}, {}, fail_closed=True)
case("remove_gate", {"index_1based": 2}, {"pop_item": gate(command="lint")})
case("remove_gate", {"index_1based": 1}, {"pop_item": "stritem"})

# -- clear_gates (goals.py leaf) --
case("clear_gates", {}, {"clear_items": 0})
case("clear_gates", {}, {"clear_items": 3})
case("clear_gates", {}, {"clear_items": sc(raise_=rs("RuntimeError", "x"))})
case("clear_gates", {}, {}, fail_closed=True)
case("clear_gates", {}, {"clear_items": [2, 1]})

# -- render_gates (goals.py leaf) --
case("render_gates", {}, {})
case("render_gates", {}, {}, mgr_=mgr(None))
case("render_gates", {}, {}, mgr_=mgr(state(gates=[gate(last_exit_code=0)])))
case("render_gates", {}, {},
     mgr_=mgr(state(gates=[gate(last_exit_code=1, attempts=2)])))
case("render_gates", {}, {},
     mgr_=mgr(state(gates=[gate(last_exit_code=0),
                           gate(command="lint", last_exit_code=2, attempts=1, max_retries=5)])))
_g = gate(); del _g["last_exit_code"]
case("render_gates", {}, {}, mgr_=mgr(state(gates=[_g])))
case("render_gates", {}, {}, mgr_=mgr(state(gates=[gate(last_exit_code=None)])))
case("render_gates", {}, {}, mgr_={"session_id": "s1"})

# -- render_contract (goals.py leaf) --
case("render_contract", {}, {}, mgr_=mgr(None))
case("render_contract", {}, {"state_has_contract": False})
case("render_contract", {},
     {"state_has_contract": True, "contract_render_block": "- Outcome: done"},
     mgr_=mgr(state(contract=contract(outcome="done"))))
_st = state(); del _st["contract"]
case("render_contract", {}, {"state_has_contract": True}, mgr_=mgr(_st))
case("render_contract", {}, {"state_has_contract": sc(raise_=rs("RuntimeError", "x"))})
case("render_contract", {}, {}, fail_closed=True)
case("render_contract", {},
     {"state_has_contract": True, "contract_render_block": sc(raise_=rs("ValueError", "x"))})

# -- wait_on_session (goals.py leaf) --
case("wait_on_session", {"session_id": "watch1"},
     {"require_active": state(), "park": state(waiting_on_session="watch1")})
case("wait_on_session", {"session_id": ""}, {"require_active": state()})
case("wait_on_session", {"session_id": "   "}, {"require_active": state()})
case("wait_on_session", {"session_id": None}, {"require_active": state()})
case("wait_on_session", {"session_id": "w"},
     {"require_active": sc(raise_=rs("RuntimeError", "no active goal to park"))})
case("wait_on_session", {"session_id": "w"},
     {"require_active": state(), "park": sc(raise_=rs("OSError", "disk"))})
case("wait_on_session", {"session_id": "w"}, {"require_active": state()}, fail_closed=True)
case("wait_on_session", {"session_id": "w", "reason": "polling"},
     {"require_active": state(), "park": state()})

# -- _check_gates (goals.py leaf) --
case("_check_gates", {}, {}, mgr_=mgr(None))
case("_check_gates", {}, {})
case("_check_gates", {},
     {"gate_workspace": [["/proj", None]], "run_gate": [[True, 0, "ok"]], "save": None},
     mgr_=mgr(state(gates=[gate()])))
case("_check_gates", {},
     {"gate_workspace": [[None, "workspace gone"]],
      "pause_decision": {"status": "paused"}},
     mgr_=mgr(state(gates=[gate()])))
case("_check_gates", {},
     {"gate_workspace": [["/p", None]], "run_gate": [[False, 1, "boom"]],
      "save": None, "decision": {"status": "active", "should_continue": True}},
     mgr_=mgr(state(gates=[gate()])))
case("_check_gates", {},
     {"gate_workspace": [["/p", None]], "run_gate": [[False, 1, "x"]],
      "pause_decision": {"status": "paused"}},
     mgr_=mgr(state(gates=[gate(attempts=3)])))
case("_check_gates", {},
     {"gate_workspace": [["/p", None]],
      "run_gate": [[True, 0, ""], [False, 2, "err"]],
      "save": None, "decision": {"status": "active"}},
     mgr_=mgr(state(gates=[gate(command="a"), gate(command="b")])))
case("_check_gates", {}, {"gate_workspace": sc(raise_=rs("OSError", "cwd"))},
     mgr_=mgr(state(gates=[gate()])))
case("_check_gates", {}, {}, mgr_=mgr(state(gates=[gate()])), fail_closed=True)
case("_check_gates", {},
     {"gate_workspace": [["/p", None]], "run_gate": [[True, 0]]},
     mgr_=mgr(state(gates=[gate()])))
case("_check_gates", {},
     {"gate_workspace": [["/p", None]], "run_gate": [[False, 1, ""]],
      "save": None, "decision": {"status": "active"}},
     mgr_=mgr(state(gates=[gate()])))
case("_check_gates", {},
     {"gate_workspace": [["/p", None]],
      "run_gate": [[True, 0, "ok"], [True, 0, "ok2"]], "save": None},
     mgr_=mgr(state(gates=[gate(command="a"), gate(command="b")])))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"gen_tui_methods_goals_flow_cases: {len(CASES)} cases -> {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")
    thin = [fn for fn, n in counts.items() if n < 5]
    if thin:
        raise SystemExit(f"THIN COVERAGE: {thin}")


if __name__ == "__main__":
    main()