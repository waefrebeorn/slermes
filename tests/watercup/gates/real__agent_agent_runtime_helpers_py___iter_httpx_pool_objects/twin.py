"""twin for agent/agent_runtime_helpers.py:_iter_httpx_pool_objects (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "_iter_httpx_pool_objects"
    C_FN = "introspect"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
