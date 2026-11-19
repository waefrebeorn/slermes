"""twin for agent/prompt_caching.py:envelope_tool_part_cache_markers_supported (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/prompt_caching.py"
    PY_FN = "envelope_tool_part_cache_markers_supported"
    C_FN = "envelope_tool_part_cache_markers_supported"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
