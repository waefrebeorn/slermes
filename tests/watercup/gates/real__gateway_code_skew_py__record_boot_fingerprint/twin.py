"""twin for gateway/code_skew.py:record_boot_fingerprint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/code_skew.py"
    PY_FN = "record_boot_fingerprint"
    C_FN = "record_boot_fingerprint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
