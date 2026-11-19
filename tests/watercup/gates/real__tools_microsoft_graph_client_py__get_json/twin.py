"""twin for tools/microsoft_graph_client.py:get_json (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/microsoft_graph_client.py"
    PY_FN = "get_json"
    C_FN = "get_json"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
