"""twin for hermes_cli/goals.py:_session_waiting (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/goals.py"
    PY_FN = "_session_waiting"
    C_FN = "_session_waiting"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
