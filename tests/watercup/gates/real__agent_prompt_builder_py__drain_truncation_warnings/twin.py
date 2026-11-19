"""twin for agent/prompt_builder.py:drain_truncation_warnings (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/prompt_builder.py"
    PY_FN = "drain_truncation_warnings"
    C_FN = "drain_truncation_warnings"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
