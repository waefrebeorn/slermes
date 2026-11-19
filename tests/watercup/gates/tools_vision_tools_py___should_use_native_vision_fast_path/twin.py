"""twin for tools/vision_tools.py:_should_use_native_vision_fast_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/vision_tools.py"
    PY_FN = "_should_use_native_vision_fast_path"
    C_FN = "vi_should_use_native_vision_fast_path"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
