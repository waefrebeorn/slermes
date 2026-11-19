"""twin for agent/proxy_sources/iron_proxy.py:configured (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/proxy_sources/iron_proxy.py"
    PY_FN = "configured"
    C_FN = "ipx_configured"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
