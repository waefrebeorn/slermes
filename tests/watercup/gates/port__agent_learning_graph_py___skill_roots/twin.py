"""twin for agent/learning_graph.py:_skill_roots (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/learning_graph.py"
    PY_FN = "_skill_roots"
    C_FN = "_skill_roots"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
