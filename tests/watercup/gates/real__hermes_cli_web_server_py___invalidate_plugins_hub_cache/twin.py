"""twin for hermes_cli/web_server.py:_invalidate_plugins_hub_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_invalidate_plugins_hub_cache"
    C_FN = "g_topology_cache"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
