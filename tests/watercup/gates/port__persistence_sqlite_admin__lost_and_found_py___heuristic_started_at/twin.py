"""twin for hermes_cli/session_lost_and_found.py:_heuristic_started_at (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_lost_and_found.py"
    PY_FN = "_heuristic_started_at"
    C_FN = "laf_heuristic_started_at"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
