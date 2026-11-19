"""twin for tools/env_probe.py:_resolve_terminal_backend (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/env_probe.py"
    PY_FN = "_resolve_terminal_backend"
    C_FN = "_resolve_terminal_backend"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
