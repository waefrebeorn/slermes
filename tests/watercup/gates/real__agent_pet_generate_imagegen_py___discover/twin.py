"""twin for agent/pet/generate/imagegen.py:_discover (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/generate/imagegen.py"
    PY_FN = "_discover"
    C_FN = "h_sessions_search_discover"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
