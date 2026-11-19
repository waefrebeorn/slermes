"""twin for agent/anthropic_endpoints.py:_requires_bearer_auth (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_endpoints.py"
    PY_FN = "_requires_bearer_auth"
    C_FN = "_requires_bearer_auth"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
