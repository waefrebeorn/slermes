"""twin for agent/web_search_registry.py:snapshot_registration (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/web_search_registry.py"
    PY_FN = "snapshot_registration"
    C_FN = "snapshot_registration"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
