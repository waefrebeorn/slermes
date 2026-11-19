"""twin for gateway/platforms/api_server.py:_response_messages_turn_start_index (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_response_messages_turn_start_index"
    C_FN = "_response_messages_turn_start_index"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
