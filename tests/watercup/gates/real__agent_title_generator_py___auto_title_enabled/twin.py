"""twin for agent/title_generator.py:_auto_title_enabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/title_generator.py"
    PY_FN = "_auto_title_enabled"
    C_FN = "auto_title_enabled"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
