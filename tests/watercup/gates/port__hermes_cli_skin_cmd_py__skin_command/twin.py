"""twin for hermes_cli/skin_cmd.py:skin_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/skin_cmd.py"
    PY_FN = "skin_command"
    C_FN = "hermes_cli_skin_cmd_skin_command"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
