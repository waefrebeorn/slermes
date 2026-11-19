"""twin for hermes_cli/web_server.py:_collect_profile_gateway_topology (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_collect_profile_gateway_topology"
    C_FN = "_collect_profile_gateway_topology"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
