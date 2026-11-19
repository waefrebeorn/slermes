"""twin for gateway/control_socket.py:_query_unix_socket (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/control_socket.py"
    PY_FN = "_query_unix_socket"
    C_FN = "_query_unix_socket"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
