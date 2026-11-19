"""twin for agent/anthropic_adapter.py:_get_anthropic_max_output (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_adapter.py"
    PY_FN = "_get_anthropic_max_output"
    C_FN = "antr_get_anthropic_max_output"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
