"""twin for hermes_cli/gui_uninstall.py:log_warn (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gui_uninstall.py"
    PY_FN = "log_warn"
    C_FN = "hermes_cli_gui_uninstall_log_warn"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
