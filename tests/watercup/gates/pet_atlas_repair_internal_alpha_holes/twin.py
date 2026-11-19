"""twin for agent/pet/generate/atlas.py:_repair_internal_alpha_holes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/generate/atlas.py"
    PY_FN = "_repair_internal_alpha_holes"
    C_FN = "pet_repair_internal_alpha_holes"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
