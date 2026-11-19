"""twin for tools/plugin_guard.py:_is_excluded (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/plugin_guard.py"
    PY_FN = "_is_excluded"
    C_FN = "_is_excluded"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
