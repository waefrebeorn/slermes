"""twin for cli.py:_reload_mcp (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_reload_mcp"
    C_FN = "cmd_reload_mcp"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
