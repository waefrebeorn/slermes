"""twin for hermes_cli/_install_repair.py:ensure_windows_bin_launchers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_install_repair.py"
    PY_FN = "ensure_windows_bin_launchers"
    C_FN = "ensure_windows_bin_launchers"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
