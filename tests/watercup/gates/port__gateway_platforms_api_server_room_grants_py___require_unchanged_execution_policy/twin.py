"""twin for gateway/platforms/api_server_room_grants.py:_require_unchanged_execution_policy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server_room_grants.py"
    PY_FN = "_require_unchanged_execution_policy"
    C_FN = "_require_unchanged_execution_policy"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
