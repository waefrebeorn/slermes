"""twin for agent/vertex_adapter.py:_read_sa_file (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/vertex_adapter.py"
    PY_FN = "_read_sa_file"
    C_FN = "_read_sa_file"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
