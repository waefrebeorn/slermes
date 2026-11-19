"""twin for gateway/control_socket.py:windows_pipe_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/control_socket.py"
    PY_FN = "windows_pipe_name"
    C_FN = "windows_pipe_name"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
