"""twin for gateway/slash_access.py:policy_for_source (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/slash_access.py"
    PY_FN = "policy_for_source"
    C_FN = "policy_for_source"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
