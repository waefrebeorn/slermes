"""twin for tools/microsoft_graph_auth.py:clear_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/microsoft_graph_auth.py"
    PY_FN = "clear_cache"
    C_FN = "token"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
