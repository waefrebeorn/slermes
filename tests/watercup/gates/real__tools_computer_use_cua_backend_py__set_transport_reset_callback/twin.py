"""twin for tools/computer_use/cua_backend.py:set_transport_reset_callback (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/cua_backend.py"
    PY_FN = "set_transport_reset_callback"
    C_FN = "set_transport_reset_callback"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
