"""twin for agent/transports/codex_event_projector.py:_project_opaque (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/codex_event_projector.py"
    PY_FN = "_project_opaque"
    C_FN = "_project_opaque"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
