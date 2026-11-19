"""twin for agent/auxiliary_client.py:_force_close_async_httpx (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_force_close_async_httpx"
    C_FN = "aux__force_close_async_httpx"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
