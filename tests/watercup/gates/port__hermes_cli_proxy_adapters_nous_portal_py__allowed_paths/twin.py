"""twin for hermes_cli/proxy/adapters/nous_portal.py:allowed_paths (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/proxy/adapters/nous_portal.py"
    PY_FN = "allowed_paths"
    C_FN = "allowed_paths"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
