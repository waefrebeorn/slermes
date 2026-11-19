"""twin for tools/url_safety.py:_install_ssrf_guard_on_async_client (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/url_safety.py"
    PY_FN = "_install_ssrf_guard_on_async_client"
    C_FN = "_install_ssrf_guard_on_async_client"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
