"""twin for agent/learning_graph_render.py:_period_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/learning_graph_render.py"
    PY_FN = "_period_key"
    C_FN = "_period_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
