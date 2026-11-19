"""twin for hermes_cli/send_cmd.py:_list_targets (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/send_cmd.py"
    PY_FN = "_list_targets"
    C_FN = "hermes_cli_send_cmd_u_list_targets"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
