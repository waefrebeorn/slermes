"""twin for hermes_cli/mcp_catalog.py:_write_tools_exclude (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/mcp_catalog.py"
    PY_FN = "_write_tools_exclude"
    C_FN = "_write_tools_exclude"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
