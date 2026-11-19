"""twin for agent/learning_mutations.py:_edit_skill (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/learning_mutations.py"
    PY_FN = "_edit_skill"
    C_FN = "_edit_skill"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
