"""twin for gateway/disk_status.py:classify_disk_pressure (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/disk_status.py"
    PY_FN = "classify_disk_pressure"
    C_FN = "classify_disk_pressure"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
