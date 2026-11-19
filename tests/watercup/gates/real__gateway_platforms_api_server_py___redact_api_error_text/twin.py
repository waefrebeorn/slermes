"""twin for gateway/platforms/api_server.py:_redact_api_error_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_redact_api_error_text"
    C_FN = "_redact_api_error_text"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
