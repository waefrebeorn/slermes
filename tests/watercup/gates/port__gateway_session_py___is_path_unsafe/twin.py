"""twin for gateway/session.py:_is_path_unsafe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session.py"
    PY_FN = "_is_path_unsafe"
    C_FN = "_is_path_unsafe"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
