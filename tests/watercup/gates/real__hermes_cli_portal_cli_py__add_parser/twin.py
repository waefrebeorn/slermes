"""twin for hermes_cli/portal_cli.py:add_parser (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/portal_cli.py"
    PY_FN = "add_parser"
    C_FN = "add_parser"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
