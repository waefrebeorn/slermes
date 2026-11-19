"""twin for agent/learning_graph_render.py:_node_meta (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/learning_graph_render.py"
    PY_FN = "_node_meta"
    C_FN = "_node_meta"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
