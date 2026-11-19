"""twin for hermes_cli/main.py:_reexec_dependency_sync_off_windows_shim (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_reexec_dependency_sync_off_windows_shim"
    C_FN = "mn_reexec_dependency_sync_off_windows_shim"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
