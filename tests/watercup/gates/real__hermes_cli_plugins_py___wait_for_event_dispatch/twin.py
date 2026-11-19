"""twin for hermes_cli/plugins.py:_wait_for_event_dispatch (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "_wait_for_event_dispatch"
    C_FN = "_wait_for_event_dispatch"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
