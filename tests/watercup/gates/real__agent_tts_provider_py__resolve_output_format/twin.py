"""twin for agent/tts_provider.py:resolve_output_format (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tts_provider.py"
    PY_FN = "resolve_output_format"
    C_FN = "resolve_output_format"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
