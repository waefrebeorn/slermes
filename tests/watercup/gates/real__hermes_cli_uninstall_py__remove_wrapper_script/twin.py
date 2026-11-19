"""twin for hermes_cli/uninstall.py:remove_wrapper_script (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/uninstall.py"
    PY_FN = "remove_wrapper_script"
    C_FN = "uninst_remove_wrapper_script"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
