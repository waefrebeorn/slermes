"""twin for gateway/platforms/api_server.py:_resolve_media_to_data_urls (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_resolve_media_to_data_urls"
    C_FN = "_resolve_media_to_data_urls"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
