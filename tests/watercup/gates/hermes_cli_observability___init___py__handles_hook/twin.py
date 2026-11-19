"""twin for hermes_cli/observability/__init__.py:handles_hook (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/__init__.py"
    PY_FN = "handles_hook"
    C_FN = "obs_handles_hook"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
