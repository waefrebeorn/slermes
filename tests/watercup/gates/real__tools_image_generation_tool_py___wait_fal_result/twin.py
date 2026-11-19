"""twin for tools/image_generation_tool.py:_wait_fal_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/image_generation_tool.py"
    PY_FN = "_wait_fal_result"
    C_FN = "_wait_fal_result"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
