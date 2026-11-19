"""twin for hermes_cli/update_inventory.py:_restart_mechanism (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_inventory.py"
    PY_FN = "_restart_mechanism"
    C_FN = "update_inventory_restart_mechanism"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
