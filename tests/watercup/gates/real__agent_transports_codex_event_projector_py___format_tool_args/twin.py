"""twin for agent/transports/codex_event_projector.py:_format_tool_args (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/codex_event_projector.py"
    PY_FN = "_format_tool_args"
    C_FN = "_format_tool_args"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
