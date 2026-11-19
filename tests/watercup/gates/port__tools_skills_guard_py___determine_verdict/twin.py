"""twin for tools/skills_guard.py:_determine_verdict (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_guard.py"
    PY_FN = "_determine_verdict"
    C_FN = "_determine_verdict"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
