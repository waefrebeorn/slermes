"""twin for gateway/pairing.py:_migrate_split_pairing_dirs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/pairing.py"
    PY_FN = "_migrate_split_pairing_dirs"
    C_FN = "migrate_split_pairing_dirs"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
