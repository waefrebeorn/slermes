"""twin for tools/skill_ledger.py:_is_within (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_ledger.py"
    PY_FN = "_is_within"
    C_FN = "is_within"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
