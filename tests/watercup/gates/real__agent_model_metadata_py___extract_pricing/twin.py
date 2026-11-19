"""twin for agent/model_metadata.py:_extract_pricing (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"
    PY_FN = "_extract_pricing"
    C_FN = "_extract_pricing"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
