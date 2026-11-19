"""twin for gateway/slash_access.py:_coerce_id_list (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/slash_access.py"
    PY_FN = "_coerce_id_list"
    C_FN = "_coerce_id_list"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
