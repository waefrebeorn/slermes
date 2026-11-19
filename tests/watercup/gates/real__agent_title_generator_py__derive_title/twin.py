"""twin for agent/title_generator.py:derive_title (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/title_generator.py"
    PY_FN = "derive_title"
    C_FN = "derive_title"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
