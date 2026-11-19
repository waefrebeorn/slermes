"""twin for agent/models_dev.py:_merge_catalog_entry_with_override (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/models_dev.py"
    PY_FN = "_merge_catalog_entry_with_override"
    C_FN = "_merge_catalog_entry_with_override"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
