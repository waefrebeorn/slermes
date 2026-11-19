"""twin for hermes_cli/inventory.py:_append_unconfigured_rows (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/inventory.py"
    PY_FN = "_append_unconfigured_rows"
    C_FN = "hermes_cli_inventory_u_append_unconfigured_rows"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
