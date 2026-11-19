"""twin for gateway/platforms/api_server_room_grants.py:_room_grant_error_response (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server_room_grants.py"
    PY_FN = "_room_grant_error_response"
    C_FN = "_room_grant_error_response"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
