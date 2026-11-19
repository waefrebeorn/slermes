"""twin for agent/agent_runtime_helpers.py:_iter_httpx_pools_with_owner (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "_iter_httpx_pools_with_owner"
    C_FN = "_iter_httpx_pools_with_owner"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
