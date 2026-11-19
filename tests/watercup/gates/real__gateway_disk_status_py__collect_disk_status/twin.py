"""twin for gateway/disk_status.py:collect_disk_status (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/disk_status.py"
    PY_FN = "collect_disk_status"
    C_FN = "collect_disk_status"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
