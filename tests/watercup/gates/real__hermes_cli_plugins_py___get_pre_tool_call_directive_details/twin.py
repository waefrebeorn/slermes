"""twin for hermes_cli/plugins.py:_get_pre_tool_call_directive_details (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "_get_pre_tool_call_directive_details"
    C_FN = "_get_pre_tool_call_directive_details"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
