"""twin for gateway/platforms/api_server.py:_close_cached_session_dbs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_close_cached_session_dbs"
    C_FN = "_close_cached_session_dbs"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
