"""twin for hermes_cli/plugins.py:_register_deferred_platform_tools (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "_register_deferred_platform_tools"
    C_FN = "_register_deferred_platform_tools"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
