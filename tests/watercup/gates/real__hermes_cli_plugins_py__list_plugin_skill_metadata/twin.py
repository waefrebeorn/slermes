"""twin for hermes_cli/plugins.py:list_plugin_skill_metadata (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "list_plugin_skill_metadata"
    C_FN = "list_plugin_skill_metadata"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
