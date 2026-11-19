"""twin for agent/pet/generate/atlas.py:_defringe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/generate/atlas.py"
    PY_FN = "_defringe"
    C_FN = "pet_defringe"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
