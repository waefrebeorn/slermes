"""twin for hermes_cli/plugins.py:plugin_resolve_pre_tool_block (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "plugin_resolve_pre_tool_block"
    C_FN = "plug_resolve_pre_tool_block"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
