"""twin for tools/image_generation_tool.py:_dispatch_to_plugin_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/image_generation_tool.py"
    PY_FN = "_dispatch_to_plugin_provider"
    C_FN = "tools_image_generation_tool_u_dispatch_to_plugin_provider"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
