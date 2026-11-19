"""twin for hermes_cli/managed_uv.py:resolve_uv (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/managed_uv.py"
    PY_FN = "resolve_uv"
    C_FN = "resolve_uv"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
