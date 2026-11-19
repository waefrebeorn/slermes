"""twin for tools/env_probe.py:_plugin_backend_is_remote (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/env_probe.py"
    PY_FN = "_plugin_backend_is_remote"
    C_FN = "_plugin_backend_is_remote"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
