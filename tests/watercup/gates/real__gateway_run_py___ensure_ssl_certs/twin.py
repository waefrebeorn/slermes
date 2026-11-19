"""twin for gateway/run.py:_ensure_ssl_certs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_ensure_ssl_certs"
    C_FN = "gw_ensure_ssl_certs"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
