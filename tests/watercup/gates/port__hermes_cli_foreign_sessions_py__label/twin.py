"""twin for hermes_cli/foreign_sessions.py:label (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/foreign_sessions.py"
    PY_FN = "label"
    C_FN = "label"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
