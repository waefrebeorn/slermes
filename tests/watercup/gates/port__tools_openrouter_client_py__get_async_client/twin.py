"""twin for tools/openrouter_client.py:get_async_client (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/openrouter_client.py"
    PY_FN = "get_async_client"
    C_FN = "get_async_client"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
