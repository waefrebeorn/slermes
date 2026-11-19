"""twin for gateway/platforms/api_server.py:_build_response_conversation_history (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_build_response_conversation_history"
    C_FN = "_build_response_conversation_history"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
