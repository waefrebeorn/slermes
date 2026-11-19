"""twin for hermes_cli/agent_plugins.py:has_enabled_agent_plugin_mcp (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/agent_plugins.py"
    PY_FN = "has_enabled_agent_plugin_mcp"
    C_FN = "ap_has_enabled_agent_plugin_mcp"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
