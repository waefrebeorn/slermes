"""twin for hermes_cli/foreign_sessions.py:run_sessions_import (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/foreign_sessions.py"
    PY_FN = "run_sessions_import"
    C_FN = "run_sessions_import"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
