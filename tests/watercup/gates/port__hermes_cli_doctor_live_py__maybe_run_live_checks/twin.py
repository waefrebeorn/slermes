"""twin for hermes_cli/doctor_live.py:maybe_run_live_checks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/doctor_live.py"
    PY_FN = "maybe_run_live_checks"
    C_FN = "doctor_maybe_run_live_checks"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
