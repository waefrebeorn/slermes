"""twin for hermes_cli/plugins_cmd.py:ensure_basic_auth_plugin_enabled_in_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins_cmd.py"
    PY_FN = "ensure_basic_auth_plugin_enabled_in_config"
    C_FN = "pcmd_ensure_basic_auth_plugin_enabled_in_config"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
