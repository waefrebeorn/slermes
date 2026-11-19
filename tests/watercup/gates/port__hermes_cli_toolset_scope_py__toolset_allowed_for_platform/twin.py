"""twin for hermes_cli/toolset_scope.py:toolset_allowed_for_platform (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/toolset_scope.py"
    PY_FN = "toolset_allowed_for_platform"
    C_FN = "toolset_allowed_for_platform"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
