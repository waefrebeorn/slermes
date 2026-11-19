"""twin for agent/prompt_caching.py:strip_anthropic_tool_cache_control (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/prompt_caching.py"
    PY_FN = "strip_anthropic_tool_cache_control"
    C_FN = "strip_anthropic_tool_cache_control"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
