"""twin for agent/runtime_cwd.py:clear_session_cwd (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/runtime_cwd.py"
    PY_FN = "clear_session_cwd"
    C_FN = "rcw_clear_session_cwd"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
