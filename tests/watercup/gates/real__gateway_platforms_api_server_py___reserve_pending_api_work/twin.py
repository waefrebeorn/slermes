"""twin for gateway/platforms/api_server.py:_reserve_pending_api_work (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_reserve_pending_api_work"
    C_FN = "_reserve_pending_api_work"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
