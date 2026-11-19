"""twin for hermes_cli/plugins.py:_evict_stale_persistent_registrations (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "_evict_stale_persistent_registrations"
    C_FN = "_evict_stale_persistent_registrations"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
