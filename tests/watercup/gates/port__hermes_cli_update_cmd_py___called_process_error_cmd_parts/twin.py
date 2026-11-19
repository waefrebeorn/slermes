"""twin for hermes_cli/update_cmd.py:_called_process_error_cmd_parts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_called_process_error_cmd_parts"
    C_FN = "_called_process_error_cmd_parts"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
