"""twin for agent/proxy_sources/iron_proxy.py:ensure_management_token (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/proxy_sources/iron_proxy.py"
    PY_FN = "ensure_management_token"
    C_FN = "ensure_management_token"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
