"""twin for tools/vision_tools.py:_build_native_vision_tool_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/vision_tools.py"
    PY_FN = "_build_native_vision_tool_result"
    C_FN = "_build_native_vision_tool_result"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
