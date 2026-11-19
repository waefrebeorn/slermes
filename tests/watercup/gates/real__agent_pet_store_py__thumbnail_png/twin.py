"""twin for agent/pet/store.py:thumbnail_png (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/store.py"
    PY_FN = "thumbnail_png"
    C_FN = "thumbnail_png"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
