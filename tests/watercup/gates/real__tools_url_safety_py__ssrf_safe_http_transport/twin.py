"""twin for tools/url_safety.py:ssrf_safe_http_transport (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/url_safety.py"
    PY_FN = "ssrf_safe_http_transport"
    C_FN = "ssrf_safe_http_transport"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
