#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/api_server_turn_boundary.py
(gate w19_turnbound_4998c3).

The two helpers are pure; the only live dependency is the
`agent.turn_context.reanchor_current_turn_user_idx` import inside
response_turn_start_index, which this oracle stubs from the fixture's
`reanchor_idx` (-1 = "no user row found").
"""
import ast
import json
import sys
import types
import __future__

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
SRC = Q + "/gateway/platforms/api_server_turn_boundary.py"
GATE = "w19_turnbound_4998c3"
_ANN = __future__.annotations.compiler_flag

STATE = {"reanchor_idx": -1}


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


for pkg in ("agent",):
    if pkg not in sys.modules:
        _mk(pkg)
_mk("agent.turn_context",
    reanchor_current_turn_user_idx=lambda messages, user_message: STATE["reanchor_idx"])


def extract():
    tree = ast.parse(open(SRC, encoding="utf-8").read())
    g = {"__name__": "turnbound_live"}
    g.update(dict(Any=object, Dict=object, List=object))
    wanted = {"_same_transcript_prefix", "response_turn_start_index"}
    for node in tree.body:
        if isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name) and t.id == "_TRANSCRIPT_IDENTITY_KEYS":
                    exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), g)
        if isinstance(node, ast.FunctionDef) and node.name in wanted:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()


def run_case(r):
    f = r.get("func")
    if f == "same_prefix":
        return G["_same_transcript_prefix"](r.get("agent_messages") or [],
                                            r.get("prefix") or [])
    if f == "start_index":
        STATE["reanchor_idx"] = int(r.get("reanchor_idx", -1))
        result = r.get("result")
        if not isinstance(result, dict):
            result = {}
        return G["response_turn_start_index"](r.get("conversation_history") or [],
                                              r.get("user_message"), result)
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in {"same_prefix", "start_index"}:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:  # noqa: BLE001 - report, do not die
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
