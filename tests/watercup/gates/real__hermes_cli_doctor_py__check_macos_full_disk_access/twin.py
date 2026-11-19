"""twin for hermes_cli/doctor.py:check_macos_full_disk_access (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/doctor.py"
    PY_FN = "check_macos_full_disk_access"
    C_FN = "check_macos_full_disk_access"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
