"""twin for agent/auxiliary_client.py:_auth_refresh_provider_for_route (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_auth_refresh_provider_for_route"
    C_FN = "_auth_refresh_provider_for_route"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
