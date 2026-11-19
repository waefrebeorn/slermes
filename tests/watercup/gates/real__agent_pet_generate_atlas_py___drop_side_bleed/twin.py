"""twin for agent/pet/generate/atlas.py:_drop_side_bleed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/generate/atlas.py"
    PY_FN = "_drop_side_bleed"
    C_FN = "_drop_side_bleed"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
