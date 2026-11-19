"""twin for tools/browser_lightpanda.py:_home_candidates (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_lightpanda.py"
    PY_FN = "_home_candidates"
    C_FN = "_home_candidates"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
