"""twin for hermes_cli/windows_ssh_runtime.py:_ensure_directory (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/windows_ssh_runtime.py"
    PY_FN = "_ensure_directory"
    C_FN = "wssr_u_ensure_directory"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
