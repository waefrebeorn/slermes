"""twin for kanban_stop_session_called_kanban_terminal.

Imports LIVE agent/kanban_stop.py::session_called_kanban_terminal and traces
the SAME ladder as the C harness. Input encoding: "<messages_json>\\t<tools>".
"""
import importlib.util as ilu
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_kanban_stop", os.path.join(DEV, "agent", "kanban_stop.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_kanban_stop"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

TERMINAL = ["kanban_complete", "kanban_block"]


def run(inputs):
    cups = []
    for raw in inputs:
        if "\t" in raw:
            js, tools_s = raw.split("\t", 1)
            tools = tools_s.split("\t") if tools_s else []
        else:
            js, tools = raw, []
        try:
            messages = json.loads(js) if js else None
        except ValueError:
            messages = None
        c = Cup("session_called_kanban_terminal")
        result = False
        if not messages or not isinstance(messages, list):
            c.branch(True)
        else:
            c.branch(False)
            broken = False
            for msg in messages:
                c.loop()
                if not isinstance(msg, dict):
                    c.branch(False)
                    continue
                c.branch(True)
                role = msg.get("role")
                if role == "assistant":
                    c.branch(True)
                    tcs = msg.get("tool_calls") or []
                    if isinstance(tcs, list) and tcs:
                        c.branch(True)
                        for tc in tcs:
                            c.loop()
                            if tc is None:
                                c.branch(False)
                                continue
                            c.branch(True)
                            fn = tc.get("function") if isinstance(tc, dict) else None
                            name = ""
                            if isinstance(fn, dict):
                                name = str(fn.get("name") or "")
                            elif isinstance(tc, dict):
                                name = str(tc.get("name") or "")
                            hit = name in tools
                            c.branch(hit)
                            if hit:
                                result = True
                                break
                    else:
                        c.branch(False)
                elif role == "tool":
                    c.branch(False)
                    c.branch(True)
                    name = str(msg.get("name") or "")
                    hit = name in tools
                    c.branch(hit)
                    if hit:
                        result = True
                else:
                    c.branch(False)
                    c.branch(False)
                if result:
                    break
            if broken:
                pass
        c.branch(result)
        # LIVE value under the same terminal-tool set the port models
        live = mod.session_called_kanban_terminal(messages)
        expected = live
        c.valstr("True" if expected else "False")
        cups.append(c.finalize())
    return cups
