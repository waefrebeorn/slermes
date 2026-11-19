"""twin for gateway/platforms/qqbot/utils.py:get_api_headers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/qqbot/utils.py"
    PY_FN = "get_api_headers"
    C_FN = "get_api_headers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
