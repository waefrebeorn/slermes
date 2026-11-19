"""twin for gateway/control_socket.py:cleanup_files (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/control_socket.py"
    PY_FN = "cleanup_files"
    C_FN = "cleanup_files"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
