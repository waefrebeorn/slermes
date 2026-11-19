"""twin for gateway/control_socket.py:_fallback_socket_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/control_socket.py"
    PY_FN = "_fallback_socket_path"
    C_FN = "_fallback_socket_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
