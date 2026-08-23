"""twin for kanban_db_classify_worker_exit.

Imports LIVE hermes_cli/kanban_db.py::_classify_worker_exit, seeds its
_recent_worker_exits reap registry, and traces the SAME rung ladder as the C
harness (kanban_workers.c::kdb_classify_worker_exit). Inputs are wait raw
status integers, or "none" for the registry-miss path. pid fixed at 4242.
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_live_kanban_db", os.path.join(DEV, "hermes_cli", "kanban_db.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_kanban_db"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

PID = 4242


def run(inputs):
    cups = []
    registry = mod._recent_worker_exits
    rate_limit = mod.KANBAN_RATE_LIMIT_EXIT_CODE
    for raw in inputs:
        c = Cup("classify_worker_exit")
        if raw == "none":
            registry.pop(PID, None)
            c.branch(True)
            kind, code = mod._classify_worker_exit(PID)
        else:
            c.branch(False)
            status = int(raw)
            registry[PID] = (status, 0.0)
            kind, code = mod._classify_worker_exit(PID)
            # mirror WIFEXITED / WIFSIGNALED ladder for trace parity
            import os as _os
            if _os.WIFEXITED(status):
                c.branch(True)
                code_v = _os.WEXITSTATUS(status)
                if code_v == 0:
                    c.branch(True)
                else:
                    c.branch(False)
                    if code_v == rate_limit:
                        c.branch(True)
                    else:
                        c.branch(False)
            else:
                c.branch(False)
                if _os.WIFSIGNALED(status):
                    c.branch(True)
                else:
                    c.branch(False)
        c.valstr("%s:%d" % (kind, -1 if code is None else code))
        cups.append(c.finalize())
    return cups
