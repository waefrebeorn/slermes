"""twin for hermes_cli/subcommands/dashboard.py:_configure_serve_parser (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/subcommands/dashboard.py"
    PY_FN = "_configure_serve_parser"
    C_FN = "_configure_serve_parser"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
