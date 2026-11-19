"""twin for gateway/platforms/api_server.py:_cors_headers_for_origin (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_cors_headers_for_origin"
    C_FN = "_cors_headers_for_origin"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
