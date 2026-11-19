"""twin for cron/suggestion_catalog.py:classify_items_script_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/suggestion_catalog.py"
    PY_FN = "classify_items_script_path"
    C_FN = "classify_items_script_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
