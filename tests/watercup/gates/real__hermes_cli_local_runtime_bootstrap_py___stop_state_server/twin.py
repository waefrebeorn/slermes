"""twin for hermes_cli/local_runtime/bootstrap.py:_stop_state_server (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/bootstrap.py"
    PY_FN = "_stop_state_server"
    C_FN = "_stop_state_server"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
