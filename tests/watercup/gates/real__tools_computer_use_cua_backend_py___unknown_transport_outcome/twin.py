"""twin for tools/computer_use/cua_backend.py:_unknown_transport_outcome (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/cua_backend.py"
    PY_FN = "_unknown_transport_outcome"
    C_FN = "_unknown_transport_outcome"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
