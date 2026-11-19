"""twin for agent/codex_runtime.py:_consume_codex_event_stream (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/codex_runtime.py"
    PY_FN = "_consume_codex_event_stream"
    C_FN = "_consume_codex_event_stream"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
