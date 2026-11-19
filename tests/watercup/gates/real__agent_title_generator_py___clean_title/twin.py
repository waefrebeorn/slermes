"""twin for agent/title_generator.py:_clean_title (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/title_generator.py"
    PY_FN = "_clean_title"
    C_FN = "_clean_title"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
