"""twin for hermes_cli/inventory.py:_apply_capabilities (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/inventory.py"
    PY_FN = "_apply_capabilities"
    C_FN = "hermes_cli_inventory_u_apply_capabilities"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
