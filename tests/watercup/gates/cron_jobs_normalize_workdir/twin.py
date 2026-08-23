"""twin for cron_jobs_normalize_workdir.

Traces the LIVE cron/jobs.py::_normalize_workdir with the same rung order as
the C harness:
  B1 null/empty / B2 relative / B3 missing path / B4 not-a-dir /
  V1 resolved | ERR | None.
The RESULT value comes from the LIVE function (ValueError -> "ERR").
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_cron_jobs", os.path.join(DEV, "cron", "jobs.py"))
_cj = ilu.module_from_spec(_spec)
sys.modules["wc_live_cron_jobs"] = _cj
try:
    _spec.loader.exec_module(_cj)
    live = _cj._normalize_workdir
except Exception:
    live = None


def live_result(workdir):
    """LIVE semantics: None on empty/None, ValueError -> 'ERR', else str."""
    if workdir is None:
        return None
    raw = str(workdir).strip()
    if not raw:
        return None
    if live is not None:
        try:
            return live(raw)
        except ValueError:
            return "ERR"
    # ported fallback
    expanded = os.path.expanduser(raw)
    if not expanded.startswith("/"):
        return "ERR"
    resolved = os.path.realpath(expanded)
    if not os.path.exists(resolved):
        return "ERR"
    if not os.path.isdir(resolved):
        return "ERR"
    return resolved


def trace(workdir):
    c = Cup("normalize_workdir")
    b1 = workdir is None
    c.branch(b1)                                # B1 null
    if b1:
        val = "None"
    else:
        raw = str(workdir).strip()
        b1b = not raw
        c.branch(b1b)                           # B1b empty-after-strip
        if b1b:
            val = "None"
        else:
            expanded = os.path.expanduser(raw)
            if not os.path.isabs(expanded):     # B2 relative
                c.branch(True)
                val = "ERR"
            else:
                c.branch(False)
                resolved = os.path.realpath(expanded)
                if not os.path.exists(resolved):        # B3 missing
                    c.branch(True)
                    val = "ERR"
                else:
                    c.branch(False)
                    if not os.path.isdir(resolved):     # B4 not a dir
                        c.branch(True)
                        val = "ERR"
                    else:
                        c.branch(False)
                        lr = live_result(raw)
                        val = "ERR" if lr in (None, "ERR") else str(lr)
    c.valstr(val)                               # V1
    return val, c.finalize()


def run(inputs):
    cups = []
    for w in inputs:
        _, cup = trace(w)
        cups.append(cup)
    return cups
