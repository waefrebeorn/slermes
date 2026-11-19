"""twin for agent/azure_identity_adapter.py:materialize_bearer_for_http (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/azure_identity_adapter.py"
    PY_FN = "materialize_bearer_for_http"
    C_FN = "materialize_bearer_for_http"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
