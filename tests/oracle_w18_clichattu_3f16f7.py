#!/usr/bin/env python3
"""Differential oracle for cli_chat_turn_mixin.py:CLIChatTurnMixin._chat_resolve_interrupt."""
import ast
import json
import os
import sys
import types

for _s in ("stdin", "stdout", "stderr"):
    try:
        getattr(sys, _s).reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
_pkg = "hermes_cli"
if _pkg not in sys.modules:
    try:
        import hermes_cli  # noqa: F401
    except Exception:
        _p = types.ModuleType(_pkg); _p.__path__ = [os.path.join(QUARRY, "hermes_cli")]
        sys.modules[_pkg] = _p

SRC = os.path.join(QUARRY, "hermes_cli", "cli_chat_turn_mixin.py")
_tree = ast.parse(open(SRC, encoding="utf-8").read())
_ns = {}
for _cls in _tree.body:
    if isinstance(_cls, ast.ClassDef) and _cls.name == "CLIChatTurnMixin":
        for _item in _cls.body:
            if isinstance(_item, ast.FunctionDef) and _item.name == "_chat_resolve_interrupt":
                exec(compile(ast.Module(body=[_item], type_ignores=[]), SRC, "exec"), _ns)
fn = _ns["_chat_resolve_interrupt"]


class _Agent:
    def __init__(self, req):
        self._interrupt_requested = req
        self.cleared = False

    def clear_interrupt(self):
        self.cleared = True


class _Self:
    pass


class _Turn:
    pass


class _Thread:
    def __init__(self, alive):
        self._alive = alive

    def is_alive(self):
        return self._alive


def compute(r):
    turn = _Turn()
    turn.result = r.get("result")
    agent = _Agent(bool(r.get("interrupt_requested", False)))
    me = _Self()
    me.agent = agent
    res = fn(me, turn, _Thread(bool(r.get("thread_alive", False))),
             r.get("interrupt_msg"), r.get("response", ""))
    return [res[0], bool(res[1])]


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "chat_resolve_interrupt":
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_clichattu_3f16f7: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
