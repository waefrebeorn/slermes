"""twin for tools/skills_guard.py:_prose_modify_re (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_guard.py"
    PY_FN = "_prose_modify_re"
    C_FN = "_prose_modify_re"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
