"""twin for hermes_cli/cli_commands_mixin.py:_set_tool_progress_mode (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cli_commands_mixin.py"
    PY_FN = "_set_tool_progress_mode"
    C_FN = "ccm_set_tool_progress_mode"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
