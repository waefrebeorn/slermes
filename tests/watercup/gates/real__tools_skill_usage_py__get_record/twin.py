"""twin for tools/skill_usage.py:get_record (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_usage.py"
    PY_FN = "get_record"
    C_FN = "su_get_record"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
