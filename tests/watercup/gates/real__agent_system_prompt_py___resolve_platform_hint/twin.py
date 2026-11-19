"""twin for agent/system_prompt.py:_resolve_platform_hint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/system_prompt.py"
    PY_FN = "_resolve_platform_hint"
    C_FN = "_resolve_platform_hint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
