"""twin for agent/anthropic_adapter.py:_model_name_is_kimi_family (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_adapter.py"
    PY_FN = "_model_name_is_kimi_family"
    C_FN = "antr_model_name_is_kimi_family"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
