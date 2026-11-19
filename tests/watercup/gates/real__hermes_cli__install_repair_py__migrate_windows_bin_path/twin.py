"""twin for hermes_cli/_install_repair.py:migrate_windows_bin_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_install_repair.py"
    PY_FN = "migrate_windows_bin_path"
    C_FN = "migrate_windows_bin_path"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
