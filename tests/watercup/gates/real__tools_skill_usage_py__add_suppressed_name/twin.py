"""twin for tools/skill_usage.py:add_suppressed_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_usage.py"
    PY_FN = "add_suppressed_name"
    C_FN = "su_add_suppressed_name"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
