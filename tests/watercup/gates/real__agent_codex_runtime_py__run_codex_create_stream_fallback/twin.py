"""twin for agent/codex_runtime.py:run_codex_create_stream_fallback (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/codex_runtime.py"
    PY_FN = "run_codex_create_stream_fallback"
    C_FN = "run_codex_create_stream_fallback"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
