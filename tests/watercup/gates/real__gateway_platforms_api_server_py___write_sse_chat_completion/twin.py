"""twin for gateway/platforms/api_server.py:_write_sse_chat_completion (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_write_sse_chat_completion"
    C_FN = "_write_sse_chat_completion"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
