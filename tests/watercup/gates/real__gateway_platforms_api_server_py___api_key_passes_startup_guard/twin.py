"""twin for gateway/platforms/api_server.py:_api_key_passes_startup_guard (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_api_key_passes_startup_guard"
    C_FN = "_api_key_passes_startup_guard"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
