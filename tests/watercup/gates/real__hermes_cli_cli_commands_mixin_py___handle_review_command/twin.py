"""twin for hermes_cli/cli_commands_mixin.py:_handle_review_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cli_commands_mixin.py"
    PY_FN = "_handle_review_command"
    C_FN = "_handle_review_command"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
