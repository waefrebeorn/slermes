"""twin for gateway/mirror.py:_append_to_sqlite (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/mirror.py"
    PY_FN = "_append_to_sqlite"
    C_FN = "_append_to_sqlite"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
