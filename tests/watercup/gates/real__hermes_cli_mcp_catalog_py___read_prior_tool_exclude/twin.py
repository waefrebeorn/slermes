"""twin for hermes_cli/mcp_catalog.py:_read_prior_tool_exclude (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/mcp_catalog.py"
    PY_FN = "_read_prior_tool_exclude"
    C_FN = "_read_prior_tool_exclude"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
