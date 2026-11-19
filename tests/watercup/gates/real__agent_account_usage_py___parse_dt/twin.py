"""twin for agent/account_usage.py:_parse_dt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/account_usage.py"
    PY_FN = "_parse_dt"
    C_FN = "parse_dt"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
