"""twin for hermes_cli/update_restart_recovery.py:_unit_property (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_restart_recovery.py"
    PY_FN = "_unit_property"
    C_FN = "_unit_property"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
