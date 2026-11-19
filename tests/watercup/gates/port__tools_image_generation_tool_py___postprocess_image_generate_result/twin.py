"""twin for tools/image_generation_tool.py:_postprocess_image_generate_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/image_generation_tool.py"
    PY_FN = "_postprocess_image_generate_result"
    C_FN = "image_gen_postprocess_image_generate_result"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
