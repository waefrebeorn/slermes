"""twin for agent/ssl_verify.py:resolve_httpx_verify (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/ssl_verify.py"
    PY_FN = "resolve_httpx_verify"
    C_FN = "resolve_httpx_verify"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
