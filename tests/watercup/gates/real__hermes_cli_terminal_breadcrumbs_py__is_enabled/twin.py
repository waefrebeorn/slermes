"""twin for hermes_cli/terminal_breadcrumbs.py:is_enabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/terminal_breadcrumbs.py"
    PY_FN = "is_enabled"
    C_FN = "is_enabled"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
