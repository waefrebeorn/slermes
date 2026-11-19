"""twin for gateway/platforms/api_server.py:_session_runtime_request_from_body (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_session_runtime_request_from_body"
    C_FN = "_session_runtime_request_from_body"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
