"""twin for gateway/config.py:get_reset_policy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/config.py"
    PY_FN = "get_reset_policy"
    C_FN = "get_reset_policy"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
