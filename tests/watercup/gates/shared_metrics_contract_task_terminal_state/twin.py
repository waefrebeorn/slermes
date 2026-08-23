"""twin for shared_metrics_contract_task_terminal_state.

Imports LIVE hermes_cli/observability/shared_metrics_contract.py::task_terminal_state
and traces the SAME branch ladder as the C harness over smc_task_terminal_state.
Input encoding: "<reason>,<interrupted 0|1>,<completed 0|1>,<failed 0|1>".
"""
import importlib.util as ilu
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_smc", os.path.join(DEV, "hermes_cli", "observability", "shared_metrics_contract.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_smc"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        reason, i_, c_, f_ = raw.split(",")
        kwargs = {"turn_exit_reason": reason,
                  "interrupted": i_ == "1",
                  "completed": c_ == "1",
                  "failed": f_ == "1"}
        r = str(kwargs.get("turn_exit_reason") or "").strip().lower()
        c = Cup("task_terminal_state")
        if kwargs.get("interrupted") or "interrupt" in r or "cancel" in r:
            c.branch(True)
        else:
            c.branch(False)
            if "timeout" in r or "timed_out" in r:
                c.branch(True)
            else:
                c.branch(False)
                if "max_iterations" in r or "budget_exhausted" in r:
                    c.branch(True)
                else:
                    c.branch(False)
                    if "approval" in r and ("denied" in r or "rejected" in r):
                        c.branch(True)
                    else:
                        c.branch(False)
                        if "guardrail" in r:
                            c.branch(True)
                        else:
                            c.branch(False)
                            if r == "system_aborted":
                                c.branch(True)
                            else:
                                c.branch(False)
                                if kwargs.get("completed") is True:
                                    c.branch(True)
                                else:
                                    c.branch(False)
                                    c.branch(kwargs.get("failed") is True
                                             or (r and r != "unknown"))
        outcome, end_reason, termination = mod.task_terminal_state(kwargs)
        c.valstr('["%s","%s","%s"]' % (outcome, end_reason, termination))
        cups.append(c.finalize())
    return cups
