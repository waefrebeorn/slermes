"""twin for agent/learning_graph.py:build_edges (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/learning_graph.py"
    PY_FN = "build_edges"
    C_FN = "build_edges"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
