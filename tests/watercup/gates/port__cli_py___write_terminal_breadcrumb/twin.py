"""twin for cli.py:_write_terminal_breadcrumb (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_write_terminal_breadcrumb"
    C_FN = "hcli_write_terminal_breadcrumb"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
