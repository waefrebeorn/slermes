"""twin for gateway/config.py:get_unauthorized_dm_behavior (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/config.py"
    PY_FN = "get_unauthorized_dm_behavior"
    C_FN = "get_unauthorized_dm_behavior"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
