"""twin for agent/model_metadata.py:grok_supports_reasoning_effort (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"
    PY_FN = "grok_supports_reasoning_effort"
    C_FN = "grok_supports_reasoning_effort"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
