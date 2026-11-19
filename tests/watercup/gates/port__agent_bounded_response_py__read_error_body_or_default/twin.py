"""twin for agent/bounded_response.py:read_error_body_or_default (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bounded_response.py"
    PY_FN = "read_error_body_or_default"
    C_FN = "agent_bounded_response_read_error_body_or_default"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
