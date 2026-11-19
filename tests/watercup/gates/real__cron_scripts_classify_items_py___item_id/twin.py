"""twin for cron/scripts/classify_items.py:_item_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scripts/classify_items.py"
    PY_FN = "_item_id"
    C_FN = "_item_id"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
