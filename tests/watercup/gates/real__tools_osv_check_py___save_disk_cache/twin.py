"""twin for tools/osv_check.py:_save_disk_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/osv_check.py"
    PY_FN = "_save_disk_cache"
    C_FN = "_save_disk_cache"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
