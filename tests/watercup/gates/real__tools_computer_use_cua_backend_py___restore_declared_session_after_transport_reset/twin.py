"""twin for tools/computer_use/cua_backend.py:_restore_declared_session_after_transport_reset (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/cua_backend.py"
    PY_FN = "_restore_declared_session_after_transport_reset"
    C_FN = "_restore_declared_session_after_transport_reset"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
