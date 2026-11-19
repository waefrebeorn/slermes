"""twin for agent/transports/chat_completions.py:_build_gemini_thinking_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/chat_completions.py"
    PY_FN = "_build_gemini_thinking_config"
    C_FN = "_build_gemini_thinking_config"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
