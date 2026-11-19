"""twin for hermes_cli/inventory.py:_raw_config_has_enabled_moa_preset (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/inventory.py"
    PY_FN = "_raw_config_has_enabled_moa_preset"
    C_FN = "hermes_cli_inventory_u_raw_config_has_enabled_moa_preset"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
