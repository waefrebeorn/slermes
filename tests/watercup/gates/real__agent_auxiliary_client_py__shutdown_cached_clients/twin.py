"""twin for agent/auxiliary_client.py:shutdown_cached_clients (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "shutdown_cached_clients"
    C_FN = "aux__shutdown_cached_clients"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
