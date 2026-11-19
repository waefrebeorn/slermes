"""twin for gateway/slash_commands.py:_gateway_session_origin_for_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/slash_commands.py"
    PY_FN = "_gateway_session_origin_for_id"
    C_FN = "_gateway_session_origin_for_id"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
