"""twin for gateway/platforms/base.py:proxy_kwargs_for_aiohttp (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "proxy_kwargs_for_aiohttp"
    C_FN = "proxy_kwargs_for_aiohttp"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
