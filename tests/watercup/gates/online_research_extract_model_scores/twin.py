"""twin for tools/online_research.py:_extract_model_scores (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/online_research.py"
    PY_FN = "_extract_model_scores"
    C_FN = "_extract_model_scores"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
