"""twin for gateway/platforms/api_server_room_grants.py:_handle_room_member_grant_revoke (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server_room_grants.py"
    PY_FN = "_handle_room_member_grant_revoke"
    C_FN = "_handle_room_member_grant_revoke"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
