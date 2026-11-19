"""twin for agent/proxy_sources/iron_proxy.py:ensure_ca_cert (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/proxy_sources/iron_proxy.py"
    PY_FN = "ensure_ca_cert"
    C_FN = "ensure_ca_cert"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
