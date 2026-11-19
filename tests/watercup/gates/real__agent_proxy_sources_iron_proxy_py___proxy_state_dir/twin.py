"""twin for agent/proxy_sources/iron_proxy.py:_proxy_state_dir (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/proxy_sources/iron_proxy.py"
    PY_FN = "_proxy_state_dir"
    C_FN = "_proxy_state_dir"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
