"""twin for agent/relay_llm.py:_namespace (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_llm.py"
    PY_FN = "_namespace"
    C_FN = "_namespace"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
