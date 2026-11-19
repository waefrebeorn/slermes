"""twin for hermes_cli/foreign_sessions.py:gather_foreign_sessions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/foreign_sessions.py"
    PY_FN = "gather_foreign_sessions"
    C_FN = "gather_foreign_sessions"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
