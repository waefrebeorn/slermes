"""twin for hermes_cli/terminal_breadcrumbs.py:write_breadcrumb (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/terminal_breadcrumbs.py"
    PY_FN = "write_breadcrumb"
    C_FN = "tbr_write_breadcrumb"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
