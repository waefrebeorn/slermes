"""twin for agent/runtime_cwd.py:_session_cwd_override (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/runtime_cwd.py"
    PY_FN = "_session_cwd_override"
    C_FN = "_session_cwd_override"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
