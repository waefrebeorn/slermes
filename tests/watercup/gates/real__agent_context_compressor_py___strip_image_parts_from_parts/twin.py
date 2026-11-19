"""twin for agent/context_compressor.py:_strip_image_parts_from_parts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_strip_image_parts_from_parts"
    C_FN = "_strip_image_parts_from_parts"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
