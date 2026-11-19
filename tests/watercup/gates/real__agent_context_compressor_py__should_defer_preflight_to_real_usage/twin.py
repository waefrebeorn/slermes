"""twin for agent/context_compressor.py:should_defer_preflight_to_real_usage (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "should_defer_preflight_to_real_usage"
    C_FN = "cc_should_defer_preflight_to_real_usage"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
