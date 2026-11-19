"""twin for agent/auxiliary_client.py:_resolve_nous_runtime_api (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_resolve_nous_runtime_api"
    C_FN = "_resolve_nous_runtime_api"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
