"""twin for hermes_cli/cli_commands_mixin.py:_emit_focus_recovery_line (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cli_commands_mixin.py"
    PY_FN = "_emit_focus_recovery_line"
    C_FN = "ccm_emit_focus_recovery_line"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
