"""twin for hermes_cli/cli_commands_mixin.py:_handle_skin_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cli_commands_mixin.py"
    PY_FN = "_handle_skin_command"
    C_FN = "ccm_handle_skin_command"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
