"""twin for gateway/run.py:_defer_agent_cleanup_until_future_done (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_defer_agent_cleanup_until_future_done"
    C_FN = "grun_u_defer_agent_cleanup_until_future_done"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
