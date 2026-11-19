"""twin for agent/vertex_adapter.py:_sa_snapshot (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/vertex_adapter.py"
    PY_FN = "_sa_snapshot"
    C_FN = "_sa_snapshot"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
