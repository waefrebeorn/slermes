"""twin for agent/pet/store.py:_safe_slug (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/store.py"
    PY_FN = "_safe_slug"
    C_FN = "_safe_slug"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
