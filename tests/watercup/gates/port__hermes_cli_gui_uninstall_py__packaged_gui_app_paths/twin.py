"""twin for hermes_cli/gui_uninstall.py:packaged_gui_app_paths (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gui_uninstall.py"
    PY_FN = "packaged_gui_app_paths"
    C_FN = "hermes_cli_gui_uninstall_packaged_gui_app_paths"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
