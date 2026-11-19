"""twin for agent/learning_mutations.py:parse_node_kind (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/learning_mutations.py"
    PY_FN = "parse_node_kind"
    C_FN = "parse_node_kind"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
