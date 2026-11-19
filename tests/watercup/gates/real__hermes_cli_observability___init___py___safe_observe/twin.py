"""twin for hermes_cli/observability/__init__.py:_safe_observe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/__init__.py"
    PY_FN = "_safe_observe"
    C_FN = "obs_safe_observe"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
