"""twin for hermes_cli/plugins.py:clear_thread_tool_whitelist (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "clear_thread_tool_whitelist"
    C_FN = "plug_clear_thread_tool_whitelist"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
