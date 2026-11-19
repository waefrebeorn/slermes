"""twin for agent/web_search_registry.py:_disabled_web_plugin_for (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/web_search_registry.py"
    PY_FN = "_disabled_web_plugin_for"
    C_FN = "agent_web_search_registry_u_disabled_web_plugin_for"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
