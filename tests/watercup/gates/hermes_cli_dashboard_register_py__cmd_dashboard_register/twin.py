"""twin for hermes_cli/dashboard_register.py:cmd_dashboard_register (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_register.py"
    PY_FN = "cmd_dashboard_register"
    C_FN = "hermes_cli_dashboard_register_cmd_dashboard_register"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
