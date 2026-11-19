"""twin for hermes_cli/_install_repair.py:_quarantine_running_hermes_exe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_install_repair.py"
    PY_FN = "_quarantine_running_hermes_exe"
    C_FN = "_quarantine_running_hermes_exe"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
