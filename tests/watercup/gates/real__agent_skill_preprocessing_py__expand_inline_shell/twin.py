"""twin for agent/skill_preprocessing.py:expand_inline_shell (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_preprocessing.py"
    PY_FN = "expand_inline_shell"
    C_FN = "expand_inline_shell"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
