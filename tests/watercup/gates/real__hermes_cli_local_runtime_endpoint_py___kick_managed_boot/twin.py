"""twin for hermes_cli/local_runtime/endpoint.py:_kick_managed_boot (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/endpoint.py"
    PY_FN = "_kick_managed_boot"
    C_FN = "_kick_managed_boot"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
