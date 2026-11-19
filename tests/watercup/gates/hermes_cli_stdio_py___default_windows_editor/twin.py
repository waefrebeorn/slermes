"""twin for hermes_cli/stdio.py:_default_windows_editor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/stdio.py"
    PY_FN = "_default_windows_editor"
    C_FN = "hermes_cli_stdio_u_default_windows_editor"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
