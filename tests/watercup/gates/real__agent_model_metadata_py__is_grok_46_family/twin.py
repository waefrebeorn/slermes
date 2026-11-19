"""twin for agent/model_metadata.py:is_grok_46_family (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"
    PY_FN = "is_grok_46_family"
    C_FN = "is_grok_46_family"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
