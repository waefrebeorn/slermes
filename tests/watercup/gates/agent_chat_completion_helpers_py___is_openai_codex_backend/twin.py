"""twin for agent/chat_completion_helpers.py:_is_openai_codex_backend (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/chat_completion_helpers.py"
    PY_FN = "_is_openai_codex_backend"
    C_FN = "cch_is_openai_codex_backend"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
