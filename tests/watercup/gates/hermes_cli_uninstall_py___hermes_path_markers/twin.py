"""twin for hermes_cli/uninstall.py:_hermes_path_markers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/uninstall.py"
    PY_FN = "_hermes_path_markers"
    C_FN = "uninst_u_hermes_path_markers"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
