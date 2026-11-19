"""twin for agent/transports/codex_event_projector.py:_project_agent_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/codex_event_projector.py"
    PY_FN = "_project_agent_message"
    C_FN = "_project_agent_message"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
