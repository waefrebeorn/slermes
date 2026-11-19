"""twin for cron/scripts/classify_items.py:_eprint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scripts/classify_items.py"
    PY_FN = "_eprint"
    C_FN = "_eprint"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
