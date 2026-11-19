"""twin for gateway/platforms/api_server_runs.py:_remember_room_retention (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server_runs.py"
    PY_FN = "_remember_room_retention"
    C_FN = "_remember_room_retention"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
