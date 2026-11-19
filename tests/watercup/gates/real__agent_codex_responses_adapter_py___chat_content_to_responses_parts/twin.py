"""twin for agent/codex_responses_adapter.py:_chat_content_to_responses_parts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/codex_responses_adapter.py"
    PY_FN = "_chat_content_to_responses_parts"
    C_FN = "_chat_content_to_responses_parts"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
