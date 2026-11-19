"""twin for agent/transports/chat_completions.py:_is_gemini_openai_compat_base_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/chat_completions.py"
    PY_FN = "_is_gemini_openai_compat_base_url"
    C_FN = "cct_is_gemini_openai_compat_base_url"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
