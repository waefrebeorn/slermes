"""twin for agent/bounded_response.py:_safe_close (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bounded_response.py"
    PY_FN = "_safe_close"
    C_FN = "agent_bounded_response_u_safe_close"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
