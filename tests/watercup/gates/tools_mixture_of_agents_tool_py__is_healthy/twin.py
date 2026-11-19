"""twin for tools/mixture_of_agents_tool.py:is_healthy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mixture_of_agents_tool.py"
    PY_FN = "is_healthy"
    C_FN = "moa_provider_health_is_healthy"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
