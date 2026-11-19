"""twin for agent/web_search_registry.py:_keyless_preference (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/web_search_registry.py"
    PY_FN = "_keyless_preference"
    C_FN = "_keyless_preference"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
