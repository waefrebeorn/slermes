"""twin for tools/url_safety.py:_resolved_http_connect_ips (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/url_safety.py"
    PY_FN = "_resolved_http_connect_ips"
    C_FN = "_resolved_http_connect_ips"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
