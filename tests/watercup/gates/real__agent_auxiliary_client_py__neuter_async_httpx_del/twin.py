"""twin for agent/auxiliary_client.py:neuter_async_httpx_del (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "neuter_async_httpx_del"
    C_FN = "aux__neuter_async_httpx_del"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
