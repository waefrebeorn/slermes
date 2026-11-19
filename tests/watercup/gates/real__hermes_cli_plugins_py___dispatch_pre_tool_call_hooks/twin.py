"""twin for hermes_cli/plugins.py:_dispatch_pre_tool_call_hooks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "_dispatch_pre_tool_call_hooks"
    C_FN = "_dispatch_pre_tool_call_hooks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
