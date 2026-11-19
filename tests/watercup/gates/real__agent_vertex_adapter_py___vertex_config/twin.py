"""twin for agent/vertex_adapter.py:_vertex_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/vertex_adapter.py"
    PY_FN = "_vertex_config"
    C_FN = "_vertex_config"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
