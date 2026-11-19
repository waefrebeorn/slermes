"""twin for tools/budget_config.py:resolve_threshold (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/budget_config.py"
    PY_FN = "resolve_threshold"
    C_FN = "tools_budget_config_resolve_threshold"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
