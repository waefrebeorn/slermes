"""twin for agent/title_generator.py:auto_title_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/title_generator.py"
    PY_FN = "auto_title_session"
    C_FN = "auto_title_session"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
