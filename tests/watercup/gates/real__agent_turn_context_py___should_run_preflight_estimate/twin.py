"""twin for agent/turn_context.py:_should_run_preflight_estimate (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/turn_context.py"
    PY_FN = "_should_run_preflight_estimate"
    C_FN = "_should_run_preflight_estimate"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
