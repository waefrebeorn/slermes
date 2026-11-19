"""twin for gateway/platforms/api_server.py:_request_audit_log_suffix (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_request_audit_log_suffix"
    C_FN = "_request_audit_log_suffix"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
