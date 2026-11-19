"""twin for tools/registry.py:restore_plugin_override_policy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/registry.py"
    PY_FN = "restore_plugin_override_policy"
    C_FN = "restore_plugin_override_policy"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
