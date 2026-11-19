"""twin for agent/lsp/protocol.py:make_error_response (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/protocol.py"
    PY_FN = "make_error_response"
    C_FN = "make_error_response"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
