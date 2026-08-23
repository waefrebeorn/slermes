"""twin for cron_scheduler_build_job_prompt.

Traces the LIVE cron/scheduler.py::_build_job_prompt with the same rung
order as the C harness, restricted to the pure I/O-free path: prerun script
result always supplied, no context_from, no skills. The downstream prompt
scanner is treated as opaque on both sides (benign prompts pass through).

Input token format: "prompt|has_script|prerun_ok|prerun_output".
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_cron_scheduler", os.path.join(DEV, "cron", "scheduler.py"))
_sched = ilu.module_from_spec(_spec)
sys.modules["wc_live_cron_scheduler"] = _sched
_spec.loader.exec_module(_sched)
bjp = _sched._build_job_prompt


def trace(token):
    c = Cup("build_job_prompt")
    parts = token.split("|")
    prompt = parts[0] if parts else ""
    has_script = len(parts) > 1 and parts[1] == "1"
    ok = len(parts) > 3 and parts[2] == "1"
    out = parts[3] if len(parts) > 3 else ""

    job = {"prompt": prompt}
    if has_script:
        job["script"] = "gate_prerun.sh"
        c.branch(True)                              # B1
        prerun = (ok, out)
        c.branch(ok)                                # B2
        if not ok:
            pass                                    # error block appended LIVE
        elif out:
            c.branch(True)                          # B3
        else:
            c.branch(False)                         # B3 — silent
    else:
        c.branch(False)                             # B1
        prerun = None
    c.branch(False)                                 # B4 context_from
    res = bjp(job, prerun, None)
    if has_script and ok and not out:
        res = None                                  # SILENT
    c.branch(True)                                  # B5 no effective skills
    return res, c.finalize()


def run(inputs):
    cups = []
    for tok in inputs:
        _, cup = trace(tok)
        cups.append(cup)
    return cups
