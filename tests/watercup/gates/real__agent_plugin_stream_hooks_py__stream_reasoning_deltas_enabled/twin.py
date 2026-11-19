"""twin for agent/plugin_stream_hooks.py:stream_reasoning_deltas_enabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/plugin_stream_hooks.py"
    PY_FN = "stream_reasoning_deltas_enabled"
    C_FN = "stream_reasoning_deltas_enabled"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
