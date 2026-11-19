"""twin for hermes_cli/local_runtime/endpoint.py:_pid_alive (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/endpoint.py"
    PY_FN = "_pid_alive"
    C_FN = "_pid_alive"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
