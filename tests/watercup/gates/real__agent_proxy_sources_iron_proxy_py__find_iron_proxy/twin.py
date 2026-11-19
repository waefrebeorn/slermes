"""twin for agent/proxy_sources/iron_proxy.py:find_iron_proxy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/proxy_sources/iron_proxy.py"
    PY_FN = "find_iron_proxy"
    C_FN = "find_iron_proxy"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
