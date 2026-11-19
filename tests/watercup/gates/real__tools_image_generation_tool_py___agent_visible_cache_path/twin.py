"""twin for tools/image_generation_tool.py:_agent_visible_cache_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/image_generation_tool.py"
    PY_FN = "_agent_visible_cache_path"
    C_FN = "_agent_visible_cache_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
