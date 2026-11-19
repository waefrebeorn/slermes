"""twin for gateway/readiness.py:collect_runtime_readiness (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/readiness.py"
    PY_FN = "collect_runtime_readiness"
    C_FN = "gateway_readiness_collect_runtime_readiness"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
