"""twin for tools/microsoft_graph_client.py:_retry_delay (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/microsoft_graph_client.py"
    PY_FN = "_retry_delay"
    C_FN = "_retry_delay"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
