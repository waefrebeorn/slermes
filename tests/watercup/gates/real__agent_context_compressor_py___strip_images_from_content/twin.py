"""twin for agent/context_compressor.py:_strip_images_from_content (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_strip_images_from_content"
    C_FN = "_strip_images_from_content"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
