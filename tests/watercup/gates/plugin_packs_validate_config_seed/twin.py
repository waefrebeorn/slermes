"""twin for hermes_cli/plugin_packs.py:validate_config_seed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugin_packs.py"
    PY_FN = "validate_config_seed"
    C_FN = "plugin_pack_validate_config_seed"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
