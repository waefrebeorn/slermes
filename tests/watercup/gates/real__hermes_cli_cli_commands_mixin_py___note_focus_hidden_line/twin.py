"""twin for hermes_cli/cli_commands_mixin.py:_note_focus_hidden_line (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cli_commands_mixin.py"
    PY_FN = "_note_focus_hidden_line"
    C_FN = "ccm_note_focus_hidden_line"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
