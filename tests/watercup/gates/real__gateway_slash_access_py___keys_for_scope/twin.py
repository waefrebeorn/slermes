"""twin for gateway/slash_access.py:_keys_for_scope (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/slash_access.py"
    PY_FN = "_keys_for_scope"
    C_FN = "keys_for_scope"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
