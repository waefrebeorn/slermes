"""twin for cli.py:_mark_terminal_io_broken (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_mark_terminal_io_broken"
    C_FN = "hcli_mark_terminal_io_broken"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
