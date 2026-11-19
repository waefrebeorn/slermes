"""twin for agent/proxy_sources/iron_proxy.py:_build_proxy_subprocess_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/proxy_sources/iron_proxy.py"
    PY_FN = "_build_proxy_subprocess_env"
    C_FN = "_build_proxy_subprocess_env"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
