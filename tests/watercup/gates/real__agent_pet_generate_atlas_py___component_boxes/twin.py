"""twin for agent/pet/generate/atlas.py:_component_boxes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/generate/atlas.py"
    PY_FN = "_component_boxes"
    C_FN = "_component_boxes"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
