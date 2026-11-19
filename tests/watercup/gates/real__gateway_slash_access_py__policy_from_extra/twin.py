"""twin for gateway/slash_access.py:policy_from_extra (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/slash_access.py"
    PY_FN = "policy_from_extra"
    C_FN = "policy_from_extra"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
