"""twin for gateway/platforms/api_server_room_grants.py:_room_grant_claims (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server_room_grants.py"
    PY_FN = "_room_grant_claims"
    C_FN = "_room_grant_claims"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
