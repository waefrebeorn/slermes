"""twin for hermes_cli/models.py:_fast_mode_route_supported (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "_fast_mode_route_supported"
    C_FN = "_fast_mode_route_supported"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
