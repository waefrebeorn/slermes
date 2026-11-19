"""twin for agent/runtime_cwd.py:set_session_cwd (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/runtime_cwd.py"
    PY_FN = "set_session_cwd"
    C_FN = "set_session_cwd"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
