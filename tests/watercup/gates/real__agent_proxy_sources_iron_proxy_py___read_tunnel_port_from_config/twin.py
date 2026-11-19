"""twin for agent/proxy_sources/iron_proxy.py:_read_tunnel_port_from_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/proxy_sources/iron_proxy.py"
    PY_FN = "_read_tunnel_port_from_config"
    C_FN = "ipx_read_tunnel_port_from_config"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
