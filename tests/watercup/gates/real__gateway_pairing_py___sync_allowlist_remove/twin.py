"""twin for gateway/pairing.py:_sync_allowlist_remove (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/pairing.py"
    PY_FN = "_sync_allowlist_remove"
    C_FN = "sync_allowlist_remove"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
