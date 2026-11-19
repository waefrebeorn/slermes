"""twin for agent/proxy_sources/iron_proxy.py:_detect_docker_bridge_ip (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/proxy_sources/iron_proxy.py"
    PY_FN = "_detect_docker_bridge_ip"
    C_FN = "_detect_docker_bridge_ip"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
