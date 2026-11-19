"""twin for gateway/platforms/_http_client_limits.py:platform_httpx_limits (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/_http_client_limits.py"
    PY_FN = "platform_httpx_limits"
    C_FN = "platform_httpx_limits"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
