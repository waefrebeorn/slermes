"""twin for agent/learning_mutations.py:_edit_memory (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/learning_mutations.py"
    PY_FN = "_edit_memory"
    C_FN = "_edit_memory"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
