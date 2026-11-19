"""twin for agent/stream_single_writer.py:stream_writer_is_current (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/stream_single_writer.py"
    PY_FN = "stream_writer_is_current"
    C_FN = "agent_stream_single_writer_stream_writer_is_current"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
