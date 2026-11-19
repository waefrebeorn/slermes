"""twin for agent/anthropic_endpoints.py:_is_deepseek_anthropic_endpoint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_endpoints.py"
    PY_FN = "_is_deepseek_anthropic_endpoint"
    C_FN = "_is_deepseek_anthropic_endpoint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
