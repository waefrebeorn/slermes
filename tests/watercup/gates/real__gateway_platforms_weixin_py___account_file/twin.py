"""twin for gateway/platforms/weixin.py:_account_file (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/weixin.py"
    PY_FN = "_account_file"
    C_FN = "_account_file"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
