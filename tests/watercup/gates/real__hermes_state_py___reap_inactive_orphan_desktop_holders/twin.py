"""twin for hermes_state.py:_reap_inactive_orphan_desktop_holders (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_reap_inactive_orphan_desktop_holders"
    C_FN = "_reap_inactive_orphan_desktop_holders"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
