"""twin for agent/plugin_llm.py:_normalize_ref (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/plugin_llm.py"
    PY_FN = "_normalize_ref"
    C_FN = "_normalize_ref"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
