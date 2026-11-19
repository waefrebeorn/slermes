"""twin for gateway/platforms/weixin.py:_api_post (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/weixin.py"
    PY_FN = "_api_post"
    C_FN = "_api_post"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
