"""twin for agent/relay_tools.py:_run_awaitable (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_tools.py"
    PY_FN = "_run_awaitable"
    C_FN = "_run_awaitable"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
