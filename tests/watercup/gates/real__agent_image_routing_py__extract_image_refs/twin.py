"""twin for agent/image_routing.py:extract_image_refs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/image_routing.py"
    PY_FN = "extract_image_refs"
    C_FN = "extract_image_refs"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
