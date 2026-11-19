"""twin for tools/image_generation_tool.py:_looks_like_absolute_file_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/image_generation_tool.py"
    PY_FN = "_looks_like_absolute_file_path"
    C_FN = "image_gen_path_looks_like_absolute_file_path"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
