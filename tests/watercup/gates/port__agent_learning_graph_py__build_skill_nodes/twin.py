"""twin for agent/learning_graph.py:build_skill_nodes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/learning_graph.py"
    PY_FN = "build_skill_nodes"
    C_FN = "build_skill_nodes"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
